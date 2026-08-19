#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/basic/close_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstddef>

// mcap_raw = close_raw[d] × shares.total_shares[a, d]  ([元])
//   close_raw 即不复权 close (← cn_stock_real_bar1d.close), 与 total_shares 同口径,
//   mcap_raw = 真实总市值 [元]; cross-section 排序与 low_mc 阈值判定均正确.

namespace feature::def {

inline void ts_mcap_raw(int a, const Axes &axes, const PitPool &pool,
                        const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *mcap_raw_deps[] = {&close_raw_spec};

inline constexpr FeatureSpec mcap_raw_spec{
    "mcap_raw", Kind::Inter, Axis::TimeSeries, mcap_raw_deps, &ts_mcap_raw,
    nullptr};

inline void ts_mcap_raw(int a, const Axes &axes, const PitPool &pool,
                        const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(close_raw_spec, a);
  auto out = T.ts_row(mcap_raw_spec, a);
  const auto &sh = pool.shares.total_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  detail::fill_before_list(out, a, axes, meta);
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
