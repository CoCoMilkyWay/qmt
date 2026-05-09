#pragma once

#include "feature/axis.hpp"
#include "feature/tensor.hpp"

namespace feature {

// Phase 3: per-D 并行 (线程池, 任务粒度 = d, 总数 = n_d()).
//   每 worker 拿到 d, 串行跑:
//     - 10 条 TS inter raw → CS 归一化 pipeline, 写入当前 Kind::Factor 槽位 (close/mcap/fmcap/pe/pb/ps/pcf/roe/roa/dy)
//       pipeline = gather_cs_row → winsor_mad → z → pct_rank → scatter_cs_row
//       invert: 估值类 (close/mcap/fmcap/pe/pb/ps/pcf) 取 1/x; 盈利/股息 (roe/roa/dy) 不取倒数
//     - compute_pool: pool_b ∧ rank(mcap_raw asc within pool_b) ≤ UNIVERSE_SIZE
//   每 d 写自己的 cs_row (各 feature 独立段), phase 内无写冲突.
void compute_cs(const Axes &, Tensor &);

} // namespace feature
