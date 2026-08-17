#include "feature/pit.hpp"

#include "feature/axis.hpp"
#include "feature/industry.hpp"
#include "misc/affinity.hpp"
#include "misc/date.hpp"
#include "misc/parquet.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

// ============================================================================
// pit.cpp — itf api 单点. 每个 itf 一组 (build + cache_layout [+ post_ffill])
//   集中定义, 末尾 ITFS[] 表挂载. load.cpp 仅迭代该表, 不出现具体 itf 名.
//   增减 itf: 1) pit.hpp 在 PitPool 加字段 (PoolArr<T> / EventStore<Ev>)
//             2) pit.cpp 加一个 namespace itf_<name> (build / cache_layout)
//             3) ITFS[] 末尾追加一行
//
// 流水 (build, miss 路径一次): 月度 parquet → 并行 (per-月) 读列 → 直接写入
//   pool 字段 (网格走 (a, row) cell, 事件走 per-a chain emplace + sort +
//   finalize). 不经任何中间 row 表示. 写完即"row D 已 cutoff 的合法数据",
//   下游 feature 直读 pool[base + d].
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

namespace pq = misc::pq;

constexpr float NaNF = std::numeric_limits<float>::quiet_NaN();
constexpr float InfF = std::numeric_limits<float>::infinity();

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

// yyyymmdd int32 → "YYYYMMDD"; <= 0 → 空串.
inline std::string ymd_str(std::int32_t v) {
  if (v <= 0) return {};
  char buf[9];
  std::snprintf(buf, sizeof(buf), "%08d", v);
  return std::string(buf, 8);
}

