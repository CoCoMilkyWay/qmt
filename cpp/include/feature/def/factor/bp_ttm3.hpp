#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pb_raw.hpp"
#include "feature/graph.hpp"

#include <cmath>

// factor: bp_ttm3 — 中性 BP (账面市值比 = 1 / pb_raw; 瞬时估值 / MRQ).
//   pct_rank(z(neutralize(winsorize_quantile(BP)))) + 截面均值填充.

namespace feature::def {

inline void cs_bp_ttm3(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *bp_ttm3_deps[] = {
    &pb_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec bp_ttm3_spec{
    "bp_ttm3", "中性BP", Kind::Factor, Axis::CrossSection, bp_ttm3_deps, nullptr,
    &cs_bp_ttm3, /*must_be_finite=*/true,
    /*formula=*/
    "BP = 1 / pb_raw; pct_rank(z(neutralize(winsorize_quantile(BP)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—; pb_raw == 0 → BP 记 NaN"};

inline void cs_bp_ttm3(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(pb_raw_spec, d, b.a);
  for (float &v : b.a) {
    if (!is_finite(v) || v == 0.0f)
      v = std::nanf("");
    else
      v = 1.0f / v;
  }
  neutral_pipeline(d, bp_ttm3_spec, T, b);
}

} // namespace feature::def
