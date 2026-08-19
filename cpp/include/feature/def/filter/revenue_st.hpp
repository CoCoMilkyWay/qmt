#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/rev_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>

// filter: revenue_st — 主板营收退市预警 (预亏 ∧ rev_raw 低于年度阈值) 状态机.
//   仅 2021 新规后 (end_date.Y ≥ 2021 ∧ ann_date ≥ 20210101) 生效.

namespace feature::def {

inline void ts_revenue_st(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *revenue_st_deps[] = {&rev_raw_spec};

inline constexpr FeatureSpec revenue_st_spec{
    "revenue_st", "营收预警", Kind::Filter, Axis::TimeSeries, revenue_st_deps,
    &ts_revenue_st, nullptr, /*must_be_finite=*/true,
    /*formula=*/
    "状态机 (per A) ∧ meta.list_sector == 1 ∧ rev_raw < (3e8 if end_date.Y ≥ "
    "2024 else 1e8): forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} "
    "∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101 时按 "
    "forecast.ann_date 触发, 至 cn_stock_financial_income_general_pit."
    "report_date == forecast.end_date 或 (end_date.Y+1, 4, monthend) 终止 "
    "(取较早)",
    /*assumption=*/
    "同 profit_st 终止条件; rev_raw 仅作区间内营收阈值; list_sector int8: "
    "1=主板 / 2=创业板 / 3=科创板 / 4=北交所"};

inline void ts_revenue_st(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(revenue_st_spec, a);
  auto rev_raw = T.ts_row(rev_raw_spec, a);
  std::fill(out.begin(), out.end(), 0.0f);

  bool mb_a = (meta.list_sector[a] == 1); // 仅主板适用 (asset 静态, 1=主板)
  if (!mb_a)
    return;

  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != ForecastType::FirstLoss &&
        e.type != ForecastType::ContinueLoss)
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
    int off_d =
        detail::find_forecast_off_d(e, pool.financial_income_annual[a], axes);
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

} // namespace feature::def
