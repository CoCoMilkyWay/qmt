#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"

namespace strategy {

// ============================================================================
// 策略 4 列的通用计算 (泛参 StrategySpec, 单点真理一份代码; 列语义见 strategy.hpp).
//   共享节点由 Phase 2/3 一次算好, 策略列只做 bool 运算/加权/排名, 量级可忽略.
//
//   compute_ts_columns — Phase 2s (per-A 并行): 循环 STRATEGIES[] 算各 pool_b.
//     依赖共享 TS: susp / is_margin / list_age / delist_age / industry_l1.
//   compute_cs_columns — Phase 3s (per-D 并行): 循环策略算
//     pool → score → rank. 依赖共享 TS (rank_key / filter) 与 CS (factor).
// ============================================================================
void compute_ts_columns(const feature::Axes &, const feature::StockMeta &,
                        feature::Tensor &);
void compute_cs_columns(const feature::Axes &, feature::Tensor &);

} // namespace strategy
