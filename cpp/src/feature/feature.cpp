#include "feature/feature.hpp"

#include "feature/axis.hpp"
#include "feature/cs.hpp"
#include "feature/industry.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"
#include "feature/ts.hpp"

#include "config.hpp"
#include "misc/date.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ============================================================================
// feature.cpp 是「特征字段表单点」: 每个 feature 一个 compute fn (TsComputeFn /
//   CsComputeFn 签名), 末尾 FEATURES[] 表挂载. ts.cpp / cs.cpp / build.cpp
//   仅看 FEATURES[] 迭代调度, 不出现具体 feature 名.
//
//   增减 feature:
//     1) feature.hpp 在 F 枚举对应位置加一行 (位置即计算顺序)
//     2) feature.cpp 新增 ts_<name> / cs_<name> compute fn
//     3) FEATURES[] 末尾对应位置加一行挂载
//
//   compute fn 内部:
//     - TS 全部签名 (int a, axes, pool, meta, T); 不需要的子集忽略.
//     - CS 全部签名 (int d, axes, T, bufs); bufs.a/b/c 是 thread-local n_a-长 buffer.
//     - 写自己的 ts_row(F::self, a) 或 scatter_cs_row(F::self, d, ...);
//     - 可读 PitPool / StockMeta / 已写就的 T.ts_row(prior_f, a) (顺序由 enum 保证).
// ============================================================================

namespace feature {

// ============================================================================
// 局部 helper (跨多个 state machine 共用)
// ============================================================================

namespace {

// pool_b 板块/行业白名单查表 (集中定义见 config.hpp).
//   未覆盖 (空串) 一律不命中 → pool_b 全期 0.
template <std::size_t N>
inline bool in_whitelist(std::string_view v,
                         const std::array<std::string_view, N> &wl) {
  if (v.empty())
    return false;
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

// list_sector 整数白名单查表 (config::POOL_LIST_SECTOR_WHITELIST 是 int8 集合).
template <std::size_t N>
inline bool in_int_whitelist(int8_t v, const std::array<int8_t, N> &wl) {
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

// industry_l1 ID 白名单 mask: 把 config::POOL_INDUSTRY_L1_WHITELIST (中文 string_view)
//   转 array<bool, 32> mask, mask[id]=true 表该 SW2021 一级行业 ID 在白名单内.
//   首次调用 lazy 构建, 后续只读. 失败的中文名 (拼写不在 SW2021_L1_NAMES 内) → id=0
//   被忽略 (不会触发 mask[0], 因为 industry_l1=0 = "未知" 永远不该入白名单).
inline const std::array<bool, SW2021_L1_COUNT> &industry_l1_whitelist_mask() {
  static const auto mask = []() {
    std::array<bool, SW2021_L1_COUNT> m{};
    for (auto name : ::config::POOL_INDUSTRY_L1_WHITELIST) {
      uint8_t id = sw2021_l1_name_to_id(name);
      if (id != 0)
        m[id] = true;
    }
    return m;
  }();
  return mask;
}

// 计算股票 a 的上市日索引 (在 axes.dates 中的 lower_bound)
// 返回 -1 表示无有效 list_date (视为永远未上市)
inline int get_list_d(int a, const Axes &axes, const StockMeta &meta) {
  const std::string &ld = meta.list_date[a];
  if (ld.size() != 8)
    return axes.n_d(); // 无 list_date → 永远未上市
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), ld);
  return static_cast<int>(std::distance(axes.dates.begin(), it));
}

// 不再填 0，保留 NaN 让后续 stage 判断数据可用性
inline void fill_before_list(std::span<float>, int, const Axes &,
                             const StockMeta &) {
}

// 计算股票 a 的退市日索引 (在 axes.dates 中的 lower_bound)
// 返回 n_d 表示无有效 delist_date (视为永不退市)
inline int get_delist_d(int a, const Axes &axes, const StockMeta &meta) {
  const std::string &dd = meta.delist_date[a];
  if (dd.size() != 8)
    return axes.n_d(); // 无 delist_date → 永不退市
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), dd);
  return static_cast<int>(std::distance(axes.dates.begin(), it));
}

// 不再填 0，保留 NaN 让后续 stage 判断数据可用性
inline void fill_after_delist(std::span<float>, int, const Axes &,
                              const StockMeta &) {
}

// forecast 触发 → 终止 d:
//   off = min(report.actual_date 同 end_date 的最早 r.v, 下一年 4 月 30 日 ceil)
//   注: r.v 已在 parse 时应用 cutoff = 首次可见 row D.
int find_forecast_off_d(const ForecastEv &fe,
                        const std::vector<ReportEv> &reports,
                        const Axes &axes) {
  int n_d = axes.n_d();
  int report_d = -1;
  for (const auto &r : reports) {
    if (r.end_date == fe.end_date) {
      report_d = r.v;
      break;
    }
  }
  int Y = year_of(fe.end_date);
  int deadline_d = -1;
  if (Y > 0) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d0430", Y + 1);
    auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(),
                               std::string(buf));
    deadline_d = (it == axes.dates.end())
                     ? n_d
                     : static_cast<int>(std::distance(axes.dates.begin(), it));
  }
  int off = n_d;
  if (report_d >= 0)
    off = std::min(off, report_d);
  if (deadline_d >= 0)
    off = std::min(off, deadline_d);
  return off;
}

} // namespace

