#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// cffoa_raw = (net_cffoa_ttm - net_cffoa_ttm_shift4) / mcap_raw
//   经营现金流量净额改善收益率: 过去 12 个月 (TTM) 经营现金流量净额 相对
//   4 季度 (=12 个月) 前同口径 TTM 的增量, 按总市值归一. shift=4 值在
//   pit.cpp build 时按 (date, instrument) 与 shift=0 主记录配对写入.
//   分母取 mcap_raw (恒正) 而非 shift4 值: 经营现金流本身有 ~23% 的格子为负,
//   比值型 c0/c4-1 在 c4<0 时符号含义翻转 (由负转正算出大负数, 亏损翻倍算出
//   大正数), 且 c4→0 时量级爆炸; 按市值归一后符号单调、尺度可比.

namespace feature::def {

inline void ts_cffoa_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *cffoa_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec cffoa_raw_spec{
    "cffoa_raw", "现金流改善收益率", Kind::Inter, Axis::TimeSeries, cffoa_raw_deps,
    &ts_cffoa_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"(net_cffoa_ttm - net_cffoa_ttm_shift4) / mcap_raw",
    /*assumption=*/
    "[ratio]; 经营现金流量净额 12 个月 (TTM) 相对 4 季度前同口径 TTM 的增量, "
    "按总市值归一; shift=4 值在 pit.cpp build 时按 (date, instrument) 与 shift=0 "
    "主记录配对写入; 分子可负 (现金流恶化) 不剔, 分母 mcap_raw 恒正 ⇒ 符号单调; "
    "mcap_raw ≤ 0 或任一侧非有限 → NaN"};

inline void ts_cffoa_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(mcap_raw_spec, a);
  detail::scan_latest_ttm(a, axes, pool, meta, T, cffoa_raw_spec,
                          [&](int d, const FinancialTtmEv &e) -> float {
                            float m = mcap[d];
                            float c0 = e.net_cffoa_ttm;
                            float c4 = e.net_cffoa_ttm_shift4;
                            return (is_finite(m) && m > 0.0f && is_finite(c0) &&
                                    is_finite(c4))
                                       ? (c0 - c4) / m
                                       : std::nanf("");
                          });
}

} // namespace feature::def
