#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/ms_bal_raw.hpp"
#include "feature/graph.hpp"

// factor: ms_bal — 融券余额截面排名. pct_rank(z(winsor_mad(ms_bal_raw))) + 截面均值填充.
//   值越大表示融券余额越高. 由 mine::MINE_FACTORS 引用 (顺带把 ms_bal_raw 拉入
//   计算图); 尚未进入任何策略的 weights.

namespace feature::def {

inline void cs_ms_bal(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *ms_bal_deps[] = {
    &ms_bal_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec ms_bal_spec{
    "ms_bal", "融券余额因子", Kind::Factor, Axis::CrossSection, ms_bal_deps, nullptr, &cs_ms_bal,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(ms_bal_raw))) + 截面均值填充",
    /*assumption=*/"—; 值越大融券余额越高; 目前只在 mine 候选池里"};

inline void cs_ms_bal(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(ms_bal_raw_spec, d, b.a);
  factor_pipeline(d, ms_bal_spec, T, b);
}

} // namespace feature::def
