#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstddef>

// dn_lim 主动 -1: 同 up_lim, 对齐下跌停. d=0 处无前一交易日 → NaN.

namespace feature::def {

inline void ts_dn_lim(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec dn_lim_spec{
    "dn_lim", "跌停价", Kind::Inter, Axis::TimeSeries, {}, &ts_dn_lim, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"cn_stock_limit_price.lower_limit[d-1] (未复权, 内部主动 -1)",
    /*assumption=*/
    "[元/股]; close_raw[D] 是 D-1 收盘, 判\"D-1 是否封板\"须配 D-1 适用的 "
    "涨跌停, 故内部再取 [d-1]. d==0 → NaN"};

inline void ts_dn_lim(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(dn_lim_spec, a);
  const auto &src = pool.limit_price.lower_limit;
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d - 1)];
  }
  detail::fill_before_list(out, a, axes, meta);
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
