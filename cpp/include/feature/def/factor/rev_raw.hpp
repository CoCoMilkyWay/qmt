#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// rev_raw ← ttm.total_operating_revenue_ttm; 给 revenue_st 过滤用 (同 ps_raw 分母).
//   <= 0 → NaN (同 ps_raw: 负营收是 BigQuant 脏值). 这里尤其要紧 — revenue_st 判
//   "rev_raw < 3e8/1e8", 负值会让阈值恒真, 把脏数据直接变成误报的退市预警.

namespace feature::def {

inline void ts_rev_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec rev_raw_spec{
    "rev_raw", Kind::Inter, Axis::TimeSeries, {}, &ts_rev_raw, nullptr};

inline void ts_rev_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T) {
  detail::scan_latest_ttm(a, axes, pool, meta, T, rev_raw_spec,
                          [](int /*d*/, const FinancialTtmEv &e) -> float {
                            float r = e.total_operating_revenue_ttm;
                            return (is_finite(r) && r > 0.0f) ? r
                                                              : std::nanf("");
                          });
}

} // namespace feature::def
