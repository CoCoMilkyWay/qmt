#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/graph.hpp"
#include "feature/tensor.hpp"

#include <span>
#include <vector>

namespace feature {

// ============================================================================
// Phase 3 入口: per-D 并行, 对每个 d 串行调用 CS_ORDER (registry.hpp 编译期推出)
//   里各节点的 compute_cs(d, axes, T, bufs). bufs 是 thread-local 长度=n_a 的
//   3 个 float buffer. 不涉及具体 feature 名.
// ============================================================================
void compute_cs(const Axes &, Tensor &);

// ============================================================================
// 通用 CS kernel (供 def/factor/*.hpp 的 per-factor compute_cs 复用)
//
// 1) winsor_mad / z / pct_rank: 截面统计原语 (跳 NaN, 原地修改).
// 2) factor_pipeline: 1 行串起来的 raw → factor 槽位标准流水
//      gather_cs_row(src, d) → 剔非在市 → [optional 1/x] → winsor_mad(k=3) → z
//      → pct_rank → scatter_cs_row(dst, d).
//
// 跨 feature 共用通用动作; 业务上每个 factor 节点只写一行
//   factor_pipeline(d, xxx_raw_spec, xxx_spec, invert=true/false, T, buf);
// ============================================================================
void winsor_mad(std::span<float> x, float k = 3.0f);
void z(std::span<float> x);
void pct_rank(std::span<float> x);

// 截面分位缩尾 (跳 NaN): 把 [lo_pct, hi_pct] 分位以外的 finite 值夹回分位边界.
//   果仁 "中性化" 实测口径: raw 先 1%-99% 缩尾再 OLS, 故独立于 winsor_mad.
void winsorize_quantile(std::span<float> x, float lo_pct = 0.01f,
                        float hi_pct = 0.99f);

// 截面行业+市值中性化 (跳 NaN, 原地): y ~ 1 + log(mcap) + 申万一级行业 dummy, 写残差.
//   y/logmc/industry 任一 NaN (或 industry=0 未知) 的样本不参与拟合, 残差留 NaN (下游均值填充).
//   logmc: mcap_raw>0 取 log, 否则 NaN. industry: 1..31 有效, 0=未知→排除.
void neutralize(std::span<float> y, std::span<const float> logmc,
                std::span<const float> industry);

// 中性化因子流水: gather raw → 剔非在市 → [1/x] → winsorize_quantile(1%,99%)
//   → neutralize(行业+log mcap) → z → pct_rank → 均值填充 → scatter.
//   输出仍 ∈[0,1], factor_score 兼容.
//   buf 需求: a=y(残差), b=log mcap, c=industry; 正好 3 个 CsBufs.
void neutral_pipeline(int d, const FeatureSpec &src, const FeatureSpec &dst,
                      bool invert, Tensor &T, CsBufs &b);

void factor_pipeline(int d, const FeatureSpec &src, const FeatureSpec &dst,
                     bool invert, Tensor &T, CsBufs &b);

} // namespace feature
