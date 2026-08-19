#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/graph.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace feature {

// 统一 [node][A][D] layout: 每节点一段连续 length = A*D 的 float (a-major, d-minor).
//   ts_row(spec, a) → 长度 D 的连续 span (Phase 2 主路径)
//   gather_cs_row(spec, d, out) → 长度 A 的 stride-D gather (Phase 3 入口一次性 copy 到栈 buffer)
// 全部初始化为 NaN.
//
//   节点 → mats 下标 由 registry.hpp 编译期算出的 ALL_NODES (全体可达节点的
//   拓扑序) 决定, 构造时一次性建 index_ (地址 → 下标), 之后 O(1) 查; 无中心
//   枚举, 新增节点不需要改这里.
//
// 策略块 strat_mats: 每策略固定 5 列 (pool_b/pool/tradable/score/rank) 的独立
//   存储, 布局与 mats 相同. slot = 策略下标 × SF_COUNT + 列 (映射由 strategy
//   层定义, feature 层只见扁平 slot). n_strat_slots 由 build 传入
//   (= strategy::N_STRAT_SLOTS).
struct Tensor {
  const Axes &axes;
  std::vector<std::vector<float>> mats;       // size = order.size(), 每段 length = n_a() * n_d()
  std::vector<std::vector<float>> strat_mats; // size = n_strat_slots, 同布局

  Tensor(const Axes &, std::span<const FeatureSpec *const> order,
         int n_strat_slots);

  std::span<float> ts_row(const FeatureSpec &f, int a);
  std::span<const float> ts_row(const FeatureSpec &f, int a) const;

  // 截面 行 (length = n_a()): out[a] = at(f, a, d)
  void gather_cs_row(const FeatureSpec &f, int d, std::span<float> out) const;
  void scatter_cs_row(const FeatureSpec &f, int d, std::span<const float> in);

  float at(const FeatureSpec &f, int a, int d) const;
  float &at(const FeatureSpec &f, int a, int d);

  // 策略块访问器 — 与共享版同语义, spec 换成扁平 slot.
  std::span<float> strat_ts_row(int slot, int a);
  std::span<const float> strat_ts_row(int slot, int a) const;
  void strat_gather_cs_row(int slot, int d, std::span<float> out) const;
  void strat_scatter_cs_row(int slot, int d, std::span<const float> in);
  float strat_at(int slot, int a, int d) const;

  // 单列 finite 校验 — 仅用于"按契约绝对无 NaN"的 feature
  //   (e.g. 所有 bool 状态机 / factor). 策略 5 列全部无 NaN, 走 strat 版.
  //   raw / *_age / daily_return 等列预期可能 NaN, 不要在那些列上调用.
  void assert_finite(const FeatureSpec &f) const;
  void assert_finite_strat(int slot) const;

private:
  std::unordered_map<const FeatureSpec *, int> index_;

  int index_of(const FeatureSpec &f) const;
};

} // namespace feature