// ============================================================================
// TS: raw 网格 — PitPool dense 直读 (row D 已 cutoff). 形状统一 [n_a, n_d].
//   每个 raw 网格 feature = 一行 helper. 对仗: (src grid 字段) → (out span) [× scale].
// ============================================================================

namespace impl {

namespace {

// 模板: 网格 float 字段 → ts_row, 可选 scale (× scale 单位转换); NaN 透传保留数据问题.
template <class GetField>
inline void grid_copy(int a, const Axes &axes, const StockMeta &meta, Tensor &T,
                      F dst, GetField get_field, float scale = 1.0f) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const std::vector<float> &src = get_field();
  auto out = T.ts_row(dst, a);
  if (scale == 1.0f) {
    for (int d = 0; d < n_d; ++d) {
      out[d] = src[base + static_cast<std::size_t>(d)];
    }
  } else {
    for (int d = 0; d < n_d; ++d) {
      float v = src[base + static_cast<std::size_t>(d)];
      out[d] = is_finite(v) ? v * scale : v;
    }
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// 模板: 网格 uint8_t bool 字段 → ts_row (1.0 / 0.0); 不调 fill_* (bool 0 = "无", 与 NaN 不同).
template <class GetField>
inline void grid_copy_bool(int a, const Axes &axes, Tensor &T, F dst,
                           GetField get_field) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const std::vector<uint8_t> &src = get_field();
  auto out = T.ts_row(dst, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d)] ? 1.0f : 0.0f;
  }
}

} // namespace

// close_raw ← cn_stock_real_bar1d.close (不复权 [元/股]).
//   row D = D-1 实际收盘 (CUTOFF=-1). 不复权口径: close 是当日真实成交价,
//   PIT-immutable (历史不随任何除权动作改写), 与 limit_price / total_shares
//   同口径. daily_return 直接基于该 close 链式, 除权日含分红/送股的真实跳跃.
void ts_close_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::close_raw,
            [&]() -> const std::vector<float> & { return pool.bar1d.close; });
}

// mcap_raw = close_raw[d] × shares.total_shares[a, d]  ([元])
//   close_raw 即不复权 close (← cn_stock_real_bar1d.close), 与 total_shares 同口径,
//   mcap_raw = 真实总市值 [元]; cross-section 排序与 low_mc 阈值判定均正确.
void ts_mcap_raw(int a, const Axes &axes, const PitPool &pool,
                 const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::mcap_raw, a);
  const auto &sh = pool.shares.total_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// fmcap_raw = close_raw[d] × shares.total_float_shares[a, d]  ([元])
void ts_fmcap_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::fmcap_raw, a);
  const auto &sh = pool.shares.total_float_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// share_raw ← cn_stock_shares.total_shares ([股]; 直读, 无单位换算)
void ts_share_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::share_raw,
            [&]() -> const std::vector<float> & { return pool.shares.total_shares; });
}

// 财务相关 raw (pb / ps / dy): 数据源 BigQuant cn_stock_financial_* 暂未迁移
//   到 PitPool, 这里先输出全 NaN — 下游 cs_pb_ttm3 / cs_ps_ttm12 / cs_dy_ttm12 的
//   factor_pipeline 会跳 NaN, 等价于 disable 该 factor (横截面无可用样本 → 全 NaN).
void ts_pb_raw(int a, const Axes &axes, const PitPool &,
               const StockMeta &, Tensor &T) {
  auto out = T.ts_row(F::pb_raw, a);
  std::fill(out.begin(), out.end(), std::nanf(""));
  (void)axes;
}

void ts_ps_raw(int a, const Axes &axes, const PitPool &,
               const StockMeta &, Tensor &T) {
  auto out = T.ts_row(F::ps_raw, a);
  std::fill(out.begin(), out.end(), std::nanf(""));
  (void)axes;
}

void ts_dy_raw(int a, const Axes &axes, const PitPool &,
               const StockMeta &, Tensor &T) {
  auto out = T.ts_row(F::dy_raw, a);
  std::fill(out.begin(), out.end(), std::nanf(""));
  (void)axes;
}

