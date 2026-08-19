#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/cffoa_raw.hpp"
#include "feature/graph.hpp"

// factor: cffoa_ttm12 — 经营现金流改善因子. pct_rank(z(winsor_mad(cffoa_raw)))
//   + 截面均值填充. 值越大表示现金流相对市值改善越多.
//   不做行业/市值中性化, 走简单 factor_pipeline (同 close/mcap).

namespace feature::def {

inline void cs_cffoa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *cffoa_ttm12_deps[] = {
    &cffoa_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec cffoa_ttm12_spec{
    "cffoa_ttm12", "现金流改善因子", Kind::Factor, Axis::CrossSection, cffoa_ttm12_deps, nullptr,
    &cs_cffoa_ttm12, /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(cffoa_raw))) + 截面均值填充",
    /*assumption=*/
    "—; 值越大现金流相对市值改善越多; cffoa_raw 已按市值定标 + tanh 封顶 "
    "(∈[-1,1], 尺度可比), 不做行业/市值中性化, 走简单 factor_pipeline (同 close/mcap)"};

inline void cs_cffoa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(cffoa_raw_spec, d, b.a);
  factor_pipeline(d, cffoa_ttm12_spec, T, b);
}

} // namespace feature::def
