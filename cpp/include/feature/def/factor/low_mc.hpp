#pragma once

#include "feature/axis.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

// low_mc: 主板阈值 5e8, 非主板 3e8. mb 判定 = meta.list_sector[a]==MAIN_BOARD (asset 静态).
//   list_sector 为中文 (与 exchange 同口径): 主板/创业板/科创板/北交所/未知.

namespace feature::def {

inline void ts_low_mc(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *low_mc_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec low_mc_spec{
    "low_mc", "低市值标记", Kind::Inter, Axis::TimeSeries, low_mc_deps, &ts_low_mc, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/"mcap_raw < (5e8 if meta.list_sector == \"主板\" else 3e8)",
    /*assumption=*/"[bool]; 主板判定 inline meta.list_sector[a] == MAIN_BOARD"};

inline void ts_low_mc(int a, const Axes &axes, const PitPool &,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto mc = T.ts_row(mcap_raw_spec, a);
  auto out = T.ts_row(low_mc_spec, a);
  float thr = (meta.list_sector[a] == MAIN_BOARD) ? 5e8f : 3e8f;
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(mc[d]) && mc[d] > 0.0f && mc[d] < thr) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
