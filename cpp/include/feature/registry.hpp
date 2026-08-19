#pragma once

// ============================================================================
// 计算图挂载点 — 全自动: 无中心特征清单.
//   计算图 (哪些节点真正参与计算, 以及计算顺序) 由此处从两类 "根" consteval
//   沿 deps 反向可达性 + 拓扑排序推出 (见 graph.hpp):
//     1) 框架自身固定需求 — backtest.cpp / analysis.cpp / strategy/columns.cpp
//        直接读取的少量 raw TS 节点 (与具体策略无关, 任何策略配置下都需要).
//     2) strategy::STRATEGIES[] 引用到的全部节点 (filters ∪ weights.f ∪
//        pool.rank_key), 递归展开它们的 deps.
//   新增 feature: 只需 1) def/{basic,factor,filter}/<name>.hpp 写 FeatureSpec
//   2) 在策略 config 或某个已挂载节点的 deps 里引用它 (#include + 取地址).
//   不在此可达闭包内的节点文件即使存在也不进入计算 (不触发计算, 不占 Tensor 存储).
// ============================================================================

#include "feature/def/basic/close_raw.hpp"
#include "feature/def/basic/daily_return.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/is_margin.hpp"
#include "feature/def/basic/limit_dn.hpp"
#include "feature/def/basic/limit_up.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/basic/susp.hpp"
#include "feature/graph.hpp"
#include "strategy/registry.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace feature {

namespace registry_detail {

// 框架自身固定根 — 与策略配置无关, 任何策略下都需要 (backtest 结算 /
//   strategy::columns pool_b 白名单计算).
inline constexpr const FeatureSpec *FRAMEWORK_ROOTS[] = {
    &def::close_raw_spec,
    &def::delist_age_spec,
    &def::limit_up_spec,
    &def::limit_dn_spec,
    &def::susp_spec,
    &def::daily_return_spec,
    &def::is_margin_spec,
    &def::list_age_spec,
    &def::industry_l1_spec,
};

consteval std::size_t count_strategy_roots() {
  std::size_t n = 0;
  for (const strategy::StrategySpec *s : strategy::STRATEGIES) {
    n += s->filters.size();
    n += s->weights.size();
    n += 1; // rank_key
  }
  return n;
}

consteval std::vector<const FeatureSpec *> all_roots_vec() {
  std::vector<const FeatureSpec *> v;
  for (const FeatureSpec *f : FRAMEWORK_ROOTS)
    v.push_back(f);
  for (const strategy::StrategySpec *s : strategy::STRATEGIES) {
    for (const FeatureSpec *f : s->filters)
      v.push_back(f);
    for (const auto &fw : s->weights)
      v.push_back(fw.f);
    v.push_back(s->pool.rank_key);
  }
  return v;
}

inline constexpr std::size_t N_ROOTS =
    sizeof(FRAMEWORK_ROOTS) / sizeof(FRAMEWORK_ROOTS[0]) +
    count_strategy_roots();

consteval std::array<const FeatureSpec *, N_ROOTS> all_roots() {
  std::vector<const FeatureSpec *> v = all_roots_vec();
  std::array<const FeatureSpec *, N_ROOTS> out{};
  for (std::size_t i = 0; i < N_ROOTS; ++i)
    out[i] = v[i];
  return out;
}

inline constexpr std::array<const FeatureSpec *, N_ROOTS> ROOTS = all_roots();

} // namespace registry_detail

// 编译期可达性 + 拓扑序 (含环检测 / 轴校验, 见 graph.hpp).
inline constexpr std::size_t N_ALL =
    count_reachable(registry_detail::ROOTS);
inline constexpr std::array<const FeatureSpec *, N_ALL> ALL_NODES =
    collect_reachable<N_ALL>(registry_detail::ROOTS);

inline constexpr std::size_t N_TS = count_axis(ALL_NODES, Axis::TimeSeries);
inline constexpr std::size_t N_CS = count_axis(ALL_NODES, Axis::CrossSection);

// TS_ORDER / CS_ORDER 是 ALL_NODES 的按轴子序列 (相对顺序不变, 仍是合法拓扑序),
// 供 ts.cpp / cs.cpp 调度循环用. Tensor 构造直接用 ALL_NODES 整体 (DFS 后序,
// TS/CS 节点可能交错) 作为 mats 下标顺序; index_ 是 unordered_map, 顺序不影响正确性.
inline constexpr std::array<const FeatureSpec *, N_TS> TS_ORDER =
    filter_axis<N_TS>(ALL_NODES, Axis::TimeSeries);
inline constexpr std::array<const FeatureSpec *, N_CS> CS_ORDER =
    filter_axis<N_CS>(ALL_NODES, Axis::CrossSection);

static_assert(all_documented(ALL_NODES),
              "ALL_NODES: 存在节点未填写 FeatureSpec::formula / assumption "
              "(定义处强制要求给出公式与假设说明, 供 registry_report 打印依赖表格)");

} // namespace feature
