#pragma once

#include "feature/axis.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// low_p: close_raw ∈ (0, 1) 元/股 — 极低价预警.

namespace feature::def {

inline void ts_low_p(int a, const Axes &axes, const PitPool &pool,
                     const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *low_p_deps[] = {&close_raw_spec};

inline constexpr FeatureSpec low_p_spec{
    "low_p", Kind::Inter, Axis::TimeSeries, low_p_deps, &ts_low_p, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/"close_raw < 1.0",
    /*assumption=*/"[bool]"};

inline void ts_low_p(int a, const Axes &axes, const PitPool &, const StockMeta &,
                     Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(close_raw_spec, a);
  auto out = T.ts_row(low_p_spec, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(cl[d]) && cl[d] > 0.0f && cl[d] < 1.0f) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
