#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"

// factor: close — 股价截面排名. pct_rank(z(winsor_mad(close_raw))) + 截面均值填充.
//   值越大表示股价越高.

namespace feature::def {

inline void cs_close(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *close_deps[] = {
    &close_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec close_spec{
    "close", "股价因子", Kind::Factor, Axis::CrossSection, close_deps, nullptr, &cs_close,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(close_raw))) + 截面均值填充",
    /*assumption=*/"—; 值越大股价越高"};

inline void cs_close(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(close_raw_spec, d, b.a);
  factor_pipeline(d, close_spec, T, b);
}

} // namespace feature::def
