#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// cffoa_raw = net_cffoa_ttm / net_cffoa_ttm_shift4 - 1
//   经营现金流量净额增长: 过去 12 个月 (TTM) 经营现金流量净额 相对
//   4 季度 (=12 个月) 前同口径 TTM 的增长率. shift=4 值在 pit.cpp build 时
//   按 (date, instrument) 与 shift=0 主记录配对写入; 分母==0 或任一侧
//   非有限 → NaN (分子分母皆可负, 不剔, 交给截面 winsor_mad 兜底).

namespace feature::def {

inline void ts_cffoa_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec cffoa_raw_spec{
    "cffoa_raw", "现金流增速", Kind::Inter, Axis::TimeSeries, {}, &ts_cffoa_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"net_cffoa_ttm / net_cffoa_ttm_shift4 - 1",
    /*assumption=*/
    "[ratio]; 经营现金流量净额 12 个月 (TTM) 相对 4 季度前同口径 TTM 的增长率; "
    "shift=4 值在 pit.cpp build 时按 (date, instrument) 与 shift=0 主记录配对写入; "
    "分子分母皆可负不剔, 交给截面 winsor_mad 兜底; 分母==0 或任一侧非有限 → NaN"};

inline void ts_cffoa_raw(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T) {
  detail::scan_latest_ttm(a, axes, pool, meta, T, cffoa_raw_spec,
                          [](int /*d*/, const FinancialTtmEv &e) -> float {
                            float c0 = e.net_cffoa_ttm;
                            float c4 = e.net_cffoa_ttm_shift4;
                            return (is_finite(c0) && is_finite(c4) && c4 != 0.0f)
                                       ? (c0 / c4 - 1.0f)
                                       : std::nanf("");
                          });
}

} // namespace feature::def
