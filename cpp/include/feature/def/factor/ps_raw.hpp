#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// ps_raw = mcap_raw / ttm.total_operating_revenue_ttm
//   分母用 total_operating_revenue_ttm (含利息/保费; BigQuant 实测口径) 而非
//   operating_revenue_ttm; 600519 茅台等 2% 误差排查得.
//   分母 <= 0 → NaN: 营收为负物理不可能, 是 BigQuant 脏值 (实测 1.35% 事件为负,
//   含 208 条已上市多年的, 例 600606 借壳期 rev_ttm = -312 亿), 不给排序含义.
//   mcap<=0 是未上市/无价格哨兵, 不参与估值.

namespace feature::def {

inline void ts_ps_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *ps_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec ps_raw_spec{
    "ps_raw", Kind::Inter, Axis::TimeSeries, ps_raw_deps, &ts_ps_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/
    "mcap_raw / ttm.total_operating_revenue_ttm (shift=0 latest visible)",
    /*assumption=*/
    "[ratio]; ttm12; 用 total_operating_revenue_ttm (含利息/保费, ≠ "
    "operating_revenue_ttm); 分母 ≤ 0 → NaN (负营收是源数据脏值, 不给排序含义)"};

inline void ts_ps_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(mcap_raw_spec, a);
  detail::scan_latest_ttm(
      a, axes, pool, meta, T, ps_raw_spec,
      [&](int d, const FinancialTtmEv &e) -> float {
        float m = mcap[d];
        float r = e.total_operating_revenue_ttm;
        return (is_finite(m) && m > 0.0f && is_finite(r) && r > 0.0f)
                   ? m / r
                   : std::nanf("");
      });
}

} // namespace feature::def
