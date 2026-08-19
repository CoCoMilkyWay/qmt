#pragma once

#include "feature/axis.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/def/factor/dn_lim.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// limit_dn: close 与 D-1 适用跌停同向触碰判定. 同 limit_up.

namespace feature::def {

inline void ts_limit_dn(int a, const Axes &axes, const PitPool &pool,
                        const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *limit_dn_deps[] = {&close_raw_spec,
                                                       &dn_lim_spec};

inline constexpr FeatureSpec limit_dn_spec{
    "limit_dn", "跌停标记", Kind::Inter, Axis::TimeSeries, limit_dn_deps, &ts_limit_dn,
    nullptr, /*must_be_finite=*/true,
    /*formula=*/"close_raw ≤ dn_lim + 1e-4",
    /*assumption=*/"[bool]; 策略跌停判定"};

inline void ts_limit_dn(int a, const Axes &axes, const PitPool &,
                        const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(close_raw_spec, a);
  auto lm = T.ts_row(dn_lim_spec, a);
  auto out = T.ts_row(limit_dn_spec, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(cl[d]) && cl[d] > 0.0f && is_finite(lm[d]) &&
              lm[d] > 0.0f && cl[d] <= lm[d] + 1e-4f)
                 ? 1.0f
                 : 0.0f;
  }
}

} // namespace feature::def
