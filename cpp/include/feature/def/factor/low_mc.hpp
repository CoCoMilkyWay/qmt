#pragma once

#include "feature/axis.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// low_mc: 主板阈值 5e8, 非主板 3e8. mb 判定 = meta.list_sector[a]==1 (asset 静态).
//   list_sector 编码: 1=主板, 2=创业板, 3=科创板, 4=北交所, 0=未知.

namespace feature::def {

inline void ts_low_mc(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *low_mc_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec low_mc_spec{
    "low_mc", "低市值标记", Kind::Inter, Axis::TimeSeries, low_mc_deps, &ts_low_mc, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/"mcap_raw < (5e8 if meta.list_sector == 1 else 3e8)",
    /*assumption=*/"[bool]; 主板判定 inline meta.list_sector[a] == 1"};

inline void ts_low_mc(int a, const Axes &axes, const PitPool &,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto mc = T.ts_row(mcap_raw_spec, a);
  auto out = T.ts_row(low_mc_spec, a);
  float thr = (meta.list_sector[a] == 1) ? 5e8f : 3e8f;
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(mc[d]) && mc[d] > 0.0f && mc[d] < thr) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
