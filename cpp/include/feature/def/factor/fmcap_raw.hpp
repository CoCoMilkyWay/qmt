#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstddef>

// fmcap_raw = close_raw[d] × shares.a_float_shares[a, d]  ([元])
//   BigQuant `float_market_cap` 实测口径 = close × a_float (A 股流通);
//   total_float 含 H 股, 002594/BYD 等 H+A 双重上市股大幅偏差.

namespace feature::def {

inline void ts_fmcap_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *fmcap_raw_deps[] = {&close_raw_spec};

inline constexpr FeatureSpec fmcap_raw_spec{
    "fmcap_raw", Kind::Inter, Axis::TimeSeries, fmcap_raw_deps, &ts_fmcap_raw,
    nullptr};

inline void ts_fmcap_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(close_raw_spec, a);
  auto out = T.ts_row(fmcap_raw_spec, a);
  const auto &sh = pool.shares.a_float_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  detail::fill_before_list(out, a, axes, meta);
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
