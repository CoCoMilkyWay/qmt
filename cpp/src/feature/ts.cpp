#include "feature/ts.hpp"

#include <cassert>
#include <functional>
#include <span>
#include <vector>

namespace feature {

namespace {

// ============================================================================
// per-A worker: 串行调 stage 1..6
// ============================================================================
void compute_ts_for_one_a(int a, const Axes &axes, const PitPool &pool,
                          const StockMeta &meta, Tensor &T);

// ----- stage 1: 网格 itf → raw 时序 (应用 offset) -----
void stage_extract_grid(int a, const Axes &, const PitPool &, Tensor &);

// ----- stage 2: ttm4 helpers + ni -----
void stage_ttm4(int a, const Axes &, const PitPool &, Tensor &);
void stage_ni_raw(int a, const Axes &, const PitPool &, Tensor &);

// ----- stage 3: asset 静态 (mb, list_age) -----
void stage_static(int a, const Axes &, const StockMeta &, Tensor &);

// ----- stage 4: 衍生 bool (low_p, low_mc, limit_up, limit_dn) -----
void stage_derived(int a, const Axes &, Tensor &);

// ----- stage 5: 状态机 + trading_st rolling -----
void stage_state_machines(int a, const Axes &, const PitPool &, Tensor &);

// ----- stage 6: pool_b, new_list -----
void stage_pool_b(int a, const Axes &, const StockMeta &, Tensor &);

// ============================================================================
// 共享 helper (per-A)
// ============================================================================

// 将事件链按 v 升序回放, 每条事件 e 触发 write_fn(d_target, e), 之后 row D
// 沿用最新值直至下一事件 (forward fill). offset 在 d_target 计算时已并入.
template <class Ev>
void forward_fill_event_to_grid(
    const std::vector<Ev> &events, int n_d, int offset_days,
    const std::function<void(int /*d_from*/, int /*d_to_excl*/, const Ev &)> &fill);

// ttm4_ytd(X) := X(t) + X(Y-1, 12) - X(Y-1, t.M) (per A 时序串)
//   events: IncomeEv / CashflowEv / FinaIndEv 这类按 (end_date, ann_date) 序的链
//   value_extract: 取 e.revenue / e.n_cashflow_act / e.roe 等
//   out: 长度 D 的 float 序列, 缺位用 NaN
template <class Ev>
void ttm4_ytd(const std::vector<Ev> &events, int n_d,
              const std::function<float(const Ev &)> &value_extract,
              std::span<float> out);

// 通用状态机骨架 (per A): 按 v 升序回放事件流, 由调用方决定上线/下线条件.
//   on_trigger(e) → 返回 true 表示进入 ST, 写终止前的 d 段
//   on_terminate(e) → 返回 true 表示退出 ST
//   写入 dst[D] 为 0/1 (NaN 表未知, 当前实现统一 0)
template <class Ev>
void state_machine(const std::vector<Ev> &events, int n_d,
                   const std::function<bool(const Ev &)> &on_trigger,
                   const std::function<bool(const Ev &)> &on_terminate,
                   std::span<float> dst);

} // namespace

// ============================================================================
// 入口
// ============================================================================

void compute_ts(const Axes &, const PitPool &, const StockMeta &, Tensor &) {
  assert(false && "feature::compute_ts not implemented");
}

namespace {

void compute_ts_for_one_a(int, const Axes &, const PitPool &,
                          const StockMeta &, Tensor &) {
  assert(false && "feature::compute_ts_for_one_a not implemented");
}

void stage_extract_grid(int, const Axes &, const PitPool &, Tensor &) {
  assert(false && "feature::stage_extract_grid not implemented");
}

void stage_ttm4(int, const Axes &, const PitPool &, Tensor &) {
  assert(false && "feature::stage_ttm4 not implemented");
}

void stage_ni_raw(int, const Axes &, const PitPool &, Tensor &) {
  assert(false && "feature::stage_ni_raw not implemented");
}

void stage_static(int, const Axes &, const StockMeta &, Tensor &) {
  assert(false && "feature::stage_static not implemented");
}

void stage_derived(int, const Axes &, Tensor &) {
  assert(false && "feature::stage_derived not implemented");
}

void stage_state_machines(int, const Axes &, const PitPool &, Tensor &) {
  assert(false && "feature::stage_state_machines not implemented");
}

void stage_pool_b(int, const Axes &, const StockMeta &, Tensor &) {
  assert(false && "feature::stage_pool_b not implemented");
}

} // namespace

} // namespace feature
