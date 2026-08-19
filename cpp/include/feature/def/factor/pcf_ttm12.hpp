#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pcf_raw.hpp"
#include "feature/graph.hpp"

// factor: pcf_ttm12 — 中性 CP. pct_rank(z(neutralize(winsorize_quantile(1 / pcf_raw))))
//   + 截面均值填充.

namespace feature::def {

inline void cs_pcf_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *pcf_ttm12_deps[] = {
    &pcf_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec pcf_ttm12_spec{
    "pcf_ttm12", Kind::Factor, Axis::CrossSection, pcf_ttm12_deps, nullptr,
    &cs_pcf_ttm12};

inline void cs_pcf_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, pcf_raw_spec, pcf_ttm12_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
