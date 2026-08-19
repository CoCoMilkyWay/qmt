#pragma once

#include "feature/axis.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>
#include <cstddef>

// daily_return: 后复权 close 链式日收益 (含分红再投入的真持有收益).
//   tensor 顶层 close_raw 是不复权真价 (mcap / limit / low_p 等需要真值);
//   "复权"是 daily_return 自身需要的内部细节 — 这里直接从 PitPool 读
//   {close, adjust_factor} 在内部叠出 close_hfq[d] = close[d] × adjust_factor[d],
//   再链式: ret[d] = close_hfq[d] / close_hfq[d-1] - 1.
//   除权日 close 真跳, adjust_factor 反向跳, close × af 平滑 ⇒ ret 仅含日内真实涨跌
//   (无除权日 -5%~-10% 假负跳; 与"持有不减仓"账户的真实收益对齐).
//   前复权不 causal (起点会随未来除权事件追溯调整, 历史回测会泄漏未来), 不采用.
//   d==0 或前一日 close/af 非 finite/0 → NaN.
//   下游 benchmark = pool 内等权 mean.

namespace feature::def {

inline void ts_daily_return(int a, const Axes &axes, const PitPool &pool,
                            const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec daily_return_spec{
    "daily_return", Kind::Inter, Axis::TimeSeries, {}, &ts_daily_return,
    nullptr};

inline void ts_daily_return(int a, const Axes &axes, const PitPool &pool,
                            const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const auto &cl = pool.bar1d.close;
  const auto &af = pool.bar1d.adjust_factor;
  auto out = T.ts_row(daily_return_spec, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    std::size_t i0 = base + static_cast<std::size_t>(d - 1);
    std::size_t i1 = base + static_cast<std::size_t>(d);
    float c0 = cl[i0], c1 = cl[i1];
    float a0 = af[i0], a1 = af[i1];
    if (is_finite(c0) && is_finite(c1) && is_finite(a0) && is_finite(a1) &&
        c0 != 0.0f && a0 != 0.0f) {
      float h0 = c0 * a0;
      float h1 = c1 * a1;
      out[d] = (h0 != 0.0f) ? (h1 / h0 - 1.0f) : std::nanf("");
    } else {
      out[d] = std::nanf("");
    }
  }
}

} // namespace feature::def
