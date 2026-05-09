#pragma once

#include "factor/axis.hpp"
#include "factor/tensor.hpp"

namespace factor {

// 编排入口: 串 Phase 0..3, 内部用 misc::Timer 报每段耗时.
//   Phase 0 axes:  load_axes() + load_stock_meta() + Tensor T(axes)
//   Phase 1 PIT:   load_pit(axes, pool)
//   Phase 2 时序: compute_ts(axes, pool, meta, T)
//   Phase 3 截面: compute_cs(axes, T)
//
// Tensor.axes 是引用, 由调用方持有 out_axes 保证生命周期 (in-memory only, 同进程).
// out_axes / out_meta 在调用前由 caller 默认构造, 调用后填充.
Tensor build(Axes &out_axes, StockMeta &out_meta);

} // namespace factor