// up_lim / dn_lim 主动 -1: limit_price (CUTOFF=0) 取的是 row D 当日盘前公布
//   = D 日适用涨跌停 (基于 D-1 close × 1.1/1.2 来的); close_raw[D] (CUTOFF=-1)
//   = D-1 日实际收盘. 想判 "D-1 日是否封板" 应用 D-1 适用涨跌停, 即 limit_price[D-1]
//   = pool.limit_price.upper_limit[a, d-1]. ts_up_lim 主动 -1 完成此对齐.
//   d=0 处无前一交易日 → NaN.
void ts_up_lim(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(F::up_lim, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = pool.limit_price.upper_limit[base + static_cast<std::size_t>(d - 1)];
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

void ts_dn_lim(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(F::dn_lim, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = pool.limit_price.lower_limit[base + static_cast<std::size_t>(d - 1)];
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// susp ← cn_stock_status.suspended (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充)
void ts_susp(int a, const Axes &axes, const PitPool &pool, const StockMeta &,
             Tensor &T) {
  grid_copy_bool(a, axes, T, F::susp,
                 [&]() -> const std::vector<uint8_t> & { return pool.status.suspended; });
}

// is_margin ← margin_detail (D, A) 存在性 (CUTOFF=0)
void ts_is_margin(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  grid_copy_bool(a, axes, T, F::is_margin,
                 [&]() -> const std::vector<uint8_t> & { return pool.margin_detail.is_margin; });
}

void ts_mr_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::mr_bal_raw,
            [&]() -> const std::vector<float> & { return pool.margin_detail.financing_balance; });
}

void ts_ms_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::ms_bal_raw,
            [&]() -> const std::vector<float> & { return pool.margin_detail.securities_lending_balance; });
}

// industry_l1: SW2021 一级行业 ID per (D, A), 0=未知, 1..31 见 industry.hpp.
//   合并 industry_component (月初快照) + industry_change (月内 change_flag=1 进入)
//   两个事件流, per-A 按 v 升序回放, last_l1_id 写每行. 上市前/无事件期保持 0.
//   退市后 last_l1_id 残留 (pool_b 已用 ¬is_finite(delist_age) 兜底排除, 不影响下游).
void ts_industry_l1(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::industry_l1, a);
  std::fill(out.begin(), out.end(), 0.0f);

  const auto &comp = pool.industry_component[a];
  const auto &chg = pool.industry_change[a];
  std::size_t ic = 0, ig = 0;
  uint8_t last_id = 0;

  for (int d = 0; d < n_d; ++d) {
    while (ic < comp.size() && comp[ic].v <= d) {
      last_id = comp[ic].l1_id;
      ++ic;
    }
    while (ig < chg.size() && chg[ig].v <= d) {
      last_id = chg[ig].l1_id;
      ++ig;
    }
    out[d] = static_cast<float>(last_id);
  }
}

// ============================================================================
// TS: raw 自算 — ttm12 拼接 (依赖 mcap_raw 已就绪 → enum 顺序保证)
// ============================================================================

void ts_rev_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  auto out = T.ts_row(F::rev_raw, a);
  ttm12_compute(
      pool.income[a], axes.n_d(),
      [](const IncomeEv &e) -> const std::string & { return e.report_type; },
      [](const IncomeEv &e) { return e.revenue; },
      out);
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// ni_raw: 仅 income.end_date.M==12 ∧ report_type=='1'; 同 end_date 后到覆盖前;
//   降级: 有 2+ 条年报取最新 2 条均值; 只有 1 条则用 1 条; 0 条才 NaN.
void ts_ni_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::ni_raw, a);
  std::fill(out.begin(), out.end(), std::nanf(""));

  struct Cell {
    float val;
    int last_v;
  };
  std::vector<std::pair<std::string, Cell>> annuals; // 用 vector 维护 (small N, linear scan 即可)
  std::size_t ev_ptr = 0;
  const auto &events = pool.income[a];

  auto annuals_find = [&](const std::string &k) -> int {
    for (std::size_t i = 0; i < annuals.size(); ++i)
      if (annuals[i].first == k)
        return static_cast<int>(i);
    return -1;
  };

  for (int d = 0; d < n_d; ++d) {
    while (ev_ptr < events.size() && events[ev_ptr].v <= d) {
      const IncomeEv &e = events[ev_ptr++];
      if (e.report_type != "1")
        continue;
      if (e.end_date.size() < 6 || month_of(e.end_date) != 12)
        continue;
      if (!is_finite(e.n_income_attr_p))
        continue;
      int idx = annuals_find(e.end_date);
      if (idx < 0)
        annuals.emplace_back(e.end_date, Cell{e.n_income_attr_p, e.v});
      else
        annuals[idx].second = Cell{e.n_income_attr_p, e.v};
    }
    if (annuals.empty())
      continue;

    // 找 last_v 最大 2 条 (或 1 条)
    int i0 = -1, i1 = -1;
    int v0 = -1, v1 = -1;
    for (std::size_t i = 0; i < annuals.size(); ++i) {
      int v = annuals[i].second.last_v;
      if (v > v0) {
        v1 = v0;
        i1 = i0;
        v0 = v;
        i0 = static_cast<int>(i);
      } else if (v > v1) {
        v1 = v;
        i1 = static_cast<int>(i);
      }
    }
    // 降级: 有 2 条取均值, 只有 1 条用 1 条
    if (i0 >= 0 && i1 >= 0) {
      out[d] = (annuals[i0].second.val + annuals[i1].second.val) * 0.5f;
    } else if (i0 >= 0) {
      out[d] = annuals[i0].second.val;
    }
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// pe_raw: 自己算 mcap_raw / ttm12(n_income_attr_p)，支持负 PE（亏损）
void ts_pe_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::vector<float> ni_ttm(n_d, std::nanf(""));
  ttm12_compute(
      pool.income[a], n_d,
      [](const IncomeEv &e) -> const std::string & { return e.report_type; },
      [](const IncomeEv &e) { return e.n_income_attr_p; },
      std::span<float>(ni_ttm.data(), ni_ttm.size()));

  auto mcap = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::pe_raw, a);
  for (int d = 0; d < n_d; ++d) {
    float m = mcap[d];
    float n = ni_ttm[d];
    out[d] = (is_finite(m) && is_finite(n) && n != 0.0f) ? m / n : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

void ts_pcf_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::vector<float> denom(n_d, std::nanf(""));
  ttm12_compute(
      pool.cashflow[a], n_d,
      [](const CashflowEv &e) -> const std::string & { return e.report_type; },
      [](const CashflowEv &e) { return e.n_cashflow_act; },
      std::span<float>(denom.data(), denom.size()));

  auto mcap = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::pcf_raw, a);
  for (int d = 0; d < n_d; ++d) {
    float m = mcap[d];
    float c = denom[d];
    out[d] = (is_finite(m) && m != 0.0f && is_finite(c) && c != 0.0f)
                 ? m / c
                 : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

void ts_roe_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  static const std::string kEmpty;
  auto out = T.ts_row(F::roe_raw, a);
  ttm12_compute(
      pool.fina_indicator[a], axes.n_d(),
      [&](const FinaIndEv &) -> const std::string & { return kEmpty; },
      [](const FinaIndEv &e) { return e.roe; },
      out);
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

void ts_roa_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  static const std::string kEmpty;
  auto out = T.ts_row(F::roa_raw, a);
  ttm12_compute(
      pool.fina_indicator[a], axes.n_d(),
      [&](const FinaIndEv &) -> const std::string & { return kEmpty; },
      [](const FinaIndEv &e) { return e.roa; },
      out);
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// ============================================================================
// TS: raw meta 派生 (per-A 动态: D - list_date / D - delist_date)
// ============================================================================

// list_age: D - list_date if D ≥ list_date else NaN.
//   PIT: 不写"距上市天数" (未来信息). 下游用 is_finite 判 "已上市".
void ts_list_age(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
                 Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::list_age, a);
  if (meta.list_date[a].size() == 8) {
    auto ld = misc::parse_yyyymmdd(meta.list_date[a]);
    for (int d = 0; d < n_d; ++d) {
      float age = static_cast<float>((axes.date_days[d] - ld).count());
      out[d] = (age >= 0.0f) ? age : std::nanf("");
    }
  } else {
    std::fill(out.begin(), out.end(), std::nanf(""));
  }
}

