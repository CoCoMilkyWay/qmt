#include "feature/feature.hpp"

#include "feature/axis.hpp"
#include "feature/cs.hpp"
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

void ts_close_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::close_raw,
            [&]() -> const std::vector<float> & { return pool.daily_basic.close; });
}

void ts_mcap_raw(int a, const Axes &axes, const PitPool &pool,
                 const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::mcap_raw, [&]() -> const std::vector<float> & { return pool.daily_basic.total_mv; }, 1e4f);
}

void ts_fmcap_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::fmcap_raw, [&]() -> const std::vector<float> & { return pool.daily_basic.circ_mv; }, 1e4f);
}

void ts_share_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::share_raw, [&]() -> const std::vector<float> & { return pool.daily_basic.total_share; }, 1e4f);
}

void ts_pb_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::pb_raw,
            [&]() -> const std::vector<float> & { return pool.daily_basic.pb; });
}

void ts_ps_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::ps_raw,
            [&]() -> const std::vector<float> & { return pool.daily_basic.ps_ttm; });
}

void ts_dy_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::dy_raw,
            [&]() -> const std::vector<float> & { return pool.daily_basic.dv_ttm; });
}

// up_lim / dn_lim feature 主动 -1 与 close_raw (post-market) 对齐; 同期未复权价比对.
void ts_up_lim(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(F::up_lim, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = pool.stk_limit.up_limit[base + static_cast<std::size_t>(d - 1)];
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
    out[d] = pool.stk_limit.down_limit[base + static_cast<std::size_t>(d - 1)];
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

void ts_susp(int a, const Axes &axes, const PitPool &pool, const StockMeta &,
             Tensor &T) {
  grid_copy_bool(a, axes, T, F::susp,
                 [&]() -> const std::vector<uint8_t> & { return pool.suspend_d.susp; });
}

void ts_is_margin(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  grid_copy_bool(a, axes, T, F::is_margin,
                 [&]() -> const std::vector<uint8_t> & { return pool.margin_secs.is_margin; });
}

void ts_mr_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::mr_bal_raw,
            [&]() -> const std::vector<float> & { return pool.margin_detail.mr_bal; });
}

void ts_ms_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::ms_bal_raw,
            [&]() -> const std::vector<float> & { return pool.margin_detail.ms_bal; });
}

// ============================================================================
// TS: raw 自算 — ttm4_ytd 拼接 (依赖 mcap_raw 已就绪 → enum 顺序保证)
// ============================================================================

