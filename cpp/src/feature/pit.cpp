#include "feature/pit.hpp"

#include "feature/axis.hpp"
#include "feature/industry.hpp"
#include "misc/affinity.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"

#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ============================================================================
// pit.cpp — itf api 单点. 每个 itf 一组 (build + cache_layout [+ post_ffill])
//   集中定义, 末尾 ITFS[] 表挂载. load.cpp 仅迭代该表, 不出现具体 itf 名.
//   增减 itf: 1) pit.hpp 在 PitPool 加字段 (PoolArr<T> / EventStore<Ev>)
//             2) pit.cpp 加一个 namespace itf_<name> (build / cache_layout)
//             3) ITFS[] 末尾追加一行
//
// 流水 (build, miss 路径一次): dayfile JSON → 并行解析 → 直接写入 pool 字段
//   (网格走 (a, row) cell, 事件走 per-a chain emplace + sort + finalize).
//   不经任何中间 row 表示 (旧 AggregateRow / replay 两段已删除). 写完即"row D
//   已 cutoff 的合法数据", 下游 feature 直读 pool[base + d].
//
// 【raw cutoff 单点真理】每个 itf namespace 内 constexpr CUTOFF = 0 / -1:
//   row D 的合法数据 = visible_date <= D + CUTOFF 的最新值
//   ⇒ replay 时 row = v_idx - CUTOFF (v_idx = floor_date(visible_date)):
//     CUTOFF= 0: 当日记录可见 — (a) 真盘前入库 (margin_trading_detail);
//                                (b) hybrid 伪装 (status, 末日 overlay).
//     CUTOFF=-1: 承认滞后 — 绝大多数盘后入库 itf (normal).
// ============================================================================

