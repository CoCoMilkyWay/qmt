#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"

// factor: mcap — 小市值因子. pct_rank(z(winsor_mad(1 / mcap_raw))) + 截面均值填充.

namespace feature::def {

inline void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *mcap_deps[] = {&mcap_raw_spec, &list_age_spec,
                                                    &delist_age_spec};

inline constexpr FeatureSpec mcap_spec{
    "mcap", "总市值因子", Kind::Factor, Axis::CrossSection, mcap_deps, nullptr, &cs_mcap,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(1 / mcap_raw))) + 截面均值填充",
    /*assumption=*/"—"};

inline void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, mcap_raw_spec, mcap_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
