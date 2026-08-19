#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// share_raw ← cn_stock_shares.total_shares ([股]; 直读, 无单位换算)

namespace feature::def {

inline void ts_share_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec share_raw_spec{
    "share_raw", Kind::Inter, Axis::TimeSeries, {}, &ts_share_raw, nullptr};

inline void ts_share_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T) {
  detail::grid_copy(a, axes, meta, T, share_raw_spec, pool.shares.total_shares);
}

} // namespace feature::def
