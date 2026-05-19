#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"

namespace backtest {

// Per-D 走 D 的回测器 (单线程, 状态强 causal).
//
// 输入 (只读):
//   - axes / meta / T (factor_score, tradable, close_raw, daily_return, susp,
//     limit_up, limit_dn, pool 已就绪)
//
// 配置 (来自 config.hpp):
//   - BACKTEST_START_DATE (右端点固定为 axes 最新日)
//   - BACKTEST_HOLD_N / BACKTEST_EXIT_RATIO / BACKTEST_CAPITAL_BASE
//   - BACKTEST_BUY_COST / BACKTEST_SELL_COST / BACKTEST_MIN_COST / BACKTEST_PRICE_LIMIT_EPS
//
// 输出 (写到 <git_root>/output/backtest/):
//   - dates.npy              [n_d_bt] int32  (axes.dates 全局索引)
//   - strategy_nav.npy       [n_d_bt] float32 (策略净值, 起点 = BACKTEST_CAPITAL_BASE)
//   - pool_nav.npy           [n_d_bt] float32 (pool 内等权 benchmark, 起点同)
//   - position_count.npy     [n_d_bt] int32  (持仓股票数)
//   - position_pct.npy       [n_d_bt] float32 (持仓市值 / 组合市值)
//   - turnover.npy           [n_d_bt] float32 (当日 卖+买 / HOLD_N)
//   - susp_pct.npy           [n_d_bt] float32 (持仓中停牌比例)
//   - executable_pct.npy     [n_d_bt] float32 (可执行订单 / 想下订单)
//   - holdings_offsets.npy   [n_d_bt+1] int32 (CSR offset)
//   - holdings_codes.npy     [total]    int32 (a 索引; per-d 内按权重降序)
//   - holdings_weights.npy   [total]    float32 (持仓占组合市值)
//   - trades_inst.npy        [n_trades] int32  (a 索引)
//   - trades_open_d.npy      [n_trades] int32
//   - trades_close_d.npy     [n_trades] int32
//   - trades_open_px.npy     [n_trades] float32
//   - trades_close_px.npy    [n_trades] float32
//   - labels.json            JSON {trades_open_names[],
//                                   trades_close_names[],
//                                   holdings_names[]}  // 当日历史简称
//
// 同时返回 elapsed_seconds (写入 meta.json).
double run(const feature::Axes &axes, const feature::StockMeta &meta,
           const feature::Tensor &T);

} // namespace backtest
