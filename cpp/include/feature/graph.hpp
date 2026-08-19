#pragma once

#include "feature/feature.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace feature {

// ============================================================================
// FeatureSpec — 特征节点的唯一身份.
//   每个节点文件声明恰一个 `inline constexpr FeatureSpec <name>_spec`; inline
//   保证跨 TU 同址, 取地址 (&<name>_spec) 即该节点的编译期稳定 ID, 用于:
//     - deps: 声明自己依赖谁 (依赖节点的 spec 地址; 要求先 #include 依赖头文件)
//     - Tensor 存取: T.ts_row(<name>_spec, a) 等, Tensor 内部按地址查行号
//     - 策略引用: PoolSpec.rank_key / StrategySpec.filters / FactorWeight.f
//
//   一个节点只应出现在 3 处: 自己的定义 (本文件) / 依赖它的其他节点的 deps
//   数组 (对方 #include 本文件) / 策略引用处 (策略 def #include 本文件).
//   没有任何中心枚举或清单需要为新节点手动维护.
// ============================================================================
struct FeatureSpec {
  const char *name;
  Kind kind;
  Axis axis;
  std::span<const FeatureSpec *const> deps; // 依赖节点地址; 空 = 仅依赖 Phase 1 输入
  TsComputeFn compute_ts;                   // axis==TimeSeries 时非 null; CS 时 null
  CsComputeFn compute_cs;                   // axis==CrossSection 时非 null; TS 时 null
  bool must_be_finite = false;              // build 末尾契约校验: 该列必须全 finite (由节点
                                            // 自己声明, 例如 bool 状态机 / factor pipeline
                                            // 输出; raw/*_age/daily_return 等允许 NaN 不设).
  const char *formula = "";                 // 必填: 计算公式 (registry_report.cpp 打印用; 不得为空)
  const char *assumption = "";              // 必填: 单位/边界条件/关键假设 (同上; 无则写 "—")
};

namespace graph_detail {
// 故意只声明不定义: consteval 求值路径一旦触达即编译失败, 函数名即诊断信息.
void error_dependency_cycle_detected();
void error_ts_node_depends_on_cs_node();
void error_self_referencing_dep();
} // namespace graph_detail

// ============================================================================
// 可达性 + 拓扑排序 — 从任意 roots 集合出发, 沿 deps 反向 DFS 后序遍历:
//   post-order (先递归 deps, 再把自己加入 done) 天然就是一个合法拓扑序
//   (每个节点出现位置一定晚于它的全部依赖). 同时做:
//     - 环检测: 节点在 "正在访问" 栈上被再次访问 ⇒ 成环, consteval fail.
//     - 轴校验: TS 节点不得依赖 CS 节点, consteval fail.
//   C++20 起 std::vector 在 consteval 内可用 (计算全程不逃逸出常量求值),
//   故直接用 vector 写自然的图遍历, 不必手搓定长数组状态机.
// ============================================================================
consteval std::vector<const FeatureSpec *>
reachable_topo_order(std::span<const FeatureSpec *const> roots) {
  std::vector<const FeatureSpec *> done;
  std::vector<const FeatureSpec *> stack; // 正在访问 (环检测用)

  auto contains = [](const std::vector<const FeatureSpec *> &v,
                     const FeatureSpec *n) {
    for (const FeatureSpec *x : v)
      if (x == n)
        return true;
    return false;
  };

  auto visit = [&](auto &&self, const FeatureSpec *n) -> void {
    if (n == nullptr)
      graph_detail::error_self_referencing_dep();
    if (contains(done, n))
      return;
    if (contains(stack, n))
      graph_detail::error_dependency_cycle_detected();
    stack.push_back(n);
    for (const FeatureSpec *d : n->deps) {
      if (n->axis == Axis::TimeSeries && d->axis == Axis::CrossSection)
        graph_detail::error_ts_node_depends_on_cs_node();
      self(self, d);
    }
    stack.pop_back();
    done.push_back(n);
  };

  for (const FeatureSpec *r : roots)
    visit(visit, r);
  return done;
}

consteval std::size_t
count_reachable(std::span<const FeatureSpec *const> roots) {
  return reachable_topo_order(roots).size();
}

template <std::size_t N>
consteval std::array<const FeatureSpec *, N>
collect_reachable(std::span<const FeatureSpec *const> roots) {
  std::vector<const FeatureSpec *> v = reachable_topo_order(roots);
  std::array<const FeatureSpec *, N> out{};
  for (std::size_t i = 0; i < N; ++i)
    out[i] = v[i];
  return out;
}

// 按 axis 过滤 (保持相对顺序; 拓扑序的按轴子序列仍是合法拓扑序).
template <std::size_t N>
consteval std::size_t count_axis(const std::array<const FeatureSpec *, N> &all,
                                 Axis ax) {
  std::size_t c = 0;
  for (const FeatureSpec *s : all)
    if (s->axis == ax)
      ++c;
  return c;
}

template <std::size_t M, std::size_t N>
consteval std::array<const FeatureSpec *, M>
filter_axis(const std::array<const FeatureSpec *, N> &all, Axis ax) {
  std::array<const FeatureSpec *, M> out{};
  std::size_t i = 0;
  for (const FeatureSpec *s : all)
    if (s->axis == ax)
      out[i++] = s;
  return out;
}

// 文档契约校验: 每个进入计算图的节点必须自己填写 formula / assumption
//   (feature/registry.hpp 打印依赖表格用; 空字符串视为漏填, 直接编译失败).
template <std::size_t N>
consteval bool all_documented(const std::array<const FeatureSpec *, N> &all) {
  for (const FeatureSpec *s : all)
    if (s->formula[0] == '\0' || s->assumption[0] == '\0')
      return false;
  return true;
}

} // namespace feature
