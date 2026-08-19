#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/roa_raw.hpp"
#include "feature/graph.hpp"

// factor: roa_ttm12 — 中性 ROA. pct_rank(z(neutralize(winsorize_quantile(roa_raw))))
//   + 截面均值填充; 无 invert.

namespace feature::def {

inline void cs_roa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *roa_ttm12_deps[] = {
    &roa_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec roa_ttm12_spec{
    "roa_ttm12", Kind::Factor, Axis::CrossSection, roa_ttm12_deps, nullptr,
    &cs_roa_ttm12, /*must_be_finite=*/true,
    /*formula=*/
    "pct_rank(z(neutralize(winsorize_quantile(roa_raw)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—"};

inline void cs_roa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, roa_raw_spec, roa_ttm12_spec, /*invert=*/false, T, b);
}

} // namespace feature::def
