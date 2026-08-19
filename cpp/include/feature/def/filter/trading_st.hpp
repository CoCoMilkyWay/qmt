#pragma once

#include "feature/axis.hpp"
#include "feature/def/factor/low_mc.hpp"
#include "feature/def/factor/low_p.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>

// filter: trading_st — rolling 20D (low_p ∨ low_mc).all() 交易类退市预警.

namespace feature::def {

inline void ts_trading_st(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *trading_st_deps[] = {&low_p_spec,
                                                          &low_mc_spec};

inline constexpr FeatureSpec trading_st_spec{
    "trading_st", Kind::Filter, Axis::TimeSeries, trading_st_deps,
    &ts_trading_st, nullptr, /*must_be_finite=*/true};

// trading_st: rolling 20D (low_p ∨ low_mc).all(). 单调连续计数即可.
inline void ts_trading_st(int a, const Axes &axes, const PitPool &,
                          const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(trading_st_spec, a);
  auto low_p = T.ts_row(low_p_spec, a);
  auto low_mc = T.ts_row(low_mc_spec, a);
  constexpr int W = 20;
  std::fill(out.begin(), out.end(), 0.0f);
  int run = 0;
  for (int d = 0; d < n_d; ++d) {
    bool ok = (low_p[d] > 0.5f) || (low_mc[d] > 0.5f);
    run = ok ? run + 1 : 0;
    out[d] = (run >= W) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
