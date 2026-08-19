#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstdint>
#include <map>

// roa_raw: 同时读 ttm + balance 两路事件流.
//   ROA = NP_ttm(含少数) / avg(total_assets) × 100
//     分子取含少数: 总资产由全体股东 (含少数) 与债权人共同支撑, 配归母净利是
//     两边错配 (母公司只享部分子公司权益却摊全部资产). 果仁亦用含少数, 换过来后
//     roa_raw 对 test/1.csv 的命中 (5% 容差) 18.2% → 94%.

namespace feature::def {

inline void ts_roa_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec roa_raw_spec{
    "roa_raw", Kind::Inter, Axis::TimeSeries, {}, &ts_roa_raw, nullptr};

inline void ts_roa_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T) {
  detail::scan_latest_ttm_and_balance(
      a, axes, pool, meta, T, roa_raw_spec,
      [](int /*d*/, const FinancialTtmEv &t,
         const std::map<std::int32_t, FinancialBalanceEv> &by_rd) -> float {
        float n = t.net_profit_ttm;
        float as = detail::ttm_window_avg(t.report_date, by_rd,
                                          &FinancialBalanceEv::total_assets);
        return (is_finite(n) && is_finite(as) && as > 0.0f)
                   ? (n / as) * 100.0f
                   : std::nanf("");
      });
}

} // namespace feature::def
