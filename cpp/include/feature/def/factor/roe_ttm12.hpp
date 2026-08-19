#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/roe_raw.hpp"
#include "feature/graph.hpp"

// factor: roe_ttm12 — 中性 ROE. pct_rank(z(neutralize(winsorize_quantile(roe_raw))))
//   + 截面均值填充; 无 invert (越大越优).

namespace feature::def {

inline void cs_roe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *roe_ttm12_deps[] = {
    &roe_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec roe_ttm12_spec{
    "roe_ttm12", "中性ROE", Kind::Factor, Axis::CrossSection, roe_ttm12_deps, nullptr,
    &cs_roe_ttm12, /*must_be_finite=*/true,
    /*formula=*/
    "pct_rank(z(neutralize(winsorize_quantile(roe_raw)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—"};

inline void cs_roe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, roe_raw_spec, roe_ttm12_spec, /*invert=*/false, T, b);
}

} // namespace feature::def
