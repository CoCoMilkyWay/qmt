#pragma once

#include "feature/axis.hpp"
#include "feature/pit.hpp"

namespace feature {

// Phase 1: 扫 data/YYYY/MM/DD/, per-(day, itf) 并行解析, 填 PitPool.
//   - 网格 itf: 按 visible_d_idx 直接写 dense slot (无写冲突, 无锁)
//   - 事件 itf: 走 per-A mutex emplace, 末段 sort by v 升序
// 调用前 pool 应为空, 调用后所有字段就绪 (网格 NaN 表缺席, 事件链按 v 升序).
void load_pit(const Axes &, PitPool &);

} // namespace feature
