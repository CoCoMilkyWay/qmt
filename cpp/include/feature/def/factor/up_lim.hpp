#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstddef>

// up_lim 主动 -1: limit_price (CUTOFF=-1) 取的是 row D 当日适用涨跌停
//   (基于 D-1 close × 1.1/1.2 来); close_raw[D] (CUTOFF=-1) = D-1 实际收盘.
//   想判 "D-1 日是否封板" 应用 D-1 适用涨跌停, 即 limit_price[a, d-1]. 主动 -1
//   完成此对齐. d=0 处无前一交易日 → NaN.

namespace feature::def {

inline void ts_up_lim(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec up_lim_spec{
    "up_lim", Kind::Inter, Axis::TimeSeries, {}, &ts_up_lim, nullptr};

inline void ts_up_lim(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(up_lim_spec, a);
  const auto &src = pool.limit_price.upper_limit;
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d - 1)];
  }
  detail::fill_before_list(out, a, axes, meta);
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
