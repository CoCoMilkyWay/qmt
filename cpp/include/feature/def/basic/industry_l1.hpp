#pragma once

#include "feature/axis.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cstdint>

// industry_l1: SW2021 一级行业 ID per (D, A), 0=未知, 1..31 见 industry.hpp.
//   合并 industry_component (月初快照) + industry_change (月内 change_flag=1 进入)
//   两个事件流, per-A 按 v 升序回放, last_l1_id 写每行. 上市前/无事件期保持 0.
//   退市后 last_l1_id 残留 (策略 pool_b 已用 ¬is_finite(delist_age) 兜底排除, 不影响下游).

namespace feature::def {

inline void ts_industry_l1(int a, const Axes &axes, const PitPool &pool,
                           const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec industry_l1_spec{
    "industry_l1", Kind::Inter, Axis::TimeSeries, {}, &ts_industry_l1, nullptr};

inline void ts_industry_l1(int a, const Axes &axes, const PitPool &pool,
                           const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(industry_l1_spec, a);
  std::fill(out.begin(), out.end(), 0.0f);

  const auto &comp = pool.industry_component[a];
  const auto &chg = pool.industry_change[a];
  std::size_t ic = 0, ig = 0;
  uint8_t last_id = 0;

  for (int d = 0; d < n_d; ++d) {
    while (ic < comp.size() && comp[ic].v <= d) {
      last_id = comp[ic].l1_id;
      ++ic;
    }
    while (ig < chg.size() && chg[ig].v <= d) {
      last_id = chg[ig].l1_id;
      ++ig;
    }
    out[d] = static_cast<float>(last_id);
  }
}

} // namespace feature::def
