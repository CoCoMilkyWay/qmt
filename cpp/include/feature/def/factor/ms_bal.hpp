#pragma once

#include "feature/cs.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/factor/ms_bal_raw.hpp"
#include "feature/graph.hpp"

// factor: ms_bal — 融券余额截面排名. pct_rank(z(winsor_mad(ms_bal_raw))) + 截面均值填充.
//   值越大表示融券余额越高. 仅供对账诊断策略引用, 把孤儿 raw ms_bal_raw 拉入计算图;
//   非交易因子, 不参与实盘选股.

namespace feature::def {

inline void cs_ms_bal(int d, const Axes &, Tensor &T, CsBufs &b);

inline constexpr const FeatureSpec *ms_bal_deps[] = {
    &ms_bal_raw_spec, &list_age_spec, &delist_age_spec};

inline constexpr FeatureSpec ms_bal_spec{
    "ms_bal", "融券余额因子", Kind::Factor, Axis::CrossSection, ms_bal_deps, nullptr, &cs_ms_bal,
    /*must_be_finite=*/true,
    /*formula=*/"pct_rank(z(winsor_mad(ms_bal_raw))) + 截面均值填充",
    /*assumption=*/"—; 值越大融券余额越高; 仅供对账诊断, 非交易因子"};

inline void cs_ms_bal(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(ms_bal_raw_spec, d, b.a);
  factor_pipeline(d, ms_bal_spec, T, b);
}

} // namespace feature::def
