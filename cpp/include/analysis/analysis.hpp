#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"
#include "strategy/strategy.hpp"

namespace analysis {

// 因子诊断 + 分层分析. Per-strategy: mask = 该策略 tradable, score = 该策略
//   score, 因子全集不变 (Kind::Factor 全体). 计算 per-D 沿 A 截面统计,
//   写入 <git_root>/output/strategy/<name>/analysis/.
//
// 输入:
//   - axes / T (共享: daily_return, 全部 factor 列; 策略块: tradable, score)
//   - spec / s_idx (窗口 bt_start_date / top-K = hold_n / 输出目录名)
//
// 配置 (config.hpp):
//   - ANALYSIS_N_QUANTILES (TAG 4 分层桶数)
//
// 输出 (npy 全部 little-endian, 形状由 dimnames 决定):
//   - dates.npy              [n_d_bt] int32 (axes 全局索引, 与 backtest 对齐)
//   - factor_ic.npy          [n_factor, n_d_bt] f4 (per-D 截面 Pearson IC vs t+1 收益, 限于 tradable)
//   - factor_turnover.npy    [n_factor, n_d_bt] f4 (top-K Jaccard turnover, K=spec.hold_n)
//   - factor_corr.npy        [n_factor, n_factor] f4 (全周期日均截面 Pearson 平均, 限于 tradable)
//   - score_ic.npy           [n_d_bt] f4 (聚合策略 score IC, 限于 tradable)
//   - score_turnover.npy     [n_d_bt] f4 (聚合 score top-K turnover)
//   - quantile_ret.npy       [ANALYSIS_N_QUANTILES, n_d_bt] f4 (tradable 内 Q 分桶,
//                              桶内 daily_return[d+1] 等权均值)
//   - pool_ret.npy           [n_d_bt] f4 (tradable[d-1] 等权 × daily_return[d],
//                              与 backtest pool_nav 同口径)
//   以下 4 列是上面几条的现成可绘形态 (NaN 视作 0 收益 / 0 IC 的累积口径):
//   - quantile_nav.npy       [ANALYSIS_N_QUANTILES, n_d_bt] f4 (分层累计净值, 起点 1.0)
//   - pool_nav_cum.npy       [n_d_bt] f4 (pool 累计净值, 起点 1.0)
//   - factor_ic_cum.npy      [n_factor, n_d_bt] f4 (累积 IC 曲线)
//   - score_ic_cum.npy       [n_d_bt] f4 (聚合 score 累积 IC 曲线)
//   - report.json            JSON 因子汇总表 (列式: 因子/当期IC/IC均值/平均IC/
//                              IR/换手率/本策略权重) + 分层 CAGR (Q1..QQ + pool)
//
// 窗口与 backtest 同源 (spec.bt_start_date, 右端点 axes 最新日), 但内部独立解析.
double run(const feature::Axes &axes, const feature::Tensor &T,
           const strategy::StrategySpec &spec, int s_idx);

} // namespace analysis
