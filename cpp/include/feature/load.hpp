#pragma once

#include "feature/axis.hpp"
#include "feature/pit.hpp"

namespace feature {

// Phase 1: 通用 flow — 仅迭代 pit.hpp 的 ITFS[] 表, 不出现具体 itf 名:
//   1) 对每个 ItfDesc 调 prealloc(axes, pool)
//   2) 扫 data/YYYY/MM/DD/<file_name>.json 全量, 入 task 队列
//   3) per-(day, itf) 并行 parse: 网格 itf 无锁 / 事件 itf 走 per-A mutex
//   4) 对每个 ItfDesc 调 post_sort(pool)  (事件 itf 末段 sort by v; 网格 nullptr)
// 增减 itf 完全不动本文件.
void load_pit(const Axes &, PitPool &);

} // namespace feature
