#pragma once

#include "backtest/backtest.hpp"
#include "feature/axis.hpp"
#include "feature/tensor.hpp"

#include <span>
#include <string_view>

// ============================================================================
// 因子权重挖掘 — 在带符号 simplex lattice 上穷举权重方向, 每个权重跑一遍**真
// 回测**, 落 per-weight 指标. 配置见 mine/spec.hpp.
//
// 为什么挖出来的权重填回 cpp 能对上 (不是"调到 99%", 是结构上同一份代码):
//   1. 选股口径: 每日 score = Σ w·pct_rank_pool(factor) / Σ|w|, 与
//      strategy/columns.cpp::cs_score 逐位同构 — pool 内分位与权重无关, 故
//      预计算一次 (PR 矩阵), 逐权重只剩一次点积.
//   2. 排名口径: (score 降序, 并列 a 升序), 与 cs_rank 一致.
//   3. 回测口径: 直接调 backtest/engine.hpp 的 Engine — 与 backtest::run 同一
//      份决策/记账实现 (现金 / 分数股 / 买卖成本 / target_per_slot 再平衡 /
//      涨跌停开关舱约束 / 停牌挡卖 / 退市强平), 只是 Recorder 换成 NullRecorder.
//   4. 指标口径: report::nav_stats — 与报告同一个 CAGR / 夏普公式.
//   5. 自检: 拿目标策略**自己的** weights 走一遍本管线, 与 backtest::run 的
//      strategy_nav 对账 (逐点相对误差), 不过关直接 assert fail.
//
// 效率要点 (n=13 因子, M=8 → 6.1e6 个权重的量级):
//   - PR 矩阵一次性算好, 因子主序 (per-day per-factor 连续) ⇒ 点积是 F 遍
//     向量化 axpy; 全体线程只读共享 (n_d_bt × 池均 × F × 4B ≈ 285 MB)
//   - 每天只取前 max(hold_n, hold_n×exit_ratio) 名: 内核的买入循环在 rank
//     > hold_n 处 break, 卖出只查 top-exit 前缀 ⇒ 全排序纯属浪费.
//     阈值预筛 + 定长插入选择, 绝大多数候选一次比较就被拒
//   - 市场状态 (last_close / 停牌 / 涨跌停 / 退市) 压成窗口矩阵, 跨权重共享
//   - lattice 按下标分块, std::thread + 原子游标动态调度
//
// 输出 (<git_root>/output/mine/):
//   - k_grid.npy   [P, n] int32   lattice 整数坐标 k (权重 w = k / MINE_LATTICE_M);
//                                 列序 = mine::MINE_FACTORS 顺序
//   - metrics.npy  [P, m] float32 per-weight 指标, 列序 = MINE_METRIC_NAMES
//   - meta.json    JSON {strategy, lattice_m, n_points, factor_names[],
//                        metric_names[], window{start, end, n_days},
//                        selfcheck{covered, max_rel_diff}}
// py/app/mine.py 直读这三样做后处理 (邻域平原度 / 组合精选 / 打印), 不重复计算.
// ============================================================================
namespace mine {

// metrics.npy 的列 (顺序即列序; py 侧按 meta.json::metric_names 对齐, 不硬编码).
inline constexpr std::string_view MINE_METRIC_NAMES[] = {
    "年化",     // report::NavStats::ann_return (CAGR, 252 日折算)
    "夏普",     // report::NavStats::sharpe (rf=0, population std)
    "波动率",   // report::NavStats::ann_vol
    "最大回撤", // report::NavStats::max_drawdown (≤ 0)
    "NAV倍数",  // nav.back() / nav.front()
    "年换手率", // mean(双边换手) × 252
    "创新高最长天数",
};

inline constexpr int MINE_N_METRICS =
    static_cast<int>(sizeof(MINE_METRIC_NAMES) / sizeof(MINE_METRIC_NAMES[0]));

// 挖掘入口. results 是 main 里 per-strategy backtest 的结果 (自检要拿目标策略
//   的 strategy_nav 对账), 下标与 strategy::STRATEGIES[] 对齐.
//   返回 elapsed_seconds.
double run(const feature::Axes &axes, const feature::Tensor &T,
           std::span<const backtest::Result> results);

} // namespace mine
