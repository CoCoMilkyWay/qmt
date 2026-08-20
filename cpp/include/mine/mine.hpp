#pragma once

#include "backtest/backtest.hpp"
#include "feature/axis.hpp"
#include "feature/tensor.hpp"

#include <span>
#include <string_view>

// ============================================================================
// 因子权重挖掘 — 四段式. 配置见 mine/spec.hpp.
//
// Phase 1 (全 lattice, 一遍日循环): 滑窗分层 + 滑窗夏普.
//   每日池内按 score 升序等分 n_bins 档 (n_bins = universe_size / hold_n ⇒
//   档宽恰 ≈ hold_n 只, **顶档就是策略实际持有的那 hold_n 只**; 档内成员逐日
//   重组、等权), 次日收益复利成档 NAV. 252 日窗 / 21 日步滑过整个回测期:
//     a) 分层 — 每窗口对 "N 档窗口 log 收益 vs 归一化档秩" 做 OLS:
//          b  = 斜率 = 最差档→最好档的拟合年化价差 (梳子的总幅度)
//          se = 斜率标准误 (档收益围绕直线的散布, 梳齿的乱度)
//          梳子分 S = b − 2·se (扣掉乱度后至少还剩多少; 可为负 = 连朝向都
//                               不敢保证). 值空间罚残差 ⇒ 小幅度局部乱秩几乎
//          不伤分; "只有头部动、中间一坨平" 被线性拟合重罚 — 头部过拟合型
//          结构性出局.
//     b) 夏普 — 顶档日收益的 Σr / Σr² 跟 NAV 一起月末快照, 任意窗口 O(1) 查
//          出 mean/std ⇒ 与 report::nav_stats 同口径 (简单日收益, population
//          std, rf=0, ×√252).
//   为什么两者必须同一遍算完: Phase 2 的邻域敏感性要查 ~120 个 1 跳邻居, 邻居
//   大多不在优选集里 — 只对幸存者算夏普会让邻居缺值, 缺值只能当"好"处理, 偏差
//   方向恰好偏向山尖, 敏感性就废了. 所以全格稠密.
//
// Phase 2 (全 lattice, 纯内存): 三个截面分数 → 总分.
//   u1 = pctrank(梳子均值)   分层质量
//   u2 = pctrank(夏普均值)   风险调整收益
//   u3 = pctrank(−敏感性)    邻域稳健性, 其中
//        敏感性 = mean_nb |u2(nb) − u2(k)| / E_null|U − u2(k)|,
//        E_null = (u2² + (1−u2)²) / 2 = 邻域纯噪声时该有的平均偏离 (闭式).
//        ⇒ 读数自带刻度: 1 = 与噪声无异, 0 = 完美平原, >1 = 真尖峰. 归一化不是
//        装饰 — 纯噪声下 E|Δ| 在 u2=0.5 处最小、两端最大, 不除掉就会仅因"身处
//        榜首边界"而多罚榜首点.
//   总分 = u1 · u2 · u3 (合取: 三项都得体面; 无权重可调).
//   1 跳邻居 = 把一个单位从某因子挪到另一因子 (Σ|k| = M 不变). 邻居定位不用
//   哈希/排序/内存: build_lattice 的枚举序本身是 lattice → [0,P) 的双射, 组合
//   序号 index(k) 由 N[i][rem] 计数表 (<1KB, 常驻 L1) O(n·M) 算出, 启动期
//   assert index(kgrid[r]) == r 自检.
//
// Phase 3 (前 MINE_DEDUP_CAND 名, 流式贪心): 持仓去重.
//   按总分降序逐个现算逐日 top-hold_n 持仓, 与**已接受集合**比平均逐日重合度;
//   ≥ 0.5 (平均半数持仓相同 ⇒ 同一风格) 则丢弃 (前者分更高), 否则接受.
//   只物化已接受集合的持仓, 候选算完即弃; 留几个风格由重合度自然决定.
//
// Phase 4 (最终名单, 几十个): 真回测.
//   直接调 backtest/engine.hpp 的 Engine — 与 backtest::run 同一份决策/记账
//   实现; 指标走 report::nav_stats. 自检: 拿目标策略自己的 weights 走一遍本
//   管线, 与 backtest::run 的 strategy_nav 逐点对账, 不过关 assert fail.
//
// 输出 (<git_root>/output/mine/):
//   - k_grid.npy        [P, n] int8    lattice 坐标 k (w = k / MINE_LATTICE_M);
//                                      列序 = mine::MINE_FACTORS 顺序
//   - point_metrics.npy [P, c] float32 Phase 1/2 全格指标, 列序 = MINE_POINT_METRIC_NAMES
//   - styles.npy        [S] int32      去重后的风格代表, k_grid 行下标, 总分降序
//   - bt_metrics.npy    [S, m] float32 Phase 4 回测指标, 列序 = MINE_BT_METRIC_NAMES
//   - windows.npy       [S, W, 4] f32  per-window 明细, 列序 = MINE_WINDOW_METRIC_NAMES
//   - meta.json         口径 / 列名 / 窗口起点 / 基线 (含其在全格中的百分位) / 自检
// py/app/mine.py 直读做后处理 (排序 / 打印可粘回 cpp 的 weights), 不重复计算.
// ============================================================================
namespace mine {

// point_metrics.npy 的列. 前 10 列 = Phase 1 跨窗口摘要 (~W 个滑窗),
//   后 5 列 = Phase 2 派生. py 侧按 meta 对齐, 不硬编码下标.
inline constexpr std::string_view MINE_POINT_METRIC_NAMES[] = {
    "梳子均值", // mean(S_w) — score1 的原始量
    "梳子IR",   // mean/std (population; std=0 时 0)
    "梳子p10",  // S_w 的 10% 分位 (下取整位次)
    "梳子胜率", // S_w > 0 的窗口占比
    "斜率均值", // mean(b_w) — 诊断: 幅度型还是干净型
    "R2均值",   // mean(R²_w) — 诊断
    "夏普均值", // mean(Sharpe_w) — score2 的原始量
    "夏普IR",   // mean/std
    "夏普p10",  //
    "夏普胜率", // Sharpe_w > 0 的窗口占比
    "分层分",   // u1 = pctrank(梳子均值)
    "夏普分",   // u2 = pctrank(夏普均值)
    "敏感性",   // 噪声归一后的邻域敏感性 (1 = 与噪声无异, 0 = 平原)
    "稳健分",   // u3 = pctrank(−敏感性)
    "总分",     // u1 · u2 · u3
};

inline constexpr int MINE_N_POINT_METRICS = static_cast<int>(
    sizeof(MINE_POINT_METRIC_NAMES) / sizeof(MINE_POINT_METRIC_NAMES[0]));

// windows.npy 最内维的列.
inline constexpr std::string_view MINE_WINDOW_METRIC_NAMES[] = {
    "梳子分", // S_w = b − 2·se
    "斜率",   // b_w (年化 log 价差)
    "R2",     // R²_w
    "夏普",   // 顶档窗口夏普
};

inline constexpr int MINE_N_WINDOW_METRICS = static_cast<int>(
    sizeof(MINE_WINDOW_METRIC_NAMES) / sizeof(MINE_WINDOW_METRIC_NAMES[0]));

// bt_metrics.npy 的列 (Phase 2, 与报告同一套公式).
inline constexpr std::string_view MINE_BT_METRIC_NAMES[] = {
    "年化",     // report::NavStats::ann_return (CAGR, 252 日折算)
    "夏普",     // report::NavStats::sharpe (rf=0, population std)
    "波动率",   // report::NavStats::ann_vol
    "最大回撤", // report::NavStats::max_drawdown (≤ 0)
    "NAV倍数",  // nav.back() / nav.front()
    "年换手率", // mean(双边换手) × 252
    "创新高最长天数",
};

inline constexpr int MINE_N_BT_METRICS = static_cast<int>(
    sizeof(MINE_BT_METRIC_NAMES) / sizeof(MINE_BT_METRIC_NAMES[0]));

// 挖掘入口. results 是 main 里 per-strategy backtest 的结果 (自检要拿目标策略
//   的 strategy_nav 对账), 下标与 strategy::STRATEGIES[] 对齐.
//   返回 elapsed_seconds.
double run(const feature::Axes &axes, const feature::Tensor &T,
           std::span<const backtest::Result> results);

} // namespace mine
