#include "feature/report.hpp"

#include "feature/registry.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace feature {

namespace {

// 运行期 DFS 后序 (= 拓扑序); 只用于本文件的分组展示, 不参与实际计算图构建
// (计算图已在 feature/registry.hpp 编译期 consteval 构建完毕).
std::vector<const FeatureSpec *>
reachable_order(const std::vector<const FeatureSpec *> &roots) {
  std::vector<const FeatureSpec *> order;
  std::unordered_set<const FeatureSpec *> seen;
  std::function<void(const FeatureSpec *)> visit = [&](const FeatureSpec *n) {
    if (!seen.insert(n).second)
      return;
    for (const FeatureSpec *d : n->deps)
      visit(d);
    order.push_back(n);
  };
  for (const FeatureSpec *r : roots)
    visit(r);
  return order;
}

// 按 Kind 分桶展示 (inter → filter → factor), 桶内保持传入的拓扑相对顺序.
std::vector<const FeatureSpec *>
by_kind_then_topo(const std::vector<const FeatureSpec *> &topo) {
  std::vector<const FeatureSpec *> out;
  for (Kind k : {Kind::Inter, Kind::Filter, Kind::Factor})
    for (const FeatureSpec *s : topo)
      if (s->kind == k)
        out.push_back(s);
  return out;
}

const char *kind_name(Kind k) {
  switch (k) {
  case Kind::Filter:
    return "filter";
  case Kind::Factor:
    return "factor";
  case Kind::Inter:
    return "inter";
  }
  return "?";
}

const char *axis_name(Axis a) { return a == Axis::TimeSeries ? "时序" : "截面"; }

std::string deps_str(const FeatureSpec *s) {
  if (s->deps.empty())
    return "—";
  std::string out;
  for (std::size_t i = 0; i < s->deps.size(); ++i) {
    if (i)
      out += ", ";
    out += s->deps[i]->name;
  }
  return out;
}

void print_group(const std::string &title,
                 const std::vector<const FeatureSpec *> &nodes) {
  std::cout << "\n-- " << title << " (" << nodes.size() << ") --\n";
  for (const FeatureSpec *s : nodes) {
    std::cout << "[" << kind_name(s->kind) << "] " << s->name << "  ("
              << axis_name(s->axis) << ")\n"
              << "  deps       : " << deps_str(s) << "\n"
              << "  formula    : " << s->formula << "\n"
              << "  assumption : " << s->assumption << "\n";
  }
}

} // namespace

void print_dependency_table() {
  // 框架固定根可达闭包 = "公共" 的基础部分 (与策略配置无关).
  std::vector<const FeatureSpec *> framework_roots(
      registry_detail::FRAMEWORK_ROOTS,
      registry_detail::FRAMEWORK_ROOTS +
          (sizeof(registry_detail::FRAMEWORK_ROOTS) /
           sizeof(registry_detail::FRAMEWORK_ROOTS[0])));
  std::vector<const FeatureSpec *> framework_set =
      reachable_order(framework_roots);
  std::unordered_set<const FeatureSpec *> framework_seen(framework_set.begin(),
                                                          framework_set.end());

  // 每个策略自己的根可达闭包 (filters ∪ weights.f ∪ rank_key).
  std::vector<std::vector<const FeatureSpec *>> per_strategy;
  for (const strategy::StrategySpec *st : strategy::STRATEGIES) {
    std::vector<const FeatureSpec *> roots;
    for (const FeatureSpec *f : st->filters)
      roots.push_back(f);
    for (const auto &fw : st->weights)
      roots.push_back(fw.f);
    roots.push_back(st->pool.rank_key);
    per_strategy.push_back(reachable_order(roots));
  }

  // common_extra = 被全部策略共同引用、且不在框架固定根闭包内的节点
  //   (单策略场景下退化为 = 该策略整个闭包; 加第二个策略后自动收窄, 无需改代码).
  std::vector<const FeatureSpec *> common_extra;
  if (!per_strategy.empty()) {
    for (const FeatureSpec *cand : per_strategy[0]) {
      if (framework_seen.count(cand))
        continue;
      bool in_all = true;
      for (std::size_t i = 1; i < per_strategy.size(); ++i) {
        if (std::find(per_strategy[i].begin(), per_strategy[i].end(), cand) ==
            per_strategy[i].end()) {
          in_all = false;
          break;
        }
      }
      if (in_all)
        common_extra.push_back(cand);
    }
  }

  std::unordered_set<const FeatureSpec *> common_set(framework_set.begin(),
                                                      framework_set.end());
  for (const FeatureSpec *f : common_extra)
    common_set.insert(f);

  // 用全局 ALL_NODES 的拓扑序重新过滤, 保证公共组内部也是合法拓扑序.
  std::vector<const FeatureSpec *> common_topo;
  for (const FeatureSpec *n : ALL_NODES)
    if (common_set.count(n))
      common_topo.push_back(n);

  std::cout << "\n================ 特征依赖表 (formula / assumption 见各节点 "
               "FeatureSpec 定义) ================\n";
  print_group("公共 — 框架固定根 + 全部策略共用", by_kind_then_topo(common_topo));

  for (std::size_t i = 0; i < strategy::STRATEGIES.size(); ++i) {
    const strategy::StrategySpec *st = strategy::STRATEGIES[i];
    std::vector<const FeatureSpec *> specific;
    for (const FeatureSpec *n : ALL_NODES) {
      if (common_set.count(n))
        continue;
      if (std::find(per_strategy[i].begin(), per_strategy[i].end(), n) !=
          per_strategy[i].end())
        specific.push_back(n);
    }
    print_group(std::string(st->name) + " 专属", by_kind_then_topo(specific));
  }
  std::cout << "=====================================================================\n";
}

} // namespace feature