namespace feature {

namespace {

constexpr float NaNF = std::numeric_limits<float>::quiet_NaN();
constexpr float InfF = std::numeric_limits<float>::infinity();

// ---- yyjson helpers ----
inline float as_float_or_nan(yyjson_val *v) {
  if (!v) return NaNF;
  if (yyjson_is_real(v)) return static_cast<float>(yyjson_get_real(v));
  if (yyjson_is_int(v)) return static_cast<float>(yyjson_get_int(v));
  return NaNF;
}

// NaN (数据缺失) 透传留给 ffill; finite 满足约束 → 原值; finite 违反约束 → +inf
// 保"业务异常"标记 (ffill 不传播 +inf, 不污染下游).
inline float positive_or_inf(float v) {
  if (std::isnan(v)) return v;
  return (std::isfinite(v) && v > 0.0f) ? v : InfF;
}
inline float non_negative_or_inf(float v) {
  if (std::isnan(v)) return v;
  return (std::isfinite(v) && v >= 0.0f) ? v : InfF;
}

inline const char *as_cstr_or_null(yyjson_val *v) {
  if (!v || !yyjson_is_str(v)) return nullptr;
  return yyjson_get_str(v);
}

inline std::string_view as_sv(yyjson_val *v) {
  if (!v || !yyjson_is_str(v)) return {};
  return std::string_view(yyjson_get_str(v), yyjson_get_len(v));
}

inline int as_int_or_default(yyjson_val *v, int def) {
  if (!v) return def;
  if (yyjson_is_int(v)) return static_cast<int>(yyjson_get_int(v));
  return def;
}

inline std::int32_t as_yyyymmdd_int(yyjson_val *v) {
  if (!v || !yyjson_is_str(v)) return 0;
  return misc::to_yyyymmdd_int(
      std::string_view(yyjson_get_str(v), yyjson_get_len(v)));
}

// 按字段名查 a 索引. 字段缺失 / 非 string / 不在 code_idx → -1.
inline int lookup_a(const Axes &axes, yyjson_val *item, const char *field) {
  const char *s = as_cstr_or_null(yyjson_obj_get(item, field));
  if (!s) return -1;
  auto it = axes.code_idx.find(s);
  return it == axes.code_idx.end() ? -1 : it->second;
}

inline bool grid_day_exists(const Axes &axes, const std::string &day) {
  return axes.date_idx.find(day) != axes.date_idx.end();
}

// 网格 itf 通用 prealloc: 各 field allocate n_a*n_d + fill (NaN/0).
inline std::size_t grid_n(const Axes &axes) {
  return static_cast<std::size_t>(axes.n_a()) *
         static_cast<std::size_t>(axes.n_d());
}

inline void prealloc_grid_float(PoolArr<float> &g, std::size_t n) {
  g.allocate(n);
  std::fill(g.begin(), g.end(), NaNF);
}

template <class T>
inline void prealloc_grid_pod(PoolArr<T> &g, std::size_t n, T init = T{}) {
  g.allocate(n);
  std::fill(g.begin(), g.end(), init);
}

// ============================================================================
// 并行驱动: per-file 拿原子 idx, 解析 JSON 后调用 body(file, root). body 内
//   决定如何 (无锁 / per-a mutex) 写 pool.
//
//   网格 itf: body 直接写 pool[a*n_d + row]. 同 file 同 (a, d) cell 互斥 (单线程内);
//             跨 file 同 day 同 a 几乎不会发生 (一天一个 dayfile, day 不同 row 不同).
//             即使发生 (data 异常重复) 也是确定性最后写赢, 无 data race UB.
//   事件 itf: body 持 mu[a] 锁 emplace 到 pool.<itf>.push_chain(a, ev).
// ============================================================================
template <class Body>
inline void parallel_parse_dayfiles(const std::vector<DayFile> &files,
                                    Body body) {
  std::size_t n = files.size();
  if (n == 0) return;
  unsigned nt = misc::Affinity::core_count();
  if (nt == 0) nt = 1;
  if (static_cast<std::size_t>(nt) > n) nt = static_cast<unsigned>(n);

  std::atomic<std::size_t> next{0};
  auto worker = [&]() {
    for (;;) {
      std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= n) break;
      const DayFile &f = files[i];
      std::string buf = misc::read_file_all(f.path);
      if (buf.empty()) continue;
      yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
      assert(doc);
      yyjson_val *root = yyjson_doc_get_root(doc);
      assert(yyjson_is_arr(root));
      body(f, root);
      yyjson_doc_free(doc);
    }
  };
  std::vector<std::thread> ts;
  ts.reserve(nt);
  for (unsigned t = 0; t < nt; ++t) ts.emplace_back(worker);
  for (auto &t : ts) t.join();
}

// 网格 itf body 辅助: 算 row, 然后 for each item 调 write(off, item).
//   不在 axes 的 day (grid_day_exists false) → 整 file skip (语义同旧 replay_grid).
//   floor_date(day) → v_idx; row = v_idx - cutoff; row 越界 → file skip.
template <class Write>
inline void per_file_grid_apply(const Axes &axes, const DayFile &f,
                                yyjson_val *root, int cutoff, Write write) {
  if (!grid_day_exists(axes, f.day)) return;
  int v_idx = axes.floor_date(f.day);
  if (v_idx < 0) return;
  int row = v_idx - cutoff;
  int n_d = axes.n_d();
  if (row < 0 || row >= n_d) return;
  std::size_t base_d = static_cast<std::size_t>(row);
  std::size_t n_d_sz = static_cast<std::size_t>(n_d);

  std::size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0) continue;
    std::size_t off = static_cast<std::size_t>(a) * n_d_sz + base_d;
    write(off, item);
  }
}

// 事件 itf body 辅助: 算 row, for each item → 持 mu[a] 锁 emit.
//   事件不要求 day ∈ axes (允许非 trading day 落到前一 trading day).
//   row >= n_d ⇒ skip (未来日, 未发生).
//   code_field: BigQuant = "instrument", Tushare = "ts_code".
template <class Emit>
inline void per_file_event_apply(const Axes &axes, const DayFile &f,
                                 yyjson_val *root, int cutoff,
                                 std::vector<std::mutex> &mu,
                                 const char *code_field, Emit emit) {
  int v_idx = axes.floor_date(f.day);
  if (v_idx < 0) return;
  int row = v_idx - cutoff;
  if (row >= axes.n_d()) return;

  std::size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    int a = lookup_a(axes, item, code_field);
    if (a < 0) continue;
    std::lock_guard<std::mutex> lk(mu[a]);
    emit(a, row, item);
  }
}

