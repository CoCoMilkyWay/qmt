#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/factor/fmcap_raw.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"

// factor: fmcap — 流通市值截面排名. pct_rank(z(winsor_mad(fmcap_raw))) + 截面均值填充.
//   值越大表示流通市值越大.

namespace feature::def {

inline void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *fmcap_deps[] = {&fmcap_raw_spec, &list_age_spec,
                                                     &delist_age_spec};

inline constexpr FeatureSpec fmcap_spec{
    "fmcap", "流通市值因子", Kind::Factor, Axis::CrossSection, fmcap_deps, nullptr, &cs_fmcap,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(fmcap_raw))) + 截面均值填充",
    /*assumption=*/"—; 值越大流通市值越大"};

inline void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(fmcap_raw_spec, d, b.a);
  factor_pipeline(d, fmcap_spec, T, b);
}

} // namespace feature::def
