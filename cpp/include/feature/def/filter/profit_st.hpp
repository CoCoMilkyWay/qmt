#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"
#include "feature/ts.hpp"

#include <vector>

// filter: profit_st — 年报预亏 (首亏/续亏 ∧ 上年归母净利 < 0) 状态机.
//   触发 forecast.ann_date, 终止 min(正式 PIT 年报, 次年 4 月底安全网).

namespace feature::def {

inline void ts_profit_st(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec profit_st_spec{
    "profit_st", "预亏预警", Kind::Filter, Axis::TimeSeries, {}, &ts_profit_st, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/
    "状态机 (per A): forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} "
    "∧ forecast.last_parent_net < 0 时按 forecast.ann_date 触发, 至 "
    "cn_stock_financial_income_general_pit.report_date == forecast.end_date "
    "或 (end_date.Y+1, 4, monthend) 终止 (取较早)",
    /*assumption=*/
    "正式 PIT 年报出现即终止; 未披露的股票不出, 4 月底安全网兜底"};

inline void ts_profit_st(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &, Tensor &T) {
  std::vector<ForecastEv> trig;
  trig.reserve(pool.forecast[a].size());
  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != ForecastType::FirstLoss &&
        e.type != ForecastType::ContinueLoss)
      continue;
    if (!is_finite(e.last_parent_net) || e.last_parent_net >= 0.0f)
      continue;
    trig.push_back(e);
  }
  state_machine_intervals(
      trig, axes.n_d(),
      [&](const ForecastEv &fe) {
        return detail::find_forecast_off_d(fe, pool.financial_income_annual[a],
                                           axes);
      },
      T.ts_row(profit_st_spec, a));
}

} // namespace feature::def