// 网格字段 per-A forward fill:
//   - finite 值记为 last (可用作 fill 源)
//   - NaN (数据缺失) 用 last 填; +inf (数据不合理) 不填, 保留标记
inline void grid_ffill(PoolArr<float> &grid, int n_a, int n_d) {
  for (int a = 0; a < n_a; ++a) {
    std::size_t base = static_cast<std::size_t>(a) *
                       static_cast<std::size_t>(n_d);
    float last = NaNF;
    for (int d = 0; d < n_d; ++d) {
      float v = grid[base + static_cast<std::size_t>(d)];
      if (std::isfinite(v)) {
        last = v;
      } else if (std::isnan(v) && std::isfinite(last)) {
        grid[base + static_cast<std::size_t>(d)] = last;
      }
    }
  }
}

} // anonymous namespace

// ============================================================================
// 网格 itf
// ============================================================================

namespace itf_cn_stock_real_bar1d {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.bar1d.close, n);
  prealloc_grid_float(p.bar1d.adjust_factor, n);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_grid_apply(axes, f, root, CUTOFF, [&](std::size_t off, yyjson_val *it) {
      p.bar1d.close[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "close")));
      p.bar1d.adjust_factor[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "adjust_factor")));
    });
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.bar1d.close);
  v.section(p.bar1d.adjust_factor);
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.bar1d.close, n_a, n_d);
  grid_ffill(p.bar1d.adjust_factor, n_a, n_d);
}

} // namespace itf_cn_stock_real_bar1d

namespace itf_cn_stock_shares {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.shares.total_shares, n);
  prealloc_grid_float(p.shares.a_float_shares, n);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_grid_apply(axes, f, root, CUTOFF, [&](std::size_t off, yyjson_val *it) {
      p.shares.total_shares[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "total_shares")));
      p.shares.a_float_shares[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "a_float_shares")));
    });
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.shares.total_shares);
  v.section(p.shares.a_float_shares);
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.shares.total_shares, n_a, n_d);
  grid_ffill(p.shares.a_float_shares, n_a, n_d);
}

} // namespace itf_cn_stock_shares

namespace itf_cn_stock_limit_price {

// CUTOFF=-1 (normal, 承认滞后). ST 翻转日略不准, 接受不 overlay.
constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.limit_price.upper_limit, n);
  prealloc_grid_float(p.limit_price.lower_limit, n);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_grid_apply(axes, f, root, CUTOFF, [&](std::size_t off, yyjson_val *it) {
      // > 0; 0 / 负 / non-finite (含 2015-2017 部分缺口) → +inf 标记 "无限制",
      // ffill 不传播.
      p.limit_price.upper_limit[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "upper_limit")));
      p.limit_price.lower_limit[off] = positive_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "lower_limit")));
    });
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.limit_price.upper_limit);
  v.section(p.limit_price.lower_limit);
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.limit_price.upper_limit, n_a, n_d);
  grid_ffill(p.limit_price.lower_limit, n_a, n_d);
}

} // namespace itf_cn_stock_limit_price

namespace itf_cn_stock_status {

// CUTOFF=0 (hybrid 伪装); 末日由 apply_meta_overlays 用 static_data 填充.
constexpr int CUTOFF = 0;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  // 默认 0 = "正常 / 未停牌"; 缺日不 ffill.
  prealloc_grid_pod<std::int8_t>(p.status.st_status, n, 0);
  prealloc_grid_pod<std::uint8_t>(p.status.suspended, n, 0);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_grid_apply(axes, f, root, CUTOFF, [&](std::size_t off, yyjson_val *it) {
      int st = as_int_or_default(yyjson_obj_get(it, "st_status"), 0);
      int rw = as_int_or_default(yyjson_obj_get(it, "is_risk_warning"), 0);
      int sp = as_int_or_default(yyjson_obj_get(it, "suspended"), 0);
      // 4 态派生: st 1/2 优先; 否则 risk_warning=1 → 3 (退市整理期); else 0.
      // 退市整理期: 交易所摘 *ST 后 st 翻 0 但 is_risk_warning 仍 1; 用 3
      // 保留识别力 (实测 *ST大通 2023/06/19 进整理期后 st_status=0/rw=1, 旧版漏判).
      std::int8_t out_st = (st == 1) ? std::int8_t{1}
                           : (st == 2) ? std::int8_t{2}
                           : (rw != 0) ? std::int8_t{3}
                                       : std::int8_t{0};
      p.status.st_status[off] = out_st;
      p.status.suspended[off] = (sp != 0) ? std::uint8_t{1} : std::uint8_t{0};
    });
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.status.st_status);
  v.section(p.status.suspended);
}

