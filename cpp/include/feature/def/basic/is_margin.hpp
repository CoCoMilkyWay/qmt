#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// is_margin ← margin_detail (D, A) 存在性 (CUTOFF=0)

namespace feature::def {

inline void ts_is_margin(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec is_margin_spec{
    "is_margin", "两融标记", Kind::Inter, Axis::TimeSeries, {}, &ts_is_margin, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/"1.0 if cn_stock_margin_trading_detail (D, A) 存在 else 0.0",
    /*assumption=*/"[bool]; 当日是否融资融券标的"};

inline void ts_is_margin(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &, Tensor &T) {
  detail::grid_copy_bool(a, axes, T, is_margin_spec, pool.margin_detail.is_margin);
}

} // namespace feature::def
