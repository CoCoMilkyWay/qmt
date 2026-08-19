#pragma once

#include "feature/cs.hpp"
#include "feature/def/factor/cffoa_raw.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/graph.hpp"

// factor: cffoa_ttm12 — 经营现金流量净额增长因子: 不做行业/市值中性化 (增长率
//   本身跨行业量级差异已由 winsor_mad + z 缩尾/标准化处理), 走简单
//   factor_pipeline (同 close/mcap); 无 invert.

namespace feature::def {

inline void cs_cffoa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *cffoa_ttm12_deps[] = {
    &cffoa_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec cffoa_ttm12_spec{
    "cffoa_ttm12", "现金流增速因子", Kind::Factor, Axis::CrossSection, cffoa_ttm12_deps, nullptr,
    &cs_cffoa_ttm12, /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(cffoa_raw))) + 截面均值填充",
    /*assumption=*/
    "—; 经营现金流量净额增长因子, 不做行业/市值中性化 (增长率本身跨行业量级差异 "
    "已由 winsor_mad + z 缩尾/标准化处理), 走简单 factor_pipeline (同 close/mcap); "
    "无 invert"};

inline void cs_cffoa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, cffoa_raw_spec, cffoa_ttm12_spec, /*invert=*/false, T, b);
}

} // namespace feature::def