// 不做 ffill — 盘前全量快照, 缺日 = 数据起点前 / 拉取漏日, 一律保持 0 (=正常).

} // namespace itf_cn_stock_status

namespace itf_cn_stock_margin_trading_detail {

// CUTOFF=0 (真盘前入库). 非两融标的日 = 无 day file 行 → is_margin=0 默认.
constexpr int CUTOFF = 0;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_pod<std::uint8_t>(p.margin_detail.is_margin, n, 0);
  prealloc_grid_float(p.margin_detail.financing_balance, n);
  prealloc_grid_float(p.margin_detail.securities_lending_balance, n);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_grid_apply(axes, f, root, CUTOFF, [&](std::size_t off, yyjson_val *it) {
      p.margin_detail.is_margin[off] = 1;
      p.margin_detail.financing_balance[off] = non_negative_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "financing_balance")));
      p.margin_detail.securities_lending_balance[off] = non_negative_or_inf(
          as_float_or_nan(yyjson_obj_get(it, "securities_lending_balance")));
    });
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.margin_detail.is_margin);
  v.section(p.margin_detail.financing_balance);
  v.section(p.margin_detail.securities_lending_balance);
}

// 不做 ffill — is_margin=0 ∧ balance!=NaN 是语义不一致 (实测 2024 多 2.25% 误标).

} // namespace itf_cn_stock_margin_trading_detail

// ============================================================================
// 事件 itf
// ============================================================================

namespace {

// Tushare forecast type string → enum (业务只关心 首亏/续亏).
inline ForecastType parse_forecast_type(const char *s, std::size_t n) {
  if (!s || n == 0) return ForecastType::Other;
  // UTF-8 hard-coded; "首亏" 6 bytes, "续亏" 6 bytes.
  if (n == 6 && std::memcmp(s, "首亏", 6) == 0) return ForecastType::FirstLoss;
  if (n == 6 && std::memcmp(s, "续亏", 6) == 0) return ForecastType::ContinueLoss;
  return ForecastType::Other;
}

} // namespace

namespace itf_cn_stock_industry_component {

// MonthFirst sw2021 一级行业归属快照. 月初首日 industry_l1 自然延续上月 base.
constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.industry_component.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      const char *ind = as_cstr_or_null(yyjson_obj_get(it, "industry"));
      if (!ind || std::strcmp(ind, "sw2021") != 0) return;
      std::string_view name = as_sv(yyjson_obj_get(it, "industry_level1_name"));
      IndustryComponentEv ev{};
      ev.v = row;
      ev.l1_id = sw2021_l1_name_to_id(name);
      p.industry_component.push_chain(a, ev);
    });
  });
  p.industry_component.sort_chains();
  p.industry_component.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.industry_component);
}

} // namespace itf_cn_stock_industry_component

namespace itf_cn_stock_industry_change {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.industry_change.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      const char *ind = as_cstr_or_null(yyjson_obj_get(it, "industry"));
      if (!ind || std::strcmp(ind, "sw2021") != 0) return;
      if (as_int_or_default(yyjson_obj_get(it, "industry_level"), 0) != 1) return;
      if (as_int_or_default(yyjson_obj_get(it, "change_flag"), -1) != 1) return;
      std::string_view name = as_sv(yyjson_obj_get(it, "industry_name"));
      IndustryChangeEv ev{};
      ev.v = row;
      ev.l1_id = sw2021_l1_name_to_id(name);
      p.industry_change.push_chain(a, ev);
    });
  });
  p.industry_change.sort_chains();
  p.industry_change.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.industry_change);
}

} // namespace itf_cn_stock_industry_change

namespace itf_cn_stock_dividend {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.dividend.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      DividendEv ev;
      ev.v = row;
      ev.report_date = as_yyyymmdd_int(yyjson_obj_get(it, "report_date"));
      ev.ex_date = as_yyyymmdd_int(yyjson_obj_get(it, "ex_date"));
      ev.cash_after_tax = as_float_or_nan(yyjson_obj_get(it, "cash_after_tax"));
      p.dividend.push_chain(a, ev);
    });
  });
  p.dividend.sort_chains();
  p.dividend.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.dividend);
}

} // namespace itf_cn_stock_dividend

