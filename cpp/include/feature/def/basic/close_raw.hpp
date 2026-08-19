#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// close_raw ← cn_stock_real_bar1d.close (不复权 [元/股]).
//   row D = D-1 实际收盘 (CUTOFF=-1). 不复权口径: close 是当日真实成交价,
//   PIT-immutable (历史不随任何除权动作改写), 与 limit_price / total_shares
//   同口径. daily_return 直接基于该 close 链式, 除权日含分红/送股的真实跳跃.

namespace feature::def {

inline void ts_close_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec close_raw_spec{
    "close_raw", "收盘价", Kind::Inter, Axis::TimeSeries, {}, &ts_close_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"cn_stock_real_bar1d.close (不复权真价)",
    /*assumption=*/
    "[元/股, 不复权真价]; PIT-immutable (不随除权改写), 与 limit_price / "
    "total_shares 同口径 ⇒ 真市值/真涨跌停/真低价股都用它; adjust_factor "
    "只在 daily_return 内部用, 不入 tensor"};

inline void ts_close_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T) {
  detail::grid_copy(a, axes, meta, T, close_raw_spec, pool.bar1d.close);
}

} // namespace feature::def
