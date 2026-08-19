#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/ps_raw.hpp"
#include "feature/graph.hpp"

// factor: ps_ttm12 — 中性 SP. pct_rank(z(neutralize(winsorize_quantile(1 / ps_raw))))
//   + 截面均值填充.

namespace feature::def {

inline void cs_ps_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *ps_ttm12_deps[] = {
    &ps_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec ps_ttm12_spec{
    "ps_ttm12", Kind::Factor, Axis::CrossSection, ps_ttm12_deps, nullptr,
    &cs_ps_ttm12};

inline void cs_ps_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, ps_raw_spec, ps_ttm12_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