// delist_age: D - delist_date if D ≥ delist_date else NaN.
//   PIT: 不写"距退市天数" (未来信息). 下游用 is_finite 判 "已退市".
void ts_delist_age(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
                   Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::delist_age, a);
  if (meta.delist_date[a].size() == 8) {
    auto dd = misc::parse_yyyymmdd(meta.delist_date[a]);
    for (int d = 0; d < n_d; ++d) {
      float age = static_cast<float>((axes.date_days[d] - dd).count());
      out[d] = (age >= 0.0f) ? age : std::nanf("");
    }
  } else {
    std::fill(out.begin(), out.end(), std::nanf(""));
  }
}

// ============================================================================
// TS: derived — 由 raw 推 (TS 内依赖)
// ============================================================================

// daily_return: 后复权 close 链式日收益 (含分红再投入的真持有收益).
//   tensor 顶层 close_raw 是不复权真价 (mcap / limit / low_p 等需要真值);
//   "复权"是 daily_return 自身需要的内部细节 — 这里直接从 PitPool 读
//   {close, adjust_factor} 在内部叠出 close_hfq[d] = close[d] × adjust_factor[d],
//   再链式: ret[d] = close_hfq[d] / close_hfq[d-1] - 1.
//   除权日 close 真跳, adjust_factor 反向跳, close × af 平滑 ⇒ ret 仅含日内真实涨跌
//   (无除权日 -5%~-10% 假负跳; 与"持有不减仓"账户的真实收益对齐).
//   前复权不 causal (起点会随未来除权事件追溯调整, 历史回测会泄漏未来), 不采用.
//   d==0 或前一日 close/af 非 finite/0 → NaN.
//   下游 benchmark = pool 内等权 mean.
void ts_daily_return(int a, const Axes &axes, const PitPool &pool,
                     const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const auto &cl = pool.bar1d.close;
  const auto &af = pool.bar1d.adjust_factor;
  auto out = T.ts_row(F::daily_return, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    std::size_t i0 = base + static_cast<std::size_t>(d - 1);
    std::size_t i1 = base + static_cast<std::size_t>(d);
    float c0 = cl[i0], c1 = cl[i1];
    float a0 = af[i0], a1 = af[i1];
    if (is_finite(c0) && is_finite(c1) && is_finite(a0) && is_finite(a1) &&
        c0 != 0.0f && a0 != 0.0f) {
      float h0 = c0 * a0;
      float h1 = c1 * a1;
      out[d] = (h0 != 0.0f) ? (h1 / h0 - 1.0f) : std::nanf("");
    } else {
      out[d] = std::nanf("");
    }
  }
}

void ts_low_p(int a, const Axes &axes, const PitPool &, const StockMeta &,
              Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::low_p, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(cl[d]) && cl[d] < 1.0f) ? 1.0f : 0.0f;
  }
}