// instrument / ts_code → a 索引; 不在 code_idx → -1.
inline int lookup_a(const Axes &axes, std::string_view code) {
  if (code.empty()) return -1;
  auto it = axes.code_idx.find(std::string(code));
  return it == axes.code_idx.end() ? -1 : it->second;
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
// row 定位 (per-file memo, 月内 distinct date 少, 避免逐行 string 化查表):
//   网格: date 必须精确命中 axes 交易日 (非交易日行 skip);
//         row = v_idx - CUTOFF, 越界 skip. distinct vd → distinct row ⇒ 跨月无锁写.
//   事件: floor_date (周末/节假日公告落上一交易日); row 越界 skip.
// ============================================================================
class GridRowMemo {
public:
  GridRowMemo(const Axes &axes, int cutoff) : axes_(axes), cutoff_(cutoff) {}
  int row(std::int32_t ymd) { // -1 = skip
    auto it = memo_.find(ymd);
    if (it != memo_.end()) return it->second;
    int r = -1;
    auto di = axes_.date_idx.find(ymd_str(ymd));
    if (di != axes_.date_idx.end()) {
      int cand = di->second - cutoff_;
      if (cand >= 0 && cand < axes_.n_d()) r = cand;
    }
    memo_.emplace(ymd, r);
    return r;
  }

private:
  const Axes &axes_;
  int cutoff_;
  std::unordered_map<std::int32_t, int> memo_;
};

class EventRowMemo {
public:
  EventRowMemo(const Axes &axes, int cutoff) : axes_(axes), cutoff_(cutoff) {}
  int row(std::int32_t ymd) { // -1 = skip
    auto it = memo_.find(ymd);
    if (it != memo_.end()) return it->second;
    int r = -1;
    int v_idx = axes_.floor_date(ymd_str(ymd));
    if (v_idx >= 0) {
      int cand = v_idx - cutoff_;
      if (cand >= 0 && cand < axes_.n_d()) r = cand;
    }
    memo_.emplace(ymd, r);
    return r;
  }

private:
  const Axes &axes_;
  int cutoff_;
  std::unordered_map<std::int32_t, int> memo_;
};

// ============================================================================
// 并行驱动: per-月 parquet 拿原子 idx, 读表后调用 body(view). body 内决定如何
//   (无锁 / per-a mutex) 写 pool. 0 行月直接 skip.
// ============================================================================
template <class Body>
inline void parallel_parse_months(const std::vector<MonthFile> &files,
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
      pq::TableView v(pq::read_table(files[i].path));
      if (v.rows() == 0) continue;
      body(v);
    }
  };
  std::vector<std::thread> ts;
  ts.reserve(nt);
  for (unsigned t = 0; t < nt; ++t) ts.emplace_back(worker);
  for (auto &t : ts) t.join();
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.bar1d.close, n);
  prealloc_grid_float(p.bar1d.adjust_factor, n);
  std::size_t n_d = static_cast<std::size_t>(axes.n_d());

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col close = v.col("close"), af = v.col("adjust_factor");
    GridRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      std::size_t off = static_cast<std::size_t>(a) * n_d +
                        static_cast<std::size_t>(row);
      p.bar1d.close[off] = positive_or_inf(close.f32(i));
      p.bar1d.adjust_factor[off] = positive_or_inf(af.f32(i));
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.shares.total_shares, n);
  prealloc_grid_float(p.shares.a_float_shares, n);
  std::size_t n_d = static_cast<std::size_t>(axes.n_d());

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col ts = v.col("total_shares"), fs = v.col("a_float_shares");
    GridRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      std::size_t off = static_cast<std::size_t>(a) * n_d +
                        static_cast<std::size_t>(row);
      p.shares.total_shares[off] = positive_or_inf(ts.f32(i));
      p.shares.a_float_shares[off] = positive_or_inf(fs.f32(i));
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_float(p.limit_price.upper_limit, n);
  prealloc_grid_float(p.limit_price.lower_limit, n);
  std::size_t n_d = static_cast<std::size_t>(axes.n_d());

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col up = v.col("upper_limit"), dn = v.col("lower_limit");
    GridRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      std::size_t off = static_cast<std::size_t>(a) * n_d +
                        static_cast<std::size_t>(row);
      // > 0; 0 / 负 / non-finite (含 2015-2017 部分缺口) → +inf 标记 "无限制",
      // ffill 不传播.
      p.limit_price.upper_limit[off] = positive_or_inf(up.f32(i));
      p.limit_price.lower_limit[off] = positive_or_inf(dn.f32(i));
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  // 默认 0 = "正常 / 未停牌"; 缺日不 ffill.
  prealloc_grid_pod<std::int8_t>(p.status.st_status, n, 0);
  prealloc_grid_pod<std::uint8_t>(p.status.suspended, n, 0);
  std::size_t n_d = static_cast<std::size_t>(axes.n_d());

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col st_c = v.col("st_status"), rw_c = v.col("is_risk_warning");
    pq::Col sp_c = v.col("suspended");
    GridRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      std::size_t off = static_cast<std::size_t>(a) * n_d +
                        static_cast<std::size_t>(row);
      int st = st_c.i32(i, 0);
      int rw = rw_c.i32(i, 0);
      int sp = sp_c.i32(i, 0);
      // 4 态派生: st 1/2 优先; 否则 risk_warning=1 → 3 (退市整理期); else 0.
      // 退市整理期: 交易所摘 *ST 后 st 翻 0 但 is_risk_warning 仍 1; 用 3
      // 保留识别力 (实测 *ST大通 2023/06/19 进整理期后 st_status=0/rw=1).
      std::int8_t out_st = (st == 1) ? std::int8_t{1}
                           : (st == 2) ? std::int8_t{2}
                           : (rw != 0) ? std::int8_t{3}
                                       : std::int8_t{0};
      p.status.st_status[off] = out_st;
      p.status.suspended[off] = (sp != 0) ? std::uint8_t{1} : std::uint8_t{0};
    }
  });
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.status.st_status);
  v.section(p.status.suspended);
}

// 不做 ffill — 盘前全量快照, 缺日 = 数据起点前 / 拉取漏日, 一律保持 0 (=正常).

} // namespace itf_cn_stock_status

