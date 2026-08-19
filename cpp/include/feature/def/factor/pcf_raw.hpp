#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// pcf_raw = mcap_raw / ttm.net_cffoa_ttm; 经营性现金流可负, 不剔;
//   mcap<=0 是未上市/无价格哨兵; c==0 → NaN.

namespace feature::def {

inline void ts_pcf_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *pcf_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec pcf_raw_spec{
    "pcf_raw", Kind::Inter, Axis::TimeSeries, pcf_raw_deps, &ts_pcf_raw,
    nullptr, /*must_be_finite=*/false,
    /*formula=*/"mcap_raw / ttm.net_cffoa_ttm (shift=0 latest visible)",
    /*assumption=*/
    "[ratio]; ttm12; 经营现金流可负 (烧钱企业 → 负 PCF), 不剔; 分母 == 0 → NaN"};

inline void ts_pcf_raw(int a, const Axes &axes, const PitPool &pool,
                       const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(mcap_raw_spec, a);
  detail::scan_latest_ttm(
      a, axes, pool, meta, T, pcf_raw_spec,
      [&](int d, const FinancialTtmEv &e) -> float {
        float m = mcap[d];
        float c = e.net_cffoa_ttm;
        return (is_finite(m) && m > 0.0f && is_finite(c) && c != 0.0f)
                   ? m / c
                   : std::nanf("");
      });
}

} // namespace feature::def
