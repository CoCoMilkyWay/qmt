#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cmath>

// pb_raw = mcap_raw / balance.total_equity_to_parent_shareholders (归母).
//   ttm1 瞬时估值 (MRQ); 支持负 PB (负权益); mcap<=0 是未上市/无价格哨兵, 不参与估值.
//   取归母而非含少数: 分子 mcap_raw 只是母公司股权市值, 分母含少数股东权益会两边
//   口径错配 (同一 mcap_raw 在 pe_raw 里配的也是归母净利). test/compare.py 实测
//   果仁亦用归母 (99.62% vs 含少数 50.16%).

namespace feature::def {

inline void ts_pb_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *pb_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec pb_raw_spec{
    "pb_raw", "市净率MRQ", Kind::Inter, Axis::TimeSeries, pb_raw_deps, &ts_pb_raw, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/
    "mcap_raw / balance.total_equity_to_parent_shareholders (取 latest "
    "report_date 的 latest visible 行)",
    /*assumption=*/
    "[ratio]; ttm1 (瞬时估值 / MRQ); 分母取归母 — 分子 mcap_raw 只是母公司 "
    "股权市值, 分母须同口径; 同 visible_date 多 report_date 取 max; 支持负 "
    "PB; 分母 == 0 或 mcap_raw ≤ 0 → NaN"};

inline void ts_pb_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(mcap_raw_spec, a);
  detail::scan_latest_balance(
      a, axes, pool, meta, T, pb_raw_spec,
      [&](int d, const FinancialBalanceEv &e) -> float {
        float m = mcap[d];
        float eq = e.total_equity_to_parent_shareholders;
        return (is_finite(m) && m > 0.0f && is_finite(eq) && eq != 0.0f)
                   ? m / eq
                   : std::nanf("");
      });
}

} // namespace feature::def
