#pragma once

namespace feature {

// 打印特征依赖表格到 stdout (config::FEATURE_TABLE_ENABLE 门控):
//   全部已定义特征 (含未被任何策略引用的, 见 feature/def/all.hpp), 组内按
//   Kind (inter → filter → factor) 分桶、桶内拓扑序; active 列标记该节点是否
//   在真正参与计算的 feature::ALL_NODES 内. 下方再按策略打印两行 (filter /
//   factor 概览, 只列名字). 每个节点的 formula / assumption 直接取自其
//   FeatureSpec 定义 (feature/graph.hpp 强制要求非空), 不在此处另行维护文案.
void print_dependency_table();

} // namespace feature
