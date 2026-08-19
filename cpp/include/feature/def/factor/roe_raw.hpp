#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstdint>
#include <map>

// roe_raw: 同时读 ttm + balance 两路事件流 (detail::scan_latest_ttm_and_balance).
//   ROE = NP_parent_ttm / avg(equity_to_parent) × 100  (经典归母 ROE, 分子分母同归母)
//   分母走 TTM 窗口平均而非期末值: 分子是 12 个月的流量, 分母必须是同窗口的平均
//   存量才同口径 (教科书 ROE = NI / average equity). 期末值配 TTM 分子会在权益
//   快速变动 (增发 / 回购 / 大额分红) 的股票上系统性失真.

namespace feature::def {

inline void ts_roe_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec roe_raw_spec{
    "roe_raw", Kind::Inter, Axis::TimeSeries, {}, &ts_roe_raw, nullptr};

inline void ts_roe_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T) {
  detail::scan_latest_ttm_and_balance(
      a, axes, pool, meta, T, roe_raw_spec,
      [](int /*d*/, const FinancialTtmEv &t,
         const std::map<std::int32_t, FinancialBalanceEv> &by_rd) -> float {
        float n = t.net_profit_to_parent_shareholders_ttm;
        float eq = detail::ttm_window_avg(
            t.report_date, by_rd,
            &FinancialBalanceEv::total_equity_to_parent_shareholders);
        return (is_finite(n) && is_finite(eq) && eq > 0.0f)
                   ? (n / eq) * 100.0f
                   : std::nanf("");
      });
}

} // namespace feature::def