void ts_rev_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  auto out = T.ts_row(F::rev_raw, a);
  ttm4_ytd_compute(
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

// pe_raw: 自己算 mcap_raw / ttm4(n_income_attr_p)，支持负 PE（亏损）
void ts_pe_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::vector<float> ni_ttm(n_d, std::nanf(""));
  ttm4_ytd_compute(
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
  ttm4_ytd_compute(
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
  ttm4_ytd_compute(
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
  ttm4_ytd_compute(
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

// daily_return: 前复权链式日收益. d==0 或 close_raw[d-1] NaN/0 → NaN.
//   下游 benchmark = pool 内等权 daily_return 均值.
void ts_daily_return(int a, const Axes &axes, const PitPool &, const StockMeta &,
                     Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::daily_return, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    float c0 = cl[d - 1];
    float c1 = cl[d];
    out[d] = (is_finite(c0) && c0 != 0.0f && is_finite(c1)) ? (c1 / c0 - 1.0f)
                                                            : std::nanf("");
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

// low_mc: 主板阈值 5e8, 非主板 3e8. mb 判定直接 inline meta.market[a] (asset 静态, 全 D 同值).
void ts_low_mc(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto mc = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::low_mc, a);
  float thr = (meta.market[a] == "主板") ? 5e8f : 3e8f;
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

  bool mb_a = (meta.market[a] == "主板"); // 仅主板适用 (asset 静态, 全 D 同值)
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
//   3y_sum = Σ over 历史 events with end_date.Y in [ann_y-3, ann_y-1]
//            的 cash_div_tax × share_raw[event.v]; 区间 [e.v, next.v) 填.
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

  bool mb_a = (meta.market[a] == "主板"); // 仅主板适用 (asset 静态, 全 D 同值)
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
      int py = year_of(p.end_date);
      if (py < lo || py > hi)
        continue;
      if (!is_finite(p.cash_div_tax))
        continue;
      int p_d = p.v;
      float sh = (p_d >= 0 && p_d < n_d) ? share_raw[p_d] : std::nanf("");
      if (!is_finite(sh))
        continue;
      sum += p.cash_div_tax * sh;
    }
    current_3ysum = sum;
  }
  apply_segment(next_apply_d, n_d, current_3ysum);
}

// risk_warn: stock_st 每日快照 (含 ffill 0→1/2) + namechange 段修正.
//   输出 0=正常, 1=ST (name 不含 '*'), 2=*ST (name 含 '*').
//
//   单一 stock_st 不准: tushare "票今天不在 ST 名单" 二义 — ① 撤销 ST 转正常 (应=0)
//   ② 进入退市整理期 "退市XX"/"XX退", tushare 不再 list (应保留 *ST 等级).
//   ffill 无法区分这两种情况. 用 namechange 派生的 "当段 name" 做边界修正:
//     段 = [start_date_i, start_date_{i+1}), name = records[i].name
//     段 name 含 "ST" 或 "退" → 信 stock_st (保留 ffill 后的 1/2)
//     段 name 不含 "ST" 也不含 "退" → 强制 0 (撤销 ST 转正常段, 抹掉 ffill 误延续)
//     最早 start_date 之前 (无 namechange 记录段) → 信 stock_st (上市初期 / 未改名票)
//
//   关键字检测: std::string::find 字节级匹配, "退" 在 UTF-8 是 0xE9 0x80 0x80, 不与 ASCII "ST" 冲突.
void ts_risk_warn(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::risk_warn, a);
  std::size_t base =
      static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);

  const auto &hist = meta.name_history[static_cast<std::size_t>(a)];

  // 段游标 cur: -1 = 早于所有 namechange (信 stock_st); 否则 = 当前段索引.
  int cur = -1;
  bool keep_st = true; // 当前段是否允许 stock_st 值 (cur=-1 时为 true)
  std::size_t next_i = 0;

  for (int d = 0; d < n_d; ++d) {
    const std::string &today = axes.dates[static_cast<std::size_t>(d)];
    // 推进游标到包含 d 的段
    while (next_i < hist.size() && hist[next_i].start_date <= today) {
      cur = static_cast<int>(next_i);
      const std::string &nm = hist[next_i].name;
      keep_st = (nm.find("ST") != std::string::npos) ||
                (nm.find("\xe9\x80\x80") != std::string::npos); // "退" UTF-8
      ++next_i;
    }
    uint8_t raw = pool.stock_st.state[base + static_cast<std::size_t>(d)];
    out[d] = keep_st ? static_cast<float>(raw) : 0.0f;
  }
  (void)cur; // cur 仅用于语义/调试, 可省
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

void ts_pool_b(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto susp_ = T.ts_row(F::susp, a);
  auto is_marg_ = T.ts_row(F::is_margin, a);
  auto delist_age_ = T.ts_row(F::delist_age, a);
  auto out = T.ts_row(F::pool_b, a);
  bool ex_ok = in_whitelist(meta.exchange[a], ::config::POOL_EXCHANGE_WHITELIST);
  bool mk_ok = in_whitelist(meta.market[a], ::config::POOL_MARKET_WHITELIST);
  bool ind_ok =
      in_whitelist(meta.industry_l1[a], ::config::POOL_INDUSTRY_L1_WHITELIST);
  bool asset_ok = ex_ok && mk_ok && ind_ok;
  constexpr bool incl_margin = ::config::POOL_INCLUDE_MARGIN;
  for (int d = 0; d < n_d; ++d) {
    // 已退市 (退市当日含) ↔ delist_age finite (PIT 契约保证 finite ⇒ ≥ 0).
    bool b = asset_ok && !(susp_[d] > 0.5f) && !is_finite(delist_age_[d]);
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
void cs_pe_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pe_raw, F::pe_ttm4, true, T, b.a);
}
void cs_pb_ttm1(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pb_raw, F::pb_ttm1, true, T, b.a);
}
void cs_ps_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::ps_raw, F::ps_ttm4, true, T, b.a);
}
void cs_pcf_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::pcf_raw, F::pcf_ttm4, true, T, b.a);
}
void cs_roe_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::roe_raw, F::roe_ttm4, /*invert=*/false, T, b.a);
}
void cs_roa_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::roa_raw, F::roa_ttm4, false, T, b.a);
}
void cs_dy_ttm4(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::dy_raw, F::dy_ttm4, false, T, b.a);
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
  if (k > 0) {
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
    // raw 自算 — ttm4_ytd 拼接 (依赖 mcap_raw)
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
    {"pe_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pe_ttm4},
    {"pb_ttm1", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pb_ttm1},
    {"ps_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_ps_ttm4},
    {"pcf_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pcf_ttm4},
    {"roe_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roe_ttm4},
    {"roa_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roa_ttm4},
    {"dy_ttm4", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_dy_ttm4},
    // pool (CS) — universe 母集
    {"pool", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_pool},
    {"tradable", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_tradable},
    {"factor_score", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_factor_score},
}};

} // namespace feature