namespace itf_cn_stock_margin_trading_detail {

// CUTOFF=0 (真盘前入库). 非两融标的日 = 无记录行 → is_margin=0 默认.
constexpr int CUTOFF = 0;

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n = grid_n(axes);
  prealloc_grid_pod<std::uint8_t>(p.margin_detail.is_margin, n, 0);
  prealloc_grid_float(p.margin_detail.financing_balance, n);
  prealloc_grid_float(p.margin_detail.securities_lending_balance, n);
  std::size_t n_d = static_cast<std::size_t>(axes.n_d());

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col fb = v.col("financing_balance");
    pq::Col sb = v.col("securities_lending_balance");
    GridRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      std::size_t off = static_cast<std::size_t>(a) * n_d +
                        static_cast<std::size_t>(row);
      p.margin_detail.is_margin[off] = 1;
      p.margin_detail.financing_balance[off] = non_negative_or_inf(fb.f32(i));
      p.margin_detail.securities_lending_balance[off] =
          non_negative_or_inf(sb.f32(i));
    }
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
inline ForecastType parse_forecast_type(std::string_view s) {
  if (s == "首亏") return ForecastType::FirstLoss;
  if (s == "续亏") return ForecastType::ContinueLoss;
  return ForecastType::Other;
}

} // namespace

namespace itf_cn_stock_industry_component {

// MonthFirst sw2021 一级行业归属快照. 月初首日 industry_l1 自然延续上月 base.
constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.industry_component.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col ind = v.col("industry"), l1 = v.col("industry_level1_name");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      if (ind.str(i) != "sw2021") continue;
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      IndustryComponentEv ev{};
      ev.v = row;
      ev.l1_id = sw2021_l1_name_to_id(l1.str(i));
      std::lock_guard<std::mutex> lk(mu[a]);
      p.industry_component.push_chain(a, ev);
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.industry_change.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col ind = v.col("industry"), lvl = v.col("industry_level");
    pq::Col flag = v.col("change_flag"), name = v.col("industry_name");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      if (ind.str(i) != "sw2021") continue;
      if (lvl.i32(i, 0) != 1) continue;
      if (flag.i32(i, -1) != 1) continue;
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      IndustryChangeEv ev{};
      ev.v = row;
      ev.l1_id = sw2021_l1_name_to_id(name.str(i));
      std::lock_guard<std::mutex> lk(mu[a]);
      p.industry_change.push_chain(a, ev);
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.dividend.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col vd = v.col("publish_date"), inst = v.col("instrument");
    pq::Col rd = v.col("report_date"), ed = v.col("ex_date");
    pq::Col cash = v.col("cash_after_tax");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(vd.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      DividendEv ev;
      ev.v = row;
      ev.report_date = rd.yyyymmdd(i);
      ev.ex_date = ed.yyyymmdd(i);
      ev.cash_after_tax = cash.f32(i);
      std::lock_guard<std::mutex> lk(mu[a]);
      p.dividend.push_chain(a, ev);
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_ttm.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col shift = v.col("shift"), rd = v.col("report_date");
    pq::Col rev = v.col("total_operating_revenue_ttm");
    pq::Col np = v.col("net_profit_to_parent_shareholders_ttm");
    pq::Col cf = v.col("net_cffoa_ttm");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      // 仅入 shift==0 (该 visible_date 最新报告期 TTM).
      if (shift.i32(i, -1) != 0) continue;
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      FinancialTtmEv ev;
      ev.v = row;
      ev.report_date = rd.yyyymmdd(i);
      ev.total_operating_revenue_ttm = rev.f32(i);
      ev.net_profit_to_parent_shareholders_ttm = np.f32(i);
      ev.net_cffoa_ttm = cf.f32(i);
      std::lock_guard<std::mutex> lk(mu[a]);
      p.financial_ttm.push_chain(a, ev);
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_balance.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col rd = v.col("report_date");
    pq::Col toe = v.col("total_owner_equity");
    pq::Col tep = v.col("total_equity_to_parent_shareholders");
    pq::Col ta = v.col("total_assets");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      // 不过滤 fs_quarter_index — 季/半/年报均入; feature 层 max(report_date) MRQ.
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      FinancialBalanceEv ev;
      ev.v = row;
      ev.report_date = rd.yyyymmdd(i);
      ev.total_owner_equity = toe.f32(i);
      ev.total_equity_to_parent_shareholders = tep.f32(i);
      ev.total_assets = ta.f32(i);
      std::lock_guard<std::mutex> lk(mu[a]);
      p.financial_balance.push_chain(a, ev);
    }
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

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.financial_income_annual.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col date = v.col("date"), inst = v.col("instrument");
    pq::Col fqi = v.col("fs_quarter_index"), rd = v.col("report_date");
    pq::Col np = v.col("net_profit_to_parent_shareholders");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      // 仅入 fs_quarter_index==4 (年报) — 给 ni_raw / dividend_st 阈值用.
      if (fqi.i32(i, -1) != 4) continue;
      int row = memo.row(date.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      FinancialIncomeAnnualEv ev;
      ev.v = row;
      ev.report_date = rd.yyyymmdd(i);
      ev.net_profit_to_parent_shareholders = np.f32(i);
      std::lock_guard<std::mutex> lk(mu[a]);
      p.financial_income_annual.push_chain(a, ev);
    }
  });
  p.financial_income_annual.sort_chains();
  p.financial_income_annual.finalize();
}

