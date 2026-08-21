#pragma once

#include <span>
#include <string>
#include <vector>

// ============================================================================
// 报告指标 kernel — 纯函数, 无业务状态, 无 IO.
//
// 单点真理: backtest (per-strategy) 与 report/aggregate (跨策略 / 等权组合) 共用
//   同一套公式 ⇒ "策略指标" 与 "组合指标" 不可能出现口径漂移.
//
// 口径约定 (全部按日频等间隔处理):
//   - 年化折算基数 TRADING_DAYS = 252
//   - 标准差一律 population (ddof=0), 与历史 numpy nanstd 口径一致
//   - NaN 语义逐函数注明; nav 序列契约上恒 > 0 (backtest 内 pv > 0 已断言)
//
// 注: 与 feature/report.hpp 无关 (那份是 stdout 打印特征依赖表).
// ============================================================================
namespace report {

inline constexpr int TRADING_DAYS = 252;

// nav → 日收益: ret[0] = 0, ret[i] = nav[i] / nav[i-1] − 1.
//   nav 非空且全 > 0 (assert).
std::vector<float> daily_returns(std::span<const float> nav);

// 回撤曲线: (nav − 累计峰值) / 累计峰值, ≤ 0.
std::vector<float> drawdown_curve(std::span<const float> nav);

// 日收益 → 累计净值 (起点 1.0, NaN 视作 0 收益).
//   分层收益 / pool 收益等"可能有空桶日"的序列用此口径, 与回测 dr_n=0 → dr=0 一致.
std::vector<float> cum_nav(std::span<const float> ret);

// NaN 视作 0 的累加 (累积 IC 曲线).
std::vector<float> nan_cumsum(std::span<const float> x);

// 滚动均值; min_periods = max(1, w / 4), 不足则用已有值均值, 全 NaN 段给 NaN.
std::vector<float> rolling_mean(std::span<const float> x, int w);

// 绝对指标 (只依赖自身 nav).
struct NavStats {
  int n_days = 0;
  float ann_return = 0.0f;     // 年化收益
  float ann_vol = 0.0f;        // 年化波动率
  float sharpe = 0.0f;         // 夏普 (rf = 0)
  float max_drawdown = 0.0f;   // 最大回撤 (≤ 0)
  int longest_no_new_high = 0; // 创新高最长间隔 (交易日)
};

NavStats nav_stats(std::span<const float> nav);

// 相对指标 (对基准日收益; 两序列等长).
struct RelStats {
  float info_ratio = 0.0f;     // 信息比率 = mean(diff) / std(diff) × √252
  float beta = 0.0f;           // cov(bench, strat) / var(bench)
  float alpha = 0.0f;          // 年化 alpha = (mean(y) − β·mean(x)) × 252
  float tracking_error = 0.0f; // std(diff) × √252
};

RelStats rel_stats(std::span<const float> ret, std::span<const float> bench_ret);

// 累计净值末值 → 全程 CAGR: nav[-1]^(252 / n) − 1 (nav 起点为 1.0).
//   分层 Q 桶用: 与"分层累计收益"图同一条净值, 不是各年年化再算术平均.
float cagr_from_nav(std::span<const float> nav);

// 单期 (年 / 月) 收益风险指标 — 年月表一行.
struct PeriodStats {
  std::string period;        // "2017" / "2017-01"
  float strat_return = 0.0f; // 期内复利收益
  float bench_return = 0.0f;
  float strat_max_dd = 0.0f;
  float bench_max_dd = 0.0f;
  float tracking_error = 0.0f;
  float info_ratio = 0.0f;
  float ann_vol = 0.0f;
  float sharpe = 0.0f;
};

// 按期次 key 分组算 PeriodStats. dates 为 "YYYYMMDD", 与 ret / bench_ret 同长.
//   by_year = true → key 取 "YYYY"; false → "YYYY-MM".
//   空期次不产出行 (与 pandas resample + dropna(how="all") 同效).
std::vector<PeriodStats> period_stats(std::span<const std::string> dates,
                                      std::span<const float> ret,
                                      std::span<const float> bench_ret,
                                      bool by_year);

// 期次胜率 = 期内复利收益 > 0 的期次占比.
//   unit: 0 = 日 (逐日直接判), 1 = 周 (pandas W-SUN 口径: 以周日收尾分箱),
//         2 = 自然月.
float win_rate(std::span<const std::string> dates, std::span<const float> ret,
               int unit);

// 两序列 Pearson (跳任一侧 NaN); 有效点 < 5 或任一方差 ≤ 0 → NaN.
float pearson(std::span<const float> x, std::span<const float> y);

// 高斯核密度估计 (Scott's rule 带宽 h = std × n^(-1/5), population std),
//   在给定网格上逐点求值 (未做边界反射校正, 网格外自然衰减).
//   samples 跳 NaN; 有效样本数 < 2 或方差 ≤ 0 → 全 NaN (前端留空 = 该期样本不足).
std::vector<float> gaussian_kde(std::span<const float> samples,
                                std::span<const float> grid);

// 跳 NaN 的均值 / 求和 / population 标准差; 无有效点 → NaN (sum → 0).
float nan_mean(std::span<const float> x);
float nan_sum(std::span<const float> x);
float nan_std(std::span<const float> x);

} // namespace report
