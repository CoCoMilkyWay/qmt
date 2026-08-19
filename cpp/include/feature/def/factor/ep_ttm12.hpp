#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pe_raw.hpp"
#include "feature/graph.hpp"

#include <cmath>

// factor: ep_ttm12 — 中性 EP (盈利收益率 = 1 / pe_raw).
//   pct_rank(z(neutralize(winsorize_quantile(EP)))) + 截面均值填充.

namespace feature::def {

inline void cs_ep_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *ep_ttm12_deps[] = {
    &pe_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec ep_ttm12_spec{
    "ep_ttm12", "中性EP", Kind::Factor, Axis::CrossSection, ep_ttm12_deps, nullptr,
    &cs_ep_ttm12, /*must_be_finite=*/true,
    /*formula=*/
    "EP = 1 / pe_raw; pct_rank(z(neutralize(winsorize_quantile(EP)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—; pe_raw == 0 → EP 记 NaN"};

inline void cs_ep_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(pe_raw_spec, d, b.a);
  for (float &v : b.a) {
    if (!is_finite(v) || v == 0.0f)
      v = std::nanf("");
    else
      v = 1.0f / v;
  }
  neutral_pipeline(d, ep_ttm12_spec, T, b);
}

} // namespace feature::def
