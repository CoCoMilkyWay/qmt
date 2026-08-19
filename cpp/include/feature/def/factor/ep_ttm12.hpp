#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pe_raw.hpp"
#include "feature/graph.hpp"

// factor: ep_ttm12 — 中性 EP. pct_rank(z(neutralize(winsorize_quantile(1 / pe_raw))))
//   + 截面均值填充.

namespace feature::def {

inline void cs_ep_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *ep_ttm12_deps[] = {
    &pe_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec ep_ttm12_spec{
    "ep_ttm12", "中性EP", Kind::Factor, Axis::CrossSection, ep_ttm12_deps, nullptr,
    &cs_ep_ttm12, /*must_be_finite=*/true,
    /*formula=*/
    "pct_rank(z(neutralize(winsorize_quantile(1 / pe_raw)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—"};

inline void cs_ep_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, pe_raw_spec, ep_ttm12_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
