#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"

// factor: close — 低价因子. pct_rank(z(winsor_mad(1 / close_raw))) + 截面均值填充.

namespace feature::def {

inline void cs_close(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *close_deps[] = {
    &close_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec close_spec{
    "close", Kind::Factor, Axis::CrossSection, close_deps, nullptr, &cs_close,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(1 / close_raw))) + 截面均值填充",
    /*assumption=*/"—; 低价因子, 越低价越优"};

inline void cs_close(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, close_raw_spec, close_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
