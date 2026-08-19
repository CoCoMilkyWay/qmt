#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pcf_raw.hpp"
#include "feature/graph.hpp"

#include <cmath>

// factor: cp_ttm12 — 中性 CP (现金流收益率 = 1 / pcf_raw).
//   pct_rank(z(neutralize(winsorize_quantile(CP)))) + 截面均值填充.

namespace feature::def {

inline void cs_cp_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *cp_ttm12_deps[] = {
    &pcf_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec cp_ttm12_spec{
    "cp_ttm12", "中性CP", Kind::Factor, Axis::CrossSection, cp_ttm12_deps, nullptr,
    &cs_cp_ttm12, /*must_be_finite=*/true,
    /*formula=*/
    "CP = 1 / pcf_raw; pct_rank(z(neutralize(winsorize_quantile(CP)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—; pcf_raw == 0 → CP 记 NaN"};

inline void cs_cp_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(pcf_raw_spec, d, b.a);
  for (float &v : b.a) {
    if (!is_finite(v) || v == 0.0f)
      v = std::nanf("");
    else
      v = 1.0f / v;
  }
  neutral_pipeline(d, cp_ttm12_spec, T, b);
}

} // namespace feature::def
