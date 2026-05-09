#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace feature {

// F 维枚举. 顺序: filter → factor → inter. Kind 与 Axis 正交, 逐项见 FEATURES[].
// 与 README §字段表 一一对应; 新增 feature 在对应分组末尾追加, 不破坏既有索引.
enum class F : int {
  // ---- filter ----
  profit_st = 0,
  revenue_st,
  dividend_st,
  trading_st,
  risk_warn,
  new_list,
  // ---- factor ----
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
  // ---- inter: raw ----
  close_raw,
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
  pcf_raw,
  roe_raw,
  roa_raw,
  rev_raw,
  ni_raw,
  // ---- inter: asset 静态 ----
  mb,
  list_age,
  // ---- inter: 衍生 bool ----
  low_p,
  low_mc,
  limit_up,
  limit_dn,
  // ---- inter: pool ----
  pool_b, // 时序 bool (basic pool, 主板 ∧ ¬susp ∧ exchange ∈ {SSE, SZSE})
  pool,   // 截面 bool (pool_b 内按 mcap_raw 升序取前 UNIVERSE_SIZE)

  COUNT,
};

enum class Kind : uint8_t { Filter, Factor, Inter };
enum class Axis : uint8_t { TimeSeries, CrossSection };

struct FeatureMeta {
  const char *name;
  Kind kind;
  Axis axis;
};

// 静态表, 索引与 F 枚举一一对应.
extern const std::array<FeatureMeta, static_cast<std::size_t>(F::COUNT)> FEATURES;

} // namespace feature