// low_mc: 主板阈值 5e8, 非主板 3e8. mb 判定 = meta.list_sector[a]==1 (asset 静态).
//   list_sector 编码: 1=主板, 2=创业板, 3=科创板, 4=北交所, 0=未知.
void ts_low_mc(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto mc = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::low_mc, a);
  float thr = (meta.list_sector[a] == 1) ? 5e8f : 3e8f;
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(mc[d]) && mc[d] < thr) ? 1.0f : 0.0f;
  }
}

void ts_limit_up(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto up = T.ts_row(F::up_lim, a);
  auto out = T.ts_row(F::limit_up, a);
  for (int d = 0; d < n_d; ++d) {
    // up >= 100000 是无涨停限制哨兵，排除
    out[d] = (is_finite(cl[d]) && is_finite(up[d]) &&
              up[d] < 100000.0f && cl[d] >= up[d] - 1e-4f)
                 ? 1.0f
                 : 0.0f;
  }
}

void ts_limit_dn(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto dn = T.ts_row(F::dn_lim, a);
  auto out = T.ts_row(F::limit_dn, a);
  for (int d = 0; d < n_d; ++d) {
    // dn <= 0.01 是无跌停限制哨兵，排除
    out[d] = (is_finite(cl[d]) && is_finite(dn[d]) &&
              dn[d] > 0.01f && cl[d] <= dn[d] + 1e-4f)
                 ? 1.0f
                 : 0.0f;
  }
}

// ============================================================================
// TS: state machines
// ============================================================================

void ts_profit_st(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  std::vector<ForecastEv> trig;
  trig.reserve(pool.forecast[a].size());
  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != "首亏" && e.type != "续亏")
      continue;
    if (!is_finite(e.last_parent_net) || e.last_parent_net >= 0.0f)
      continue;
    trig.push_back(e);
  }
  state_machine_intervals(
      trig, axes.n_d(),
      [&](const ForecastEv &fe) {
        return find_forecast_off_d(fe, pool.report[a], axes);
      },
      T.ts_row(F::profit_st, a));
}

void ts_revenue_st(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::revenue_st, a);
  auto rev_raw = T.ts_row(F::rev_raw, a);
  std::fill(out.begin(), out.end(), 0.0f);

  bool mb_a = (meta.list_sector[a] == 1); // 仅主板适用 (asset 静态, 1=主板)
  if (!mb_a)
    return;

  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != "首亏" && e.type != "续亏")
      continue;
    int end_y = year_of(e.end_date);
    if (end_y < 2021)
      continue;
    if (e.v < 1 || e.v >= n_d)
      continue;
    // 业务判 forecast.ann_date >= 20210101: e.v 是 row D, e.v-1 是 visible_d
    if (axes.dates[e.v - 1] < "20210101")
      continue;

    int on_d = e.v;
    int off_d = find_forecast_off_d(e, pool.report[a], axes);
    if (on_d < 0)
      on_d = 0;
    if (off_d > n_d)
      off_d = n_d;
    float thr = (end_y >= 2024) ? 3e8f : 1e8f;
    for (int d = on_d; d < off_d; ++d) {
      if (is_finite(rev_raw[d]) && rev_raw[d] < thr) {
        out[d] = 1.0f;
      }
    }
  }
}

