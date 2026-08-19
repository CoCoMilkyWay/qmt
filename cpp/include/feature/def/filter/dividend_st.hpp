#pragma once

#include "feature/axis.hpp"
#include "feature/def/factor/ni_raw.hpp"
#include "feature/def/factor/share_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

// filter: dividend_st — 主板分红不足预警 (3y 累计现金分红双阈 ∧ ni_raw > 0).
//   阶梯 forward fill: 每 dividend event 重算 3y_sum, 区间 [e.v, next.v) 内判定.

namespace feature::def {

inline void ts_dividend_st(int a, const Axes &axes, const PitPool &pool,
                           const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *dividend_st_deps[] = {&share_raw_spec,
                                                           &ni_raw_spec};

inline constexpr FeatureSpec dividend_st_spec{
    "dividend_st", "分红预警", Kind::Filter, Axis::TimeSeries, dividend_st_deps,
    &ts_dividend_st, nullptr, /*must_be_finite=*/true,
    /*formula=*/
    "meta.list_sector == 1 ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_after_tax × "
    "share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7",
    /*assumption=*/
    "3y 窗口 = dividend.report_date.Y ∈ [Y-3, Y-1] (Y = "
    "dividend.publish_date.Y); share_raw 取 publish_date 当日快照; 单位均 [元]"};

// dividend_st: 阶梯 forward fill — 每 dividend event 重算 3y_sum,
//   3y_sum = Σ over 历史 events with report_date.Y in [ann_y-3, ann_y-1]
//            的 cash_after_tax × share_raw[event.v]; 区间 [e.v, next.v) 填.
//   注: e.v 已是首次可见 row D; ann_y 用 axes.dates[e.v - 1] 还原 visible 日期年份.
//
// 暖机期 (warmup_d 之前) 一律不命中, 避免 3y 回望不完整时偏严:
//   1) 数据轴边界: axes 起点前事件不可见 → 起点后 3 年内 3y 窗口必然不完整
//   2) 股票自身: 上市后 3 年内分红记录天然少, 不视为 ST
//   warmup_d = max(axes_warmup_d, stock_warmup_d), 取较晚者.
inline void ts_dividend_st(int a, const Axes &axes, const PitPool &pool,
                           const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(dividend_st_spec, a);
  auto share_raw = T.ts_row(share_raw_spec, a);
  auto ni_raw = T.ts_row(ni_raw_spec, a);
  std::fill(out.begin(), out.end(), 0.0f);

  bool mb_a = (meta.list_sector[a] == 1); // 仅主板适用 (asset 静态, 1=主板)
  if (!mb_a)
    return;

  int axes_start_y = (n_d > 0) ? year_of(axes.dates[0]) : 0;
  int axes_warmup_d = n_d;
  for (int d = 0; d < n_d; ++d) {
    if (year_of(axes.dates[d]) >= axes_start_y + 3) {
      axes_warmup_d = d;
      break;
    }
  }
  int stock_warmup_d = n_d;
  if (meta.list_date[a].size() == 8) {
    int list_y = year_of(meta.list_date[a]);
    for (int d = 0; d < n_d; ++d) {
      if (year_of(axes.dates[d]) >= list_y + 3) {
        stock_warmup_d = d;
        break;
      }
    }
  }
  int warmup_d = std::max(axes_warmup_d, stock_warmup_d);

  const auto &divs = pool.dividend[a];

  auto apply_segment = [&](int seg_start, int seg_end, float val_3ysum) {
    if (seg_start < warmup_d)
      seg_start = warmup_d;
    if (seg_start < 0)
      seg_start = 0;
    if (seg_end > n_d)
      seg_end = n_d;
    for (int d = seg_start; d < seg_end; ++d) {
      if (!is_finite(ni_raw[d]) || ni_raw[d] <= 0.0f)
        continue;
      if (val_3ysum < 0.30f * ni_raw[d] && val_3ysum < 5e7f) {
        out[d] = 1.0f;
      }
    }
  };

  float current_3ysum = 0.0f;
  int next_apply_d = 0;

  for (std::size_t ev_idx = 0; ev_idx < divs.size(); ++ev_idx) {
    const auto &e = divs[ev_idx];
    int e_d = e.v;
    apply_segment(next_apply_d, e_d, current_3ysum);
    next_apply_d = e_d;

    // 业务判 dividend.ann_date 年份: e.v-1 是 visible_d
    int ann_y = (e.v >= 1 && e.v < n_d) ? year_of(axes.dates[e.v - 1]) : 0;
    if (ann_y == 0)
      continue;
    int lo = ann_y - 3, hi = ann_y - 1;

    float sum = 0.0f;
    for (std::size_t j = 0; j <= ev_idx; ++j) {
      const auto &p = divs[j];
      int py = year_of(p.report_date);
      if (py < lo || py > hi)
        continue;
      if (!is_finite(p.cash_after_tax))
        continue;
      int p_d = p.v;
      float sh = (p_d >= 0 && p_d < n_d) ? share_raw[p_d] : std::nanf("");
      if (!is_finite(sh))
        continue;
      sum += p.cash_after_tax * sh;
    }
    current_3ysum = sum;
  }
  apply_segment(next_apply_d, n_d, current_3ysum);
}

} // namespace feature::def
