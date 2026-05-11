#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"

#include <span>
#include <vector>

namespace feature {

// 统一 [F][A][D] layout: 每 feature 一段连续 length = A*D 的 float (a-major, d-minor).
//   ts_row(f, a) → 长度 D 的连续 span (Phase 2 主路径)
//   gather_cs_row(f, d, out) → 长度 A 的 stride-D gather (Phase 3 入口一次性 copy 到栈 buffer)
// 全部初始化为 NaN.
struct Tensor {
  const Axes &axes;
  std::vector<std::vector<float>> mats; // size = F::COUNT, 每段 length = n_a() * n_d()

  explicit Tensor(const Axes &);

  std::span<float>       ts_row(F f, int a);
  std::span<const float> ts_row(F f, int a) const;

  // 截面 行 (length = n_a()): out[a] = at(f, a, d)
  void gather_cs_row(F f, int d, std::span<float> out) const;
  void scatter_cs_row(F f, int d, std::span<const float> in);

  float  at(F f, int a, int d) const;
  float &at(F f, int a, int d);

  // 单列 finite 校验 — 仅用于"按 README 契约绝对无 NaN"的 feature
  //   (e.g. pool / tradable / 所有 bool 状态机). raw / factor 列预期可能 NaN
  //   (上市前/退市后/财报未出), 不要在那些列上调用.
  void assert_finite(F f) const;
};

} // namespace feature
