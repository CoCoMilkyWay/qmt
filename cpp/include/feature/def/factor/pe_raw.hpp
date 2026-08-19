#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// pe_raw = mcap_raw / ttm.net_profit_to_parent_shareholders_ttm
//   支持负 PE (亏损); mcap<=0 是未上市/无价格哨兵; n==0 → NaN.

namespace feature::def {

inline void ts_pe_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *pe_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec pe_raw_spec{
    "pe_raw", Kind::Inter, Axis::TimeSeries, pe_raw_deps, &ts_pe_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/
    "mcap_raw / ttm.net_profit_to_parent_shareholders_ttm (取 shift=0 latest "
    "visible)",
    /*assumption=*/
    "[ratio]; ttm12; 支持负 PE (亏损不剔); shift=0 行 = 该 visible_date 的 "
    "最新报告期, 沿 v 单调推进; 分母 == 0 → NaN"};

inline void ts_pe_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(mcap_raw_spec, a);
  detail::scan_latest_ttm(
      a, axes, pool, meta, T, pe_raw_spec,
      [&](int d, const FinancialTtmEv &e) -> float {
        float m = mcap[d];
        float n = e.net_profit_to_parent_shareholders_ttm;
        return (is_finite(m) && m > 0.0f && is_finite(n) && n != 0.0f)
                   ? m / n
                   : std::nanf("");
      });
}

} // namespace feature::def
