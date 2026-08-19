#pragma once

#include "feature/axis.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <cstddef>
#include <cstdint>

// filter: risk_warn — ST/*ST/退市整理期 4 态 (0/1/2/3), 直读 status.st_status.

namespace feature::def {

inline void ts_risk_warn(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec risk_warn_spec{
    "risk_warn", "风险预警", Kind::Filter, Axis::TimeSeries, {}, &ts_risk_warn, nullptr,
    /*must_be_finite=*/true,
    /*formula=*/
    "派生 4 态 (int8 → float; 0=正常, 1=ST, 2=*ST, 3=退市整理期): 历史日由 "
    "cn_stock_status.st_status (1/2 → 1/2) ∧ is_risk_warning (st_status==0 ∧ "
    "rw!=0 → 3) 派生; 实盘当日由 cn_stock_static_data.in_delist (=1 → 3) ∧ "
    "st_status 派生",
    /*assumption=*/"—"};

// risk_warn: 直读 pool.status.st_status (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充).
//   pit.cpp itf_cn_stock_status::replay + apply_meta_overlays 已派生 4 态:
//     0=正常 / 1=ST / 2=*ST / 3=退市整理期 (int8 → float 直接 cast).
//   数据起点前一律 0 (prealloc 为 0, 文件不存在时不写, 保持初值).
//   注: 退市整理期靠 4 态识别 — 交易所摘 *ST 标签后狭义 st_status 翻 0, 仅靠
//       原始 st_status 漏判会被 strategy 选中持有至退市 (实测 *ST大通 2023/06/19
//       进退市整理期 → 漏排 → 持有 11 个交易日至退市). 派生规则见 pit.cpp.
//   下游 pool 把 risk_warn > 0.5 视为排除 (1.0/2.0/3.0 均触发 filter).
inline void ts_risk_warn(int a, const Axes &axes, const PitPool &pool,
                         const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(risk_warn_spec, a);
  std::size_t base =
      static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  for (int d = 0; d < n_d; ++d) {
    int8_t st = pool.status.st_status[base + static_cast<std::size_t>(d)];
    out[d] = static_cast<float>(st);
  }
}

} // namespace feature::def
