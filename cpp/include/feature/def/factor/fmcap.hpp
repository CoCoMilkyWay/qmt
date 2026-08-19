#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/factor/fmcap_raw.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"

// factor: fmcap — 小流通市值因子. pct_rank(z(winsor_mad(1 / fmcap_raw))) + 截面均值填充.

namespace feature::def {

inline void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *fmcap_deps[] = {&fmcap_raw_spec, &list_age_spec,
                                                     &delist_age_spec};

inline constexpr FeatureSpec fmcap_spec{
    "fmcap", Kind::Factor, Axis::CrossSection, fmcap_deps, nullptr, &cs_fmcap};

inline void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, fmcap_raw_spec, fmcap_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
