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
//   - tradable_nav.npy       [n_d_bt] float32 (tradable 内等权 benchmark, 起点同)
//   - position_count.npy     [n_d_bt] int32  (持仓股票数)
//   - position_pct.npy       [n_d_bt] float32 (持仓市值 / 组合市值)
//   - turnover.npy           [n_d_bt] float32 (双边: 当日买卖额 / 2 / 组合市值;
//                              1 个成分股满额换手 = 1/HOLD_N, 再平衡碎股按额计)
//   - susp_pct.npy           [n_d_bt] float32 (持仓中停牌比例)
//   - executable_pct.npy     [n_d_bt] float32 (可执行订单 / 想下订单)
//   - holdings_offsets.npy   [n_d_bt+1] int32 (CSR offset)
//   - holdings_codes.npy     [total]    int32 (a 索引; per-d 内按权重降序)
//   - holdings_weights.npy   [total]    float32 (持仓占组合市值)
//   - watch_offsets.npy      [n_d_bt+1] int32 (CSR offset)
//   - watch_codes.npy        [total]    int32 (a 索引; per-d 内因子降序,
//                              长度 = min(HOLD_N*2, n_cands))
//   - watch_scores.npy       [total]    float32 (当日 factor_score)
//   - watch_rank_chg.npy     [total]    float32 (5 日 rank 均线 − 当日 rank;
//                              + = 排名上升, 单位=名次)
//   - trades_inst.npy        [n_trades] int32  (a 索引)
//   - trades_open_d.npy      [n_trades] int32
//   - trades_close_d.npy     [n_trades] int32
//   - trades_open_px.npy     [n_trades] float32
//   - trades_close_px.npy    [n_trades] float32
//   - fills_d.npy            [n_fills]  int32  (axes D; 正式调仓逐笔)
//   - fills_a.npy            [n_fills]  int32
//   - fills_side.npy         [n_fills]  int32  (+1 买 / -1 卖; 含 pop 补槽)
//   - fills_px.npy           [n_fills]  float32
//   - labels.json            JSON {trades_open_names[],
//                                   trades_close_names[],
//                                   holdings_names[],
//                                   watch_names[],
//                                   fills_names[]}
//
// 同时返回 elapsed_seconds (写入 meta.json).
double run(const feature::Axes &axes, const feature::StockMeta &meta,
           const feature::Tensor &T);

} // namespace backtest
