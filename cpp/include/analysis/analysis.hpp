#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"

namespace analysis {

// 因子诊断 + 分层分析. 计算 per-D 沿 A 截面统计, 写入 <git_root>/output/analysis/.
//
// 输入:
//   - axes / T (factor_score, pool, daily_return, F::close .. F::dy_ttm4)
//
// 配置 (config.hpp):
//   - ANALYSIS_N_QUANTILES (TAG 4 分层桶数)
//
// 输出 (npy 全部 little-endian, 形状由 dimnames 决定):
//   - dates.npy              [n_d_bt] int32 (axes 全局索引, 与 backtest 对齐)
//   - factor_ic.npy          [n_factor, n_d_bt] f4 (per-D 截面 Pearson IC vs t+1 收益, 限于 pool)
//   - factor_turnover.npy    [n_factor, n_d_bt] f4 (top-K Jaccard turnover, K=BACKTEST_HOLD_N)
//   - factor_corr.npy        [n_factor, n_factor] f4 (全周期日均截面 Pearson 平均)
//   - score_ic.npy           [n_d_bt] f4 (聚合 factor_score IC)
//   - score_turnover.npy     [n_d_bt] f4 (聚合 score top-K turnover)
//   - quantile_ret.npy       [ANALYSIS_N_QUANTILES, n_d_bt] f4 (Q 分桶 当日 daily_return 等权均值)
//   - pool_ret.npy           [n_d_bt] f4 (pool 内 daily_return 等权均值, 即 quantile 全集 baseline)
//
// 依赖 backtest 的窗口 (BACKTEST_START/END_DATE), 但内部独立解析.
double run(const feature::Axes &axes, const feature::Tensor &T);

} // namespace analysis
