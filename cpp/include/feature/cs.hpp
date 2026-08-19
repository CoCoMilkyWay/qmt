#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/graph.hpp"
#include "feature/tensor.hpp"

#include <span>

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
// 2) factor_pipeline / neutral_pipeline: 纯统计标准化流水, 对"方向"一无所知 —
//      不做任何 1/x 之类的定向变换. 调用方 (各 cs_<name>) 自己先
//      T.gather_cs_row(src, d, b.a), 需要的话在 b.a 上做自己的 elementwise
//      变换 (例如 EP := 1/PE 是该因子自身的定义, 不是"方向偏好", 留在各自
//      文件里), 再调 pipeline 完成 剔非在市 → winsor → [neutralize] → z →
//      pct_rank → 均值填充 → scatter(dst, d).
//   因子"好坏方向"完全由 strategy::FactorWeight.w 的符号决定 (可正可负),
//   feature 层不预设任何方向.
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

// 中性化因子流水: b.a 需已由调用方 gather(+ 自定义变换) 好待处理原始值 →
//   剔非在市 → winsorize_quantile(1%,99%) → neutralize(行业+log mcap) → z
//   → pct_rank → 均值填充 → scatter(dst, d). 输出仍 ∈[0,1], factor_score 兼容.
//   buf 占用: a=y(输入/残差), b=log mcap, c=industry; 正好 3 个 CsBufs.
void neutral_pipeline(int d, const FeatureSpec &dst, Tensor &T, CsBufs &b);

// b.a 需已由调用方 gather(+ 自定义变换) 好待处理原始值 → 剔非在市 →
//   winsor_mad(k=3) → z → pct_rank → 均值填充 → scatter(dst, d).
//   buf 占用: a=输入/输出, b=mask 借用.
void factor_pipeline(int d, const FeatureSpec &dst, Tensor &T, CsBufs &b);

} // namespace feature