namespace itf_cn_stock_financial_ttm_shift {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_ttm.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      // 仅入 shift==0 (该 visible_date 最新报告期 TTM).
      if (as_int_or_default(yyjson_obj_get(it, "shift"), -1) != 0) return;
      FinancialTtmEv ev;
      ev.v = row;
      ev.report_date = as_yyyymmdd_int(yyjson_obj_get(it, "report_date"));
      ev.total_operating_revenue_ttm =
          as_float_or_nan(yyjson_obj_get(it, "total_operating_revenue_ttm"));
      ev.net_profit_to_parent_shareholders_ttm = as_float_or_nan(
          yyjson_obj_get(it, "net_profit_to_parent_shareholders_ttm"));
      ev.net_cffoa_ttm =
          as_float_or_nan(yyjson_obj_get(it, "net_cffoa_ttm"));
      p.financial_ttm.push_chain(a, ev);
    });
  });
  p.financial_ttm.sort_chains();
  p.financial_ttm.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.financial_ttm);
}

} // namespace itf_cn_stock_financial_ttm_shift

namespace itf_cn_stock_financial_balance_general_pit {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_balance.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      // 不过滤 fs_quarter_index — 季/半/年报均入; feature 层 max(report_date) MRQ.
      FinancialBalanceEv ev;
      ev.v = row;
      ev.report_date = as_yyyymmdd_int(yyjson_obj_get(it, "report_date"));
      ev.total_owner_equity =
          as_float_or_nan(yyjson_obj_get(it, "total_owner_equity"));
      ev.total_equity_to_parent_shareholders = as_float_or_nan(
          yyjson_obj_get(it, "total_equity_to_parent_shareholders"));
      ev.total_assets = as_float_or_nan(yyjson_obj_get(it, "total_assets"));
      p.financial_balance.push_chain(a, ev);
    });
  });
  p.financial_balance.sort_chains();
  p.financial_balance.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.financial_balance);
}

} // namespace itf_cn_stock_financial_balance_general_pit

namespace itf_cn_stock_financial_income_general_pit {

constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_income_annual.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "instrument",
                         [&](int a, int row, yyjson_val *it) {
      // 仅入 fs_quarter_index==4 (年报) — 给 ni_raw / dividend_st 阈值用.
      if (as_int_or_default(yyjson_obj_get(it, "fs_quarter_index"), -1) != 4)
        return;
      FinancialIncomeAnnualEv ev;
      ev.v = row;
      ev.report_date = as_yyyymmdd_int(yyjson_obj_get(it, "report_date"));
      ev.net_profit_to_parent_shareholders = as_float_or_nan(
          yyjson_obj_get(it, "net_profit_to_parent_shareholders"));
      p.financial_income_annual.push_chain(a, ev);
    });
  });
  p.financial_income_annual.sort_chains();
  p.financial_income_annual.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.financial_income_annual);
}

} // namespace itf_cn_stock_financial_income_general_pit

namespace itf_forecast {

// Tushare forecast: 字段用 ts_code; type 字符串一次性 map 到 enum.
constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<DayFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.forecast.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_dayfiles(files, [&](const DayFile &f, yyjson_val *root) {
    per_file_event_apply(axes, f, root, CUTOFF, mu, "ts_code",
                         [&](int a, int row, yyjson_val *it) {
      ForecastEv ev;
      ev.v = row;
      ev.end_date = as_yyyymmdd_int(yyjson_obj_get(it, "end_date"));
      yyjson_val *tv = yyjson_obj_get(it, "type");
      const char *ts = as_cstr_or_null(tv);
      std::size_t tl = ts ? yyjson_get_len(tv) : 0;
      ev.type = parse_forecast_type(ts, tl);
      ev.last_parent_net =
          as_float_or_nan(yyjson_obj_get(it, "last_parent_net"));
      p.forecast.push_chain(a, ev);
    });
  });
  p.forecast.sort_chains();
  p.forecast.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.forecast);
}

} // namespace itf_forecast

