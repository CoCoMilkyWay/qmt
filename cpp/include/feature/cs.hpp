#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"

#include <span>
#include <vector>

namespace feature {

// ============================================================================
// Phase 3 入口: per-D 并行, 对每个 d 串行调用 FEATURES[] 中 axis==CrossSection 的
//   compute_cs(d, axes, T, bufs). bufs 是 thread-local 长度=n_a 的 3 个 float buffer.
//   不涉及具体 feature 名.
// ============================================================================
void compute_cs(const Axes &, Tensor &);

// ============================================================================
// 通用 CS kernel (供 feature.cpp 的 per-feature compute_cs 复用)
//
// 1) winsor_mad / z / pct_rank: 截面统计原语 (跳 NaN, 原地修改).
// 2) factor_pipeline: 1 行串起来的 raw → factor 槽位标准流水
//      gather_cs_row(src, d) → [optional 1/x] → winsor_mad(k=3) → z → pct_rank → scatter_cs_row(dst, d).
//
// 跨 feature 共用通用动作; 业务上每个 factor feature 在 feature.cpp 里只写一行
//   factor_pipeline(d, F::xxx_raw, F::xxx, invert=true/false, T, buf);
// ============================================================================
void winsor_mad(std::span<float> x, float k = 3.0f);
void z(std::span<float> x);
void pct_rank(std::span<float> x);

void factor_pipeline(int d, F src, F dst, bool invert, Tensor &T,
                     std::span<float> buf);

} // namespace feature
