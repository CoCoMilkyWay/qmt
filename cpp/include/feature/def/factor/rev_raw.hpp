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
    "rev_raw", "营业总收入TTM", Kind::Inter, Axis::TimeSeries, {}, &ts_rev_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/
    "ttm.total_operating_revenue_ttm (shift=0 latest visible); ≤ 0 → NaN",
    /*assumption=*/
    "[元]; ttm12; 与 ps_raw 同源 (含利息/保费); 给 revenue_st 用. ≤0 必须剔: "
    "负值让 revenue_st 的 rev_raw < 3e8/1e8 恒真, 脏值直接变误报退市预警"};

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
