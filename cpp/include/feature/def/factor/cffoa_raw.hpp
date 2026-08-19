#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// cffoa_raw = tanh((net_cffoa_ttm - net_cffoa_ttm_shift4) / mcap_raw)
//   经营现金流量净额改善率: 过去 12 个月 (TTM) 经营现金流量净额 相对 4 季度
//   (=12 个月) 前同口径 TTM 的增量, 按总市值定标 (denominator = mcap_raw).
//   分母不用 |c0|+|c4|: 该口径下但凡 c0/c4 一正一负 (~24% 的样本), c0-c4 与
//   |c0|+|c4| 恒等 ⇒ 比值死死钉在 ±1, 不管改善量是 1 元还是 1 亿元, 丢光了
//   "改善了多少" 的信息 (实测符号反转样本的该比值 100% 落在 ±1, std≈1 的双峰
//   分布); 分母也不用 c4 本身: c4 变号/趋近 0 时比值符号失真 / 量级发散.
//   改回按市值定标: 现金流增量相对公司体量的占比越大, |比值| 才越大 —
//   增量与市值不成比例 (即使跨零) 时比值自然趋近 0, 天然实现"太小的改善/
//   恶化压到接近 0", 不需要额外的显著性开关; 跨零只是让符号变了, 不会让比值
//   突变到 ±1. tanh 只在极端处 (增量比肩甚至超过市值本身, 千分之一强的样本)
//   平滑封顶到 [-1, 1], 常规范围内 tanh(x)≈x 几乎不改变原始比例, 优于硬截断.

namespace feature::def {

inline void ts_cffoa_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *cffoa_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec cffoa_raw_spec{
    "cffoa_raw", "现金流改善率", Kind::Inter, Axis::TimeSeries, cffoa_raw_deps,
    &ts_cffoa_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/
    "tanh((net_cffoa_ttm - net_cffoa_ttm_shift4) / mcap_raw)",
    /*assumption=*/
    "[-1,1]; 经营现金流量净额 12 个月 (TTM) 相对 4 季度前同口径 TTM 的增量, "
    "按总市值定标后 tanh 平滑封顶; 增量相对市值占比越小越趋近 0 (含跨零情形), "
    "占比越大 (含跨零) |值| 才越大, 只有增量比肩市值本身的极端样本才饱和到 "
    "±1; shift=4 值在 pit.cpp build 时按 (date, instrument) 与 shift=0 主记录 "
    "配对写入; mcap_raw ≤ 0 或任一侧非有限 → NaN"};

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
                                       ? std::tanh((c0 - c4) / m)
                                       : std::nanf("");
                          });
}

} // namespace feature::def
