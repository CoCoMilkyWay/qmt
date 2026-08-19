#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// ms_bal_raw ← margin_detail.securities_lending_balance ([元])

namespace feature::def {

inline void ts_ms_bal_raw(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec ms_bal_raw_spec{
    "ms_bal_raw", "融券余额", Kind::Inter, Axis::TimeSeries, {}, &ts_ms_bal_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"cn_stock_margin_trading_detail.securities_lending_balance",
    /*assumption=*/"[元]; 融券余额; per-A grid post_ffill"};

inline void ts_ms_bal_raw(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T) {
  detail::grid_copy(a, axes, meta, T, ms_bal_raw_spec,
                    pool.margin_detail.securities_lending_balance, false);
}

} // namespace feature::def