// dividend_st: 阶梯 forward fill — 每 dividend event 重算 3y_sum,
//   3y_sum = Σ over 历史 events with report_date.Y in [ann_y-3, ann_y-1]
//            的 cash_after_tax × share_raw[event.v]; 区间 [e.v, next.v) 填.
//   注: e.v 已是首次可见 row D; ann_y 用 axes.dates[e.v - 1] 还原 visible 日期年份.
//
// 暖机期 (warmup_d 之前) 一律不命中, 避免 3y 回望不完整时偏严:
//   1) 数据轴边界: axes 起点前事件不可见 → 起点后 3 年内 3y 窗口必然不完整
//   2) 股票自身: 上市后 3 年内分红记录天然少, 不视为 ST
//   warmup_d = max(axes_warmup_d, stock_warmup_d), 取较晚者.
void ts_dividend_st(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::dividend_st, a);
  auto share_raw = T.ts_row(F::share_raw, a);
  auto ni_raw = T.ts_row(F::ni_raw, a);
  std::fill(out.begin(), out.end(), 0.0f);

  bool mb_a = (meta.list_sector[a] == 1); // 仅主板适用 (asset 静态, 1=主板)
  if (!mb_a)
    return;

  int axes_start_y = (n_d > 0) ? year_of(axes.dates[0]) : 0;
  int axes_warmup_d = n_d;
  for (int d = 0; d < n_d; ++d) {
    if (year_of(axes.dates[d]) >= axes_start_y + 3) {
      axes_warmup_d = d;
      break;
    }
  }
  int stock_warmup_d = n_d;
  if (meta.list_date[a].size() == 8) {
    int list_y = year_of(meta.list_date[a]);
    for (int d = 0; d < n_d; ++d) {
      if (year_of(axes.dates[d]) >= list_y + 3) {
        stock_warmup_d = d;
        break;
      }
    }
  }
  int warmup_d = std::max(axes_warmup_d, stock_warmup_d);

  const auto &divs = pool.dividend[a];

  auto apply_segment = [&](int seg_start, int seg_end, float val_3ysum) {
    if (seg_start < warmup_d)
      seg_start = warmup_d;
    if (seg_start < 0)
      seg_start = 0;
    if (seg_end > n_d)
      seg_end = n_d;
    for (int d = seg_start; d < seg_end; ++d) {
      if (!is_finite(ni_raw[d]) || ni_raw[d] <= 0.0f)
        continue;
      if (val_3ysum < 0.30f * ni_raw[d] && val_3ysum < 5e7f) {
        out[d] = 1.0f;
      }
    }
  };

  float current_3ysum = 0.0f;
  int next_apply_d = 0;

  for (std::size_t ev_idx = 0; ev_idx < divs.size(); ++ev_idx) {
    const auto &e = divs[ev_idx];
    int e_d = e.v;
    apply_segment(next_apply_d, e_d, current_3ysum);
    next_apply_d = e_d;

    // 业务判 dividend.ann_date 年份: e.v-1 是 visible_d
    int ann_y = (e.v >= 1 && e.v < n_d) ? year_of(axes.dates[e.v - 1]) : 0;
    if (ann_y == 0)
      continue;
    int lo = ann_y - 3, hi = ann_y - 1;

    float sum = 0.0f;
    for (std::size_t j = 0; j <= ev_idx; ++j) {
      const auto &p = divs[j];
      int py = year_of(p.report_date);
      if (py < lo || py > hi)
        continue;
      if (!is_finite(p.cash_after_tax))
        continue;
      int p_d = p.v;
      float sh = (p_d >= 0 && p_d < n_d) ? share_raw[p_d] : std::nanf("");
      if (!is_finite(sh))
        continue;
      sum += p.cash_after_tax * sh;
    }
    current_3ysum = sum;
  }
  apply_segment(next_apply_d, n_d, current_3ysum);
}

// risk_warn: 直读 cn_stock_status.st_status (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充).
//   输出 0=正常, 1=ST, 2=*ST (int8 → float 直接 cast). 数据起点前一律 0
//   (parse 时 prealloc 为 0, 文件不存在时不写, 保持初值).
//   注: 旧版本走 stock_st (Tushare 每日 ST 名单 + ffill + namechange 段修正) 是
//       为了兜住 tushare 票"今天不在 ST 名单"的二义性 (撤销 ST vs 退市整理期);
//       新数据 cn_stock_status 是交易所盘前快照, st_status 字段语义明确, 无需修正.
//   下游 cs_tradable 把 risk_warn > 0.5 视为排除 (1.0 ST / 2.0 *ST 都触发).
void ts_risk_warn(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::risk_warn, a);
  std::size_t base =
      static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  for (int d = 0; d < n_d; ++d) {
    int8_t st = pool.status.st_status[base + static_cast<std::size_t>(d)];
    out[d] = static_cast<float>(st);
  }
}

// trading_st: rolling 20D (low_p ∨ low_mc).all(). 单调连续计数即可.
void ts_trading_st(int a, const Axes &axes, const PitPool &, const StockMeta &,
                   Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::trading_st, a);
  auto low_p = T.ts_row(F::low_p, a);
  auto low_mc = T.ts_row(F::low_mc, a);
  constexpr int W = 20;
  std::fill(out.begin(), out.end(), 0.0f);
  int run = 0;
  for (int d = 0; d < n_d; ++d) {
    bool ok = (low_p[d] > 0.5f) || (low_mc[d] > 0.5f);
    run = ok ? run + 1 : 0;
    out[d] = (run >= W) ? 1.0f : 0.0f;
  }
}

// ============================================================================
// TS: 杂项 filter / pool
// ============================================================================

void ts_new_list(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  int n_d = axes.n_d();
  auto la = T.ts_row(F::list_age, a);
  auto out = T.ts_row(F::new_list, a);
  for (int d = 0; d < n_d; ++d) {
    // list_age PIT 契约: finite ⇒ ≥ 0; NaN = 未上市.
    out[d] = (is_finite(la[d]) && la[d] < 60.0f) ? 1.0f : 0.0f;
  }
}

