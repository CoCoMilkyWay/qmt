#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"

namespace feature {

// Phase 4: per-feature × per-year 统计, 类似 pandas .describe() + nan%.
//   asset 维度 (a) 与 year 内的 d 全部展平成一维样本, 收集 finite 值后排序计算
//   分位数. 总是输出 "all" 行 (全期); config::DESCRIBE_BY_YEAR=true 时额外按
//   axes.dates[d].substr(0,4) 切年.
//
// 输出双通道:
//   1) stdout 等宽对齐表 (与现有 [feature] 日志同流)
//   2) <git_root>/output/feature_describe.tsv (制表符分隔, 完整精度)
void describe(const Axes &axes, const Tensor &T);

} // namespace feature
