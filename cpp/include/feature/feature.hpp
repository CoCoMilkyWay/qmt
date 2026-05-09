#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace feature {

// fwd decl: 避免互相 #include (feature.hpp 是最底层 spec, 不依赖具体存储类型)
struct Axes;
struct PitPool;
struct StockMeta;
struct Tensor;

// ============================================================================
// F 枚举: 顺序 = 计算顺序 = FEATURES[] 索引. 单点真理.
//   增减 feature: 1) 此处加一行 2) feature.cpp 加一行 compute fn + FEATURES[] 一行
//   计算顺序保证 (per-A / per-D 串行调用):
//     phase 2 TS: 0..pool_b 全部 axis==TimeSeries 的项, 按 enum 顺序逐一调
//     phase 3 CS: 同序遍历, 仅触发 axis==CrossSection 的项
//     依赖 = 出现在 enum 更靠后 ⇒ 其依赖 (无论 PitPool/StockMeta 还是先前 F) 已经就绪
// ============================================================================
enum class F : int {
  // ---- TS raw (PitPool 网格抽取) ----
  close_raw = 0,
  up_lim,
  dn_lim,
  susp,
  mcap_raw,
  fmcap_raw,
  share_raw,
  pe_raw,
  pb_raw,
  ps_raw,
  dy_raw,

  // ---- TS ttm4 (财报 ytd 拼接; pcf_raw 依赖 mcap_raw) ----
  rev_raw,
  pcf_raw,
  roe_raw,
  roa_raw,
  ni_raw,

  // ---- TS asset 静态广播 ----
  mb,
  list_age,

  // ---- TS 衍生 bool (依赖 raw + mb) ----
  low_p,
  low_mc,
  limit_up,
  limit_dn,

  // ---- TS state machine (filter; 依赖 raw + ttm4 + static + derived) ----
  profit_st,
  revenue_st,
  dividend_st,
  risk_warn,
  trading_st,

  // ---- TS 杂项 (filter / pool) ----
  new_list,
  pool_b,

  // ---- CS factor (依赖各 raw) ----
  close,
  mcap,
  fmcap,
  pe_ttm4,
  pb_ttm1,
  ps_ttm4,
  pcf_ttm4,
  roe_ttm4,
  roa_ttm4,
  dy_ttm4,

  // ---- CS pool (依赖 pool_b + mcap_raw) ----
  pool,

  COUNT,
};

enum class Kind : uint8_t { Filter, Factor, Inter };
enum class Axis : uint8_t { TimeSeries, CrossSection };

// per-A TS compute: 写自己 ts_row(F::self, a). 可读 pool / meta / 已写就的 T.ts_row(prior_f, a).
//   入参对所有 TS feature 统一; 不需要的子集就忽略.
using TsComputeFn = void (*)(int a, const Axes &, const PitPool &,
                             const StockMeta &, Tensor &);

// per-D CS scratch: thread-local, 长度 = n_a 的 3 个 buffer (复用避免反复分配).
//   factor pipeline 用 a; pool 用 a (pool_b) + b (mcap_raw); c 留给未来扩展.
struct CsBufs {
  std::span<float> a;
  std::span<float> b;
  std::span<float> c;
};

using CsComputeFn = void (*)(int d, const Axes &, Tensor &, CsBufs &);

struct FeatureMeta {
  const char *name;
  Kind        kind;
  Axis        axis;
  TsComputeFn compute_ts; // axis==TimeSeries 时非 null; CS 时 null
  CsComputeFn compute_cs; // axis==CrossSection 时非 null; TS 时 null
};

// 静态表, 索引 = F 枚举值. 单点真理. 在 feature.cpp 定义.
extern const std::array<FeatureMeta, static_cast<std::size_t>(F::COUNT)> FEATURES;

// ============================================================================
// 公用小工具 (供 feature.cpp / ts.cpp / cs.cpp 共享)
// ============================================================================

// -ffast-math 下 std::isfinite/std::isnan UB; 用 IEEE-754 bit-pattern 判定.
//   (bits & 0x7f80_0000) != 0x7f80_0000 ⇔ exp 非全 1 ⇔ 有限值 (非 inf/NaN).
inline bool is_finite(float x) {
  std::uint32_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  return (bits & 0x7f800000u) != 0x7f800000u;
}

// "YYYYMMDD" → int Y / int M; 长度不足返回 0
inline int year_of(const std::string_view yyyymmdd) {
  if (yyyymmdd.size() < 4) return 0;
  return (yyyymmdd[0] - '0') * 1000 + (yyyymmdd[1] - '0') * 100 +
         (yyyymmdd[2] - '0') * 10 + (yyyymmdd[3] - '0');
}

inline int month_of(const std::string_view yyyymmdd) {
  if (yyyymmdd.size() < 6) return 0;
  return (yyyymmdd[4] - '0') * 10 + (yyyymmdd[5] - '0');
}

} // namespace feature