// pool_b = exchange ∈ wl ∧ list_sector ∈ wl ∧ industry_l1 ∈ wl
//          ∧ ¬susp ∧ ¬退市 ∧ (true if include_margin else ¬is_margin)
//   exchange / list_sector 是 asset 静态 (全 D 同值, 启动期判一次);
//   industry_l1 是时变 (per-D 读 T.ts_row(F::industry_l1, a) → ID → mask 查白名单).
//   industry_l1 ID 0 (未知) 不在 mask 任何位 → ¬ind_ok, 自然排除.
void ts_pool_b(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto susp_ = T.ts_row(F::susp, a);
  auto is_marg_ = T.ts_row(F::is_margin, a);
  auto delist_age_ = T.ts_row(F::delist_age, a);
  auto industry_l1_ = T.ts_row(F::industry_l1, a);
  auto out = T.ts_row(F::pool_b, a);

  bool ex_ok = in_whitelist(meta.exchange[a], ::config::POOL_EXCHANGE_WHITELIST);
  bool sec_ok = in_int_whitelist(meta.list_sector[a],
                                 ::config::POOL_LIST_SECTOR_WHITELIST);
  bool asset_ok = ex_ok && sec_ok;
  const auto &mask = industry_l1_whitelist_mask();
  constexpr bool incl_margin = ::config::POOL_INCLUDE_MARGIN;

  for (int d = 0; d < n_d; ++d) {
    bool ind_ok = false;
    if (asset_ok) {
      int id = static_cast<int>(industry_l1_[d]);
      if (id > 0 && id < static_cast<int>(SW2021_L1_COUNT))
        ind_ok = mask[static_cast<std::size_t>(id)];
    }
    bool b = asset_ok && ind_ok && !(susp_[d] > 0.5f) &&
             !is_finite(delist_age_[d]);
    if (!incl_margin)
      b = b && !(is_marg_[d] > 0.5f);
    out[d] = b ? 1.0f : 0.0f;
  }
}

// ============================================================================
// CS: factor pipelines (gather → [1/x] → winsor_mad → z → pct_rank → scatter)
//   每个 feature 一行 factor_pipeline 调用; src/dst/invert 按字段表.
// ============================================================================

void cs_close(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::close_raw, F::close, /*invert=*/true, T, b.a);
}
void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::mcap_raw, F::mcap, true, T, b.a);
}
void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::fmcap_raw, F::fmcap, true, T, b.a);
}
void cs_pe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pe_raw, F::pe_ttm12, true, T, b.a);
}
void cs_pb_ttm3(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pb_raw, F::pb_ttm3, true, T, b.a);
}
void cs_ps_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::ps_raw, F::ps_ttm12, true, T, b.a);
}
void cs_pcf_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pcf_raw, F::pcf_ttm12, true, T, b.a);
}
void cs_roe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::roe_raw, F::roe_ttm12, /*invert=*/false, T, b.a);
}
void cs_roa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::roa_raw, F::roa_ttm12, false, T, b.a);
}
void cs_dy_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::dy_raw, F::dy_ttm12, false, T, b.a);
}

// pool: pool_b ∧ rank(mcap_raw asc within pool_b) ≤ POOL_UNIVERSE_SIZE
void cs_pool(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(F::pool_b, d, b.a);
  T.gather_cs_row(F::mcap_raw, d, b.b);

  std::vector<std::pair<float, int>> cands; // (mcap, a)
  cands.reserve(b.a.size());
  for (std::size_t a = 0; a < b.a.size(); ++a) {
    if (!(b.a[a] > 0.5f))
      continue;
    float mc = b.b[a];
    if (!is_finite(mc))
      continue;
    cands.emplace_back(mc, static_cast<int>(a));
  }

  int n = static_cast<int>(cands.size());
  int k = std::min(::config::POOL_UNIVERSE_SIZE, n);
  // k == n 时 cands.begin()+k == cands.end(), std::nth_element 第二参传 end 是 UB;
  // 全部入选时无需分割, 跳过 nth_element.
  if (k > 0 && k < n) {
    std::nth_element(cands.begin(), cands.begin() + k, cands.end(),
                     [](const auto &x, const auto &y) { return x.first < y.first; });
  }

  std::fill(b.c.begin(), b.c.end(), 0.0f);
  for (int i = 0; i < k; ++i)
    b.c[static_cast<std::size_t>(cands[i].second)] = 1.0f;
  T.scatter_cs_row(F::pool, d, std::span<const float>(b.c.data(), b.c.size()));
}

// tradable: pool ∧ ¬OR(config::STRATEGY_ENABLED_FILTERS).
//   pool 是 cs (pct_rank / nth-smallest 母集), tradable 是策略实际 top-K 母集.
//   filter 子集集中在 config.hpp; 删一行即禁用该 filter.
void cs_tradable(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(F::pool, d, b.a);
  std::size_t na = b.a.size();

  for (F src : ::config::STRATEGY_ENABLED_FILTERS) {
    T.gather_cs_row(src, d, b.b);
    for (std::size_t a = 0; a < na; ++a) {
      if (b.b[a] > 0.5f)
        b.a[a] = 0.0f;
    }
  }

  T.scatter_cs_row(F::tradable, d, std::span<const float>(b.a.data(), na));
}

