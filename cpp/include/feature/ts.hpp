#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace feature {

// ============================================================================
// Phase 2 入口: per-A 并行, 对每个 a 串行调用 registry.hpp 编译期推出的
//   TS_ORDER 中各节点的 compute_ts(a, axes, pool, meta, T). 不涉及具体
//   feature 名 — 业务逻辑全部下沉到 feature/def/ 的 per-node 函数.
// ============================================================================
void compute_ts(const Axes &, const PitPool &, const StockMeta &, Tensor &);

// ============================================================================
// 通用 TS kernel — state_machine_intervals<TEv>:
//   按 v 升序遍历 trigger_events, 每 trigger 用 find_off(trigger) 求终止 d,
//   区间 [trigger.v, off_d) 写 1.0; 区间外 0.0; 多 trigger OR (重叠取并集).
//   ev.v 已是 pit.cpp replay 时应用 raw cutoff 后的首次可见 row D, 直接用.
// ============================================================================
template <class TEv, class FindOff>
void state_machine_intervals(const std::vector<TEv> &triggers, int n_d,
                             FindOff find_off, std::span<float> dst) {
  std::fill(dst.begin(), dst.end(), 0.0f);
  for (const TEv &e : triggers) {
    int on_d = e.v;
    int off_d = find_off(e);
    if (on_d < 0)
      on_d = 0;
    if (off_d > n_d)
      off_d = n_d;
    if (off_d <= on_d)
      continue;
    for (int d = on_d; d < off_d; ++d)
      dst[d] = 1.0f;
  }
}

} // namespace feature
