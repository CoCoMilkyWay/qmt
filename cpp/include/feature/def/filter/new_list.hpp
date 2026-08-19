#pragma once

#include "feature/axis.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// filter: new_list — 次新股 (0 ≤ list_age < 60 日历日).

namespace feature::def {

inline void ts_new_list(int a, const Axes &axes, const PitPool &pool,
                        const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *new_list_deps[] = {&list_age_spec};

inline constexpr FeatureSpec new_list_spec{
    "new_list", Kind::Filter, Axis::TimeSeries, new_list_deps, &ts_new_list,
    nullptr, /*must_be_finite=*/true,
    /*formula=*/"0 ≤ list_age < 60",
    /*assumption=*/"—"};

inline void ts_new_list(int a, const Axes &axes, const PitPool &,
                        const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto la = T.ts_row(list_age_spec, a);
  auto out = T.ts_row(new_list_spec, a);
  for (int d = 0; d < n_d; ++d) {
    // list_age PIT 契约: finite ⇒ ≥ 0; NaN = 未上市.
    out[d] = (is_finite(la[d]) && la[d] < 60.0f) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
