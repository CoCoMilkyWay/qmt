#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// susp ← cn_stock_status.suspended (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充)

namespace feature::def {

inline void ts_susp(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec susp_spec{
    "susp", Kind::Inter, Axis::TimeSeries, {}, &ts_susp, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/"cn_stock_status.suspended == 1",
    /*assumption=*/"[bool]; 当日是否停牌"};

inline void ts_susp(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &, Tensor &T) {
  detail::grid_copy_bool(a, axes, T, susp_spec, pool.status.suspended);
}

} // namespace feature::def
