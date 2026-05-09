#pragma once

#include "feature/axis.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

namespace feature {

// Phase 2: per-A 并行 (线程池, 任务粒度 = a, 总数 = n_a()).
//   每 worker 拿到 a, 串行调 stage 1..6 (见 ts.cpp 内 compute_ts_for_one_a):
//     stage 1: extract_grid       — 网格 itf → close_raw / *_lim / susp / mcap_raw / ... (应用 offset)
//     stage 2: ttm4_helpers + ni  — rev_raw / pcf_raw / roe_raw / roa_raw / ni_raw
//     stage 3: static             — mb / list_age (来自 StockMeta)
//     stage 4: derived bool       — low_p / low_mc / limit_up / limit_dn
//     stage 5: state_machines     — profit_st / revenue_st / dividend_st / risk_warn + trading_st (rolling)
//     stage 6: pool_b / new_list
//   每 a 写自己的 ts_row(F, a), phase 内无写冲突.
void compute_ts(const Axes &, const PitPool &, const StockMeta &, Tensor &);

} // namespace feature