// ============================================================================
// ITFS[] — 单点真理
//   增减 itf 在此追加 / 删除一行 + 上方 itf_<name> 块.
// ============================================================================
const ItfDesc ITFS[] = {
    {"cn_stock_real_bar1d",
     &itf_cn_stock_real_bar1d::build,
     &itf_cn_stock_real_bar1d::cache_layout,
     &itf_cn_stock_real_bar1d::post_ffill},
    {"cn_stock_shares",
     &itf_cn_stock_shares::build,
     &itf_cn_stock_shares::cache_layout,
     &itf_cn_stock_shares::post_ffill},
    {"cn_stock_limit_price",
     &itf_cn_stock_limit_price::build,
     &itf_cn_stock_limit_price::cache_layout,
     &itf_cn_stock_limit_price::post_ffill},
    {"cn_stock_status",
     &itf_cn_stock_status::build,
     &itf_cn_stock_status::cache_layout,
     nullptr},
    {"cn_stock_margin_trading_detail",
     &itf_cn_stock_margin_trading_detail::build,
     &itf_cn_stock_margin_trading_detail::cache_layout,
     nullptr},
    {"cn_stock_industry_component",
     &itf_cn_stock_industry_component::build,
     &itf_cn_stock_industry_component::cache_layout,
     nullptr},
    {"cn_stock_industry_change",
     &itf_cn_stock_industry_change::build,
     &itf_cn_stock_industry_change::cache_layout,
     nullptr},
    {"cn_stock_dividend",
     &itf_cn_stock_dividend::build,
     &itf_cn_stock_dividend::cache_layout,
     nullptr},
    {"cn_stock_financial_ttm_shift",
     &itf_cn_stock_financial_ttm_shift::build,
     &itf_cn_stock_financial_ttm_shift::cache_layout,
     nullptr},
    {"cn_stock_financial_balance_general_pit",
     &itf_cn_stock_financial_balance_general_pit::build,
     &itf_cn_stock_financial_balance_general_pit::cache_layout,
     nullptr},
    {"cn_stock_financial_income_general_pit",
     &itf_cn_stock_financial_income_general_pit::build,
     &itf_cn_stock_financial_income_general_pit::cache_layout,
     nullptr},
    {"forecast",
     &itf_forecast::build,
     &itf_forecast::cache_layout,
     nullptr},
};

const int ITFS_COUNT = static_cast<int>(sizeof(ITFS) / sizeof(ITFS[0]));

// ============================================================================
// apply_meta_overlays — hybrid PIT 收尾.
// ============================================================================
void apply_meta_overlays(const Axes &axes, PitPool &pool) {
  namespace fs = std::filesystem;
  fs::path meta_path =
      misc::git_root() / "data" / "_meta" / "cn_stock_static_data.json";
  if (!fs::exists(meta_path)) return;

  std::string buf = misc::read_file_all(meta_path);
  if (buf.empty()) return;

  int n_d = axes.n_d();
  if (n_d <= 0) return;
  int last_d = n_d - 1;
  std::size_t base_off = static_cast<std::size_t>(last_d);

  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_arr(root));

  // static_data 盘前更新, trading_days 可能盘后才补 CN 当日; 只校验快照为今天.
  yyjson_val *first = yyjson_arr_get_first(root);
  if (first) {
    const char *snap_d = as_cstr_or_null(yyjson_obj_get(first, "date"));
    assert(snap_d && "cn_stock_static_data row 缺 date 字段");
    const std::string today = misc::today_yyyymmdd();
    assert(today == snap_d && "cn_stock_static_data.MAX(date) 不是今天");
  }

  std::size_t n_d_sz = static_cast<std::size_t>(n_d);
  std::size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0) continue;
    std::size_t off = static_cast<std::size_t>(a) * n_d_sz + base_off;

    int st = as_int_or_default(yyjson_obj_get(item, "st_status"), 0);
    int dl = as_int_or_default(yyjson_obj_get(item, "in_delist"), 0);
    int sp = as_int_or_default(yyjson_obj_get(item, "suspended"), 0);
    std::int8_t out_st = (dl != 0) ? std::int8_t{3}
                         : (st == 1) ? std::int8_t{1}
                         : (st == 2) ? std::int8_t{2}
                                     : std::int8_t{0};
    pool.status.st_status[off] = out_st;
    pool.status.suspended[off] = (sp != 0) ? std::uint8_t{1} : std::uint8_t{0};
  }

  yyjson_doc_free(doc);
}

} // namespace feature
