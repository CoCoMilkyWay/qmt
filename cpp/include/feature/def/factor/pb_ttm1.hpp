#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/pb_raw.hpp"
#include "feature/graph.hpp"

// factor: pb_ttm1 — 中性 BP (瞬时估值 / MRQ). pct_rank(z(neutralize(
//   winsorize_quantile(1 / pb_raw)))) + 截面均值填充.

namespace feature::def {

inline void cs_pb_ttm1(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *pb_ttm1_deps[] = {
    &pb_raw_spec, &mcap_raw_spec, &industry_l1_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec pb_ttm1_spec{
    "pb_ttm1", Kind::Factor, Axis::CrossSection, pb_ttm1_deps, nullptr,
    &cs_pb_ttm1, /*must_be_finite=*/true,
    /*formula=*/
    "pct_rank(z(neutralize(winsorize_quantile(1 / pb_raw)))) + 截面均值填充; "
    "中性化 = 行业+log(mcap) OLS 残差",
    /*assumption=*/"—"};

inline void cs_pb_ttm1(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, pb_raw_spec, pb_ttm1_spec, /*invert=*/true, T, b);
}

} // namespace feature::def
