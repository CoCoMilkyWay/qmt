#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"
#include "strategy/strategy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace backtest {

// 历史简称时间线 (报表标签用; 全策略共享, main 加载一次).
struct NameInterval {
  int lo;
  int hi;
  std::string name;
};

struct NameTimeline {
  std::vector<std::vector<NameInterval>> by_a;
};

NameTimeline load_name_timeline(const feature::Axes &);

// per-a 末日简称 (axes.dates.back() 那天生效的那个), 落 meta.json 的 names[].
//   与 codes[] 同序同长; 全策略共享 ⇒ 报告里任何"a 索引 → 人看的名字"都查这里.
std::vector<std::string> last_names(const feature::Axes &,
                                    const feature::StockMeta &,
                                    const NameTimeline &);

// per-strategy 回测结果 — 跨策略聚合层 (report/aggregate) 的输入.
//   只带聚合真正要用的三样东西 (净值 / 持仓 CSR / 窗口左端), 让 aggregate 不必
//   反过来重读自己刚写出的 npy.
struct Result {
  double elapsed_seconds = 0.0;
  double analysis_seconds = 0.0;        // analysis::run 计时, main 循环里回填
  int d_lo = 0;                         // 窗口左端 (axes D 全局索引)
  std::vector<float> strategy_nav;      // [n_d_bt]
  std::vector<float> pool_nav;          // [n_d_bt] 该策略自己的 pool 等权影子指数
                                        //   (与 strategy_nav 同窗口); aggregate
                                        //   层拿它当该策略专属的"基准" (见
                                        //   report/aggregate.hpp).
  std::vector<std::int32_t> hold_off;   // [n_d_bt + 1] CSR offset
  std::vector<std::int32_t> hold_codes; // [total] a 索引
  std::vector<float> hold_weights;      // [total] 占组合市值
};

// Per-D 走 D 的回测器 (单线程, 状态强 causal). Per-strategy: spec 提供窗口
// (bt_start_date, 右端点固定 axes 最新日) / hold_n / exit_ratio; s_idx 是
// strategy::STRATEGIES[] 下标 (定位 Tensor 策略块).
//
// 输入 (只读):
//   - axes / meta / T (共享: close_raw, daily_return, susp, limit_up, limit_dn;
//     策略块: pool, score, rank 已就绪) / name_timeline
//
// 配置 (来自 config.hpp, 券商账户属性, 全策略共享):
//   - BACKTEST_CAPITAL_BASE
//   - BACKTEST_BUY_COST / BACKTEST_SELL_COST / BACKTEST_MIN_COST / BACKTEST_PRICE_LIMIT_EPS
//
// 候选集从策略 rank 列直读 (1-based 降序排名, 0 = 不在母集) — 回测 top-N /
// exit-N·ratio / watch-2N 与实盘选股读同一列, "回测 = 实盘" 收敛到单一入口.
//
// 输出 (写到 <git_root>/output/strategy/<name>/backtest/):
//   序列 / 明细走 npy, 指标 / 表格走 report.json, 标的名字符串走 labels.json.
//   - dates.npy              [n_d_bt] int32  (axes.dates 全局索引)
//   - strategy_nav.npy       [n_d_bt] float32 (策略净值, 起点 = BACKTEST_CAPITAL_BASE)
//   - strategy_dd.npy        [n_d_bt] float32 (策略回撤曲线, ≤ 0)
//   position_count.npy 以下: pool 基准净值仍在内部算 (供 rel_stats
//   跟踪误差/IR/Beta/Alpha 与"超额"行用), 但不再落盘/展示 (无 pool_nav.npy /
//   pool_dd.npy).
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
//   - watch_scores.npy       [total]    float32 (当日策略 score)
//   - watch_rank_chg.npy     [total]    float32 (5 日 rank 均线 − 当日 rank;
//                              + = 排名上升, 单位=名次)
//   以下 3 列让前端 hover 成为纯格式化 (逐日 watch 明细面板所需的全部状态,
//   不必在 JS 里从 holdings / fills 反推):
//   - watch_hold_w.npy       [total]    float32 (该 a 当日持仓权重; NaN = 未持仓)
//   - watch_hold_days.npy    [total]    int32   (已连续持有天数; 0 = 未持仓)
//   - watch_bought.npy       [total]    int32   (1 = 当日正式买入)
//   - trades_inst.npy        [n_trades] int32  (a 索引)
//   - trades_open_d.npy      [n_trades] int32
//   - trades_close_d.npy     [n_trades] int32
//   - trades_open_px.npy     [n_trades] float32
//   - trades_close_px.npy    [n_trades] float32
//   - fills_d.npy            [n_fills]  int32  (axes D; 正式调仓逐笔)
//   - fills_a.npy            [n_fills]  int32
//   - fills_side.npy         [n_fills]  int32  (+1 买 / -1 卖; 含 pop 补槽)
//   - fills_px.npy           [n_fills]  float32
//   - fills_weight.npy       [n_fills]  float32 (买: 成交当日收盘权重;
//                              卖: 卖出前一日权重)
//   - fills_pnl.npy          [n_fills]  float32 (卖: 本笔 trade 收益率 %;
//                              买: NaN)
//   - labels.json            JSON {trades_open_names[], trades_close_names[],
//                                   holdings_names[], watch_names[],
//                                   fills_names[]}
//   - report.json            JSON 指标 + 表格 (py 侧零计算):
//                              indicators{策略/超额} (pool 净值不单
//                                独展示行, 只作"超额"差值与 rel_stats 算子)
//                              trade_stats{16 项}
//                              annual{} / monthly{} 列式期次表
//                              holdings{} 末日持仓表 (a 索引 → code/简称/行业由
//                                py 用 meta.json 的 codes/names/industries 查)
//
// 返回 Result (elapsed_seconds 进 meta.json; 其余给 report/aggregate).
Result run(const feature::Axes &axes, const feature::StockMeta &meta,
           const feature::Tensor &T, const NameTimeline &name_timeline,
           const strategy::StrategySpec &spec, int s_idx);

} // namespace backtest
