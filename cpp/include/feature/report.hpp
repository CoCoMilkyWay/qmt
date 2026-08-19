#pragma once

namespace feature {

// 打印特征依赖表格到 stdout (config::FEATURE_TABLE_ENABLE 门控):
//   公共 (框架固定根 FRAMEWORK_ROOTS + 全部策略共同引用) → 各策略专属,
//   组内保持 ALL_NODES 的拓扑序 (inter → filter → factor 分桶展示).
//   每个节点的 formula / assumption 直接取自其 FeatureSpec 定义 (feature/graph.hpp
//   强制要求非空), 不在此处另行维护文案.
void print_dependency_table();

} // namespace feature
