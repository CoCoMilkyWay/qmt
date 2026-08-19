#pragma once

#include "feature/axis.hpp"
#include "feature/def/factor/low_mc.hpp"
#include "feature/def/factor/low_p.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>

// filter: trading_st — rolling 15D (low_p ∨ low_mc).all() 交易类退市预警.
//   交易所规则是连续 20 个交易日收盘价 < 1 元才构成交易类退市 (面值退市),
//   但实测停牌与该 20 日阈值同日触发 (第 20 日当天已停牌, 无法再卖出) ⇒ 20
//   日口径对策略是零缓冲的"事后确认", 起不到预警作用. 提前到 15 日 (缓冲 5
//   个交易日) 换取真正可执行的退出窗口, 代价是可能提前卖出后续回升未退市的
//   正常低价股 (更高误伤率换更早离场).

namespace feature::def {

inline void ts_trading_st(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *trading_st_deps[] = {&low_p_spec,
                                                         &low_mc_spec};

inline constexpr FeatureSpec trading_st_spec{
    "trading_st", "交易类退市预警", Kind::Filter, Axis::TimeSeries, trading_st_deps,
    &ts_trading_st, nullptr, /*must_be_finite=*/true,
    /*formula=*/"rolling_15D(low_p ∨ low_mc).all()",
    /*assumption=*/"提前于交易所 20 日退市线 5 个交易日预警, 换可执行退出窗口"};

// trading_st: rolling 15D (low_p ∨ low_mc).all(). 单调连续计数即可.
inline void ts_trading_st(int a, const Axes &axes, const PitPool &,
                          const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(trading_st_spec, a);
  auto low_p = T.ts_row(low_p_spec, a);
  auto low_mc = T.ts_row(low_mc_spec, a);
  constexpr int W = 15;
  std::fill(out.begin(), out.end(), 0.0f);
  int run = 0;
  for (int d = 0; d < n_d; ++d) {
    bool ok = (low_p[d] > 0.5f) || (low_mc[d] > 0.5f);
    run = ok ? run + 1 : 0;
    out[d] = (run >= W) ? 1.0f : 0.0f;
  }
}

} // namespace feature::def
