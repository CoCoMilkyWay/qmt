#include "factor/cs.hpp"

#include "factor/feature.hpp"

#include <cassert>
#include <span>

namespace factor {

namespace {

// ============================================================================
// per-D pipeline: src (raw) → dst (factor)
//   1. gather_cs_row(src, d, buf)            — buf[A] = T.at(src, a, d)
//   2. (invert ? buf[a] = 1/buf[a] : 留)     — NaN / 0 → NaN
//   3. winsor_mad(buf, k=3.0)                — 3*MAD 截断, 跳 NaN
//   4. z(buf)                                — (x - mean) / std, 跳 NaN
//   5. pct_rank(buf)                         — 升序百分位 ∈ [0,1], 跳 NaN
//   6. scatter_cs_row(dst, d, buf)
// ============================================================================
void pipeline(int d, F src, F dst, bool invert, Tensor &T);

// ============================================================================
// 纯函数 helper (跳 NaN, 原地修改; A 维 buffer)
// ============================================================================
void winsor_mad(std::span<float> x, float k = 3.0f);
void z(std::span<float> x);
void pct_rank(std::span<float> x);

// ============================================================================
// pool 截面: pool_b (TS bool) ∧ rank(mcap_raw asc within pool_b) ≤ UNIVERSE_SIZE
//   → 写 F::pool 的 cs_row[d]
// ============================================================================
void compute_pool(int d, int universe_size, Tensor &T);

} // namespace

void compute_cs(const Axes &, Tensor &) {
  assert(false && "factor::compute_cs not implemented");
}

namespace {

void pipeline(int, F, F, bool, Tensor &) {
  assert(false && "factor::pipeline not implemented");
}

void winsor_mad(std::span<float>, float) {
  assert(false && "factor::winsor_mad not implemented");
}

void z(std::span<float>) {
  assert(false && "factor::z not implemented");
}

void pct_rank(std::span<float>) {
  assert(false && "factor::pct_rank not implemented");
}

void compute_pool(int, int, Tensor &) {
  assert(false && "factor::compute_pool not implemented");
}

} // namespace

} // namespace factor
