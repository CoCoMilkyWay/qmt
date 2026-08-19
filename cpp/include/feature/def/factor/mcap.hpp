#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"

// factor: mcap — 总市值截面排名. pct_rank(z(winsor_mad(mcap_raw))) + 截面均值填充.
//   值越大表示市值越大.

namespace feature::def {

inline void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *mcap_deps[] = {&mcap_raw_spec, &list_age_spec,
                                                    &delist_age_spec};

inline constexpr FeatureSpec mcap_spec{
    "mcap", "总市值因子", Kind::Factor, Axis::CrossSection, mcap_deps, nullptr, &cs_mcap,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(mcap_raw))) + 截面均值填充",
    /*assumption=*/"—; 值越大市值越大"};

inline void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(mcap_raw_spec, d, b.a);
  factor_pipeline(d, mcap_spec, T, b);
}

} // namespace feature::def
