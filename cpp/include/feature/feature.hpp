#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

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
//     phase 2 TS: 顺序遍历 FEATURES[], 仅触发 axis==TimeSeries 的项
//     phase 3 CS: 同序遍历, 仅触发 axis==CrossSection 的项
//     依赖 = 出现在 enum 更靠后 ⇒ 其依赖 (无论 PitPool/StockMeta 还是先前 F) 已经就绪
//
//   分段: TS 段 (一切 per-A 沿 D 计算) → CS 段 (一切 per-D 沿 A 计算).
//   段内不强分小类, 但聚集相似项以利对仗 (raw / derived / filter / pool):
//     raw     = 网格直读或事件 ttm12 拼接 / meta 派生 (单位 [元/股/%/ratio/bool])
//     derived = TS 内由 raw 推 (单位通常 [bool] 或 [ratio])
//     filter  = 状态机或单点判 → 排除位 [bool], 1 = 排除 (D, A)
//     pool    = universe 母集 [bool]
// ============================================================================
enum class F : int {
  // ============================== TS ==============================
  // raw 网格 — PitPool dense 直读 / 由 raw 直接相乘 (per-A 动态)
  close_raw = 0, // bar1d.close (不复权 [元/股]; ← cn_stock_real_bar1d.close)
  mcap_raw,      // close_raw × shares.total_shares  ([元])
  fmcap_raw,     // close_raw × shares.a_float_shares  ([元]; BigQuant 实测口径 = A 股流通市值, 不含 H 股)
  share_raw,     // shares.total_shares  ([股])
  pb_raw,        // (财务: 暂保留 NaN 输出, 待 BigQuant 财务表迁移后实现)
  ps_raw,        // (财务: 暂保留 NaN)
  dy_raw,        // (财务: 暂保留 NaN)
  up_lim,        // limit_price.upper_limit (主动 -1 对齐到 close_raw[D]=D-1 收盘的同源比较)
  dn_lim,        // limit_price.lower_limit (同上)
  susp,          // status.suspended ([bool])
  is_margin,     // margin_detail (D,A) 存在性 ([bool])
  mr_bal_raw,    // margin_detail.financing_balance ([元])
  ms_bal_raw,    // margin_detail.securities_lending_balance ([元])
  industry_l1,   // sw2021 一级行业 ID 0..31 (industry_component 月初 + industry_change 月内回放)

  // raw 自算 — ttm12 拼接 (财务 itf 暂未落地数据 → events 空 → 全 NaN)
  rev_raw,
  ni_raw,
  pe_raw,
  pcf_raw,
  roe_raw,
  roa_raw,

  // raw meta 派生 — per-A 动态 (D - list_date / D - delist_date)
  list_age,
  delist_age,

  // derived — 由 raw 推 (TS 内依赖)
  daily_return,
  low_p,
  low_mc,
  limit_up,
  limit_dn,

  // filter — 状态机或单点判, 1 = 排除 (D, A)
  profit_st,
  revenue_st,
  dividend_st,
  risk_warn,
  trading_st,
  new_list,

  // pool (TS) — asset 静态白名单 ∩ industry_l1 白名单 ∩ ¬susp ∩ ¬退市 ∩ (可选 ¬is_margin)
  pool_b,

  // ============================== CS ==============================
  // factor — winsor_mad → z → pct_rank, ∈ [0, 1]
  close,
  mcap,
  fmcap,
  pe_ttm12,
  pb_ttm3,
  ps_ttm12,
  pcf_ttm12,
  roe_ttm12,
  roa_ttm12,
  dy_ttm12,

  // pool (CS) — universe 母集
  pool,     // pool_b ∧ rank(mcap_raw asc) ≤ POOL_UNIVERSE_SIZE
  tradable, // pool ∧ ¬(filter OR), filter 列表 = config::STRATEGY_ENABLED_FILTERS

  // 加权合成因子分数 (限定 pool 内输出, 配置见 config::STRATEGY_FACTOR_WEIGHTS):
  //   factor_score[a, d] = Σ w_f · factor_f[a, d] / Σ w_f · 1{is_finite(factor_f[a, d])}
  //   pool[a, d] != 1 → NaN; 全 factor 缺 → NaN; 范围近似 [0, 1] (各 factor 已 pct_rank)
  factor_score,

  COUNT,
};

enum class Kind : uint8_t { Filter,
                            Factor,
                            Inter };
enum class Axis : uint8_t { TimeSeries,
                            CrossSection };

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
  Kind kind;
  Axis axis;
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
  if (yyyymmdd.size() < 4)
    return 0;
  return (yyyymmdd[0] - '0') * 1000 + (yyyymmdd[1] - '0') * 100 +
         (yyyymmdd[2] - '0') * 10 + (yyyymmdd[3] - '0');
}

inline int month_of(const std::string_view yyyymmdd) {
  if (yyyymmdd.size() < 6)
    return 0;
  return (yyyymmdd[4] - '0') * 10 + (yyyymmdd[5] - '0');
}

} // namespace feature