void cache_layout(PitPool &p, CacheVisitor &v) {
  v.section(p.financial_income_annual);
}

} // namespace itf_cn_stock_financial_income_general_pit

namespace itf_forecast {

// Tushare forecast: 字段用 ts_code; 日期列是 "YYYYMMDD" string (tushare parquet).
constexpr int CUTOFF = -1;

void build(const Axes &axes, const std::vector<MonthFile> &files, PitPool &p) {
  std::size_t n_a = static_cast<std::size_t>(axes.n_a());
  p.forecast.resize_chains(n_a);
  std::vector<std::mutex> mu(n_a);

  parallel_parse_months(files, [&](const pq::TableView &v) {
    pq::Col vd = v.col("ann_date"), inst = v.col("ts_code");
    pq::Col ed = v.col("end_date"), type = v.col("type");
    pq::Col lpn = v.col("last_parent_net");
    EventRowMemo memo(axes, CUTOFF);
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      int row = memo.row(vd.yyyymmdd(i));
      if (row < 0) continue;
      int a = lookup_a(axes, inst.str(i));
      if (a < 0) continue;
      ForecastEv ev;
      ev.v = row;
      ev.end_date = ed.yyyymmdd(i);
      ev.type = parse_forecast_type(type.str(i));
      ev.last_parent_net = lpn.f32(i);
      std::lock_guard<std::mutex> lk(mu[a]);
      p.forecast.push_chain(a, ev);
    }
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
//   读 data/_meta/cn_stock_static_data.parquet (真盘前 09:00 快照), 把
//   suspended / st_status 填充到 row=last_d (实盘当日). 历史天完全不动.
//   _meta 不存在 ⇒ silent noop (纯历史回测场景).
// ============================================================================
void apply_meta_overlays(const Axes &axes, PitPool &pool) {
  namespace fs = std::filesystem;
  fs::path meta_path = pq::meta_path("cn_stock_static_data");
  if (!fs::exists(meta_path)) return;

  int n_d = axes.n_d();
  if (n_d <= 0) return;
  int last_d = n_d - 1;

  pq::TableView v(pq::read_table(meta_path));
  if (v.rows() == 0) return;

  pq::Col date = v.col("date"), inst = v.col("instrument");
  pq::Col st_c = v.col("st_status"), dl_c = v.col("in_delist");
  pq::Col sp_c = v.col("suspended");

  // static_data 盘前更新, trading_days 可能盘后才补 CN 当日; 只校验快照为今天.
  {
    std::string today = misc::today_yyyymmdd();
    std::string snap = ymd_str(date.yyyymmdd(0));
    assert(!snap.empty() && "cn_stock_static_data 缺 date");
    assert(today == snap && "cn_stock_static_data.MAX(date) 不是今天");
  }

  std::size_t n_d_sz = static_cast<std::size_t>(n_d);
  std::size_t base_off = static_cast<std::size_t>(last_d);
  for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
    int a = lookup_a(axes, inst.str(i));
    if (a < 0) continue;
    std::size_t off = static_cast<std::size_t>(a) * n_d_sz + base_off;

    int st = st_c.i32(i, 0);
    int dl = dl_c.i32(i, 0);
    int sp = sp_c.i32(i, 0);
    std::int8_t out_st = (dl != 0) ? std::int8_t{3}
                         : (st == 1) ? std::int8_t{1}
                         : (st == 2) ? std::int8_t{2}
                                     : std::int8_t{0};
    pool.status.st_status[off] = out_st;
    pool.status.suspended[off] = (sp != 0) ? std::uint8_t{1} : std::uint8_t{0};
  }
}

} // namespace feature
