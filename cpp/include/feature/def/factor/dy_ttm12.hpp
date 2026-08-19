#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/factor/dy_raw.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"

// factor: dy_ttm12 — 中性股息率. pct_rank(z(neutralize(winsorize_quantile(dy_raw))))
//   + 截面均值填充; 无 invert.

namespace feature::def {

inline void cs_dy_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *dy_ttm12_deps[] = {
    &dy_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec dy_ttm12_spec{
    "dy_ttm12", Kind::Factor, Axis::CrossSection, dy_ttm12_deps, nullptr,
    &cs_dy_ttm12};

inline void cs_dy_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, dy_raw_spec, dy_ttm12_spec, /*invert=*/false, T, b);
}

} // namespace feature::def