// factor_score: pool 内 finite-加权平均.
//   buf 用法: a = score_num accumulator, b = score_den accumulator, c = factor temp / pool mask.
//   流: 0 累加 num/den; 1 读 pool 用作 mask; 2 num/den + pool → score; scatter.
//   注: factor_pipeline 已对 raw=0 (上市前) 填 0, 这里 finite 会算入分母 → score 有定义但分子被压;
//       对该业务可接受 — 上市前的样本理论上不会进 pool (asset list_age 过滤先于 pool).
void cs_factor_score(int d, const Axes &, Tensor &T, CsBufs &b) {
  std::size_t na = b.a.size();
  std::fill(b.a.begin(), b.a.end(), 0.0f);
  std::fill(b.b.begin(), b.b.end(), 0.0f);

  for (const auto &fw : ::config::STRATEGY_FACTOR_WEIGHTS) {
    T.gather_cs_row(fw.f, d, b.c);
    for (std::size_t a = 0; a < na; ++a) {
      float v = b.c[a];
      if (is_finite(v)) {
        b.a[a] += fw.w * v;
        b.b[a] += fw.w;
      }
    }
  }

  T.gather_cs_row(F::pool, d, b.c);
  for (std::size_t a = 0; a < na; ++a) {
    bool in_pool = b.c[a] > 0.5f;
    b.a[a] = (in_pool && b.b[a] > 0.0f) ? (b.a[a] / b.b[a]) : std::nanf("");
  }
  T.scatter_cs_row(F::factor_score, d, std::span<const float>(b.a.data(), na));
}

} // namespace impl

// ============================================================================
// FEATURES[] — 单点真理 (索引 = F 枚举值 = 计算顺序)
// ============================================================================

const std::array<FeatureMeta, static_cast<std::size_t>(F::COUNT)> FEATURES = {{
    // ============================== TS ==============================
    // raw 网格 — PitPool dense 直读 (per-A 动态)
    {"close_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_close_raw, nullptr},
    {"mcap_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_mcap_raw, nullptr},
    {"fmcap_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_fmcap_raw, nullptr},
    {"share_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_share_raw, nullptr},
    {"pb_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pb_raw, nullptr},
    {"ps_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ps_raw, nullptr},
    {"dy_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_dy_raw, nullptr},
    {"up_lim", Kind::Inter, Axis::TimeSeries, &impl::ts_up_lim, nullptr},
    {"dn_lim", Kind::Inter, Axis::TimeSeries, &impl::ts_dn_lim, nullptr},
    {"susp", Kind::Inter, Axis::TimeSeries, &impl::ts_susp, nullptr},
    {"is_margin", Kind::Inter, Axis::TimeSeries, &impl::ts_is_margin, nullptr},
    {"mr_bal_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_mr_bal_raw, nullptr},
    {"ms_bal_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ms_bal_raw, nullptr},
    // industry_l1 — sw2021 一级行业 ID per (D, A) (component 月初 + change 月内回放)
    {"industry_l1", Kind::Inter, Axis::TimeSeries, &impl::ts_industry_l1, nullptr},
    // raw 自算 — ttm12 拼接 (依赖 mcap_raw)
    {"rev_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_rev_raw, nullptr},
    {"ni_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ni_raw, nullptr},
    {"pe_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pe_raw, nullptr},
    {"pcf_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pcf_raw, nullptr},
    {"roe_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_roe_raw, nullptr},
    {"roa_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_roa_raw, nullptr},
    // raw meta 派生 — per-A 动态 (D - list_date / D - delist_date)
    {"list_age", Kind::Inter, Axis::TimeSeries, &impl::ts_list_age, nullptr},
    {"delist_age", Kind::Inter, Axis::TimeSeries, &impl::ts_delist_age, nullptr},
    // derived — 由 raw 推
    {"daily_return", Kind::Inter, Axis::TimeSeries, &impl::ts_daily_return, nullptr},
    {"low_p", Kind::Inter, Axis::TimeSeries, &impl::ts_low_p, nullptr},
    {"low_mc", Kind::Inter, Axis::TimeSeries, &impl::ts_low_mc, nullptr},
    {"limit_up", Kind::Inter, Axis::TimeSeries, &impl::ts_limit_up, nullptr},
    {"limit_dn", Kind::Inter, Axis::TimeSeries, &impl::ts_limit_dn, nullptr},
    // filter — 1 = 排除 (D, A)
    {"profit_st", Kind::Filter, Axis::TimeSeries, &impl::ts_profit_st, nullptr},
    {"revenue_st", Kind::Filter, Axis::TimeSeries, &impl::ts_revenue_st, nullptr},
    {"dividend_st", Kind::Filter, Axis::TimeSeries, &impl::ts_dividend_st, nullptr},
    {"risk_warn", Kind::Filter, Axis::TimeSeries, &impl::ts_risk_warn, nullptr},
    {"trading_st", Kind::Filter, Axis::TimeSeries, &impl::ts_trading_st, nullptr},
    {"new_list", Kind::Filter, Axis::TimeSeries, &impl::ts_new_list, nullptr},
    // pool (TS) — universe 母集
    {"pool_b", Kind::Inter, Axis::TimeSeries, &impl::ts_pool_b, nullptr},
    // ============================== CS ==============================
    // factor — winsor_mad → z → pct_rank
    {"close", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_close},
    {"mcap", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_mcap},
    {"fmcap", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_fmcap},
    {"pe_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pe_ttm12},
    {"pb_ttm3", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pb_ttm3},
    {"ps_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_ps_ttm12},
    {"pcf_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pcf_ttm12},
    {"roe_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roe_ttm12},
    {"roa_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roa_ttm12},
    {"dy_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_dy_ttm12},
    // pool (CS) — universe 母集
    {"pool", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_pool},
    {"tradable", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_tradable},
    {"factor_score", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_factor_score},
}};

} // namespace feature
