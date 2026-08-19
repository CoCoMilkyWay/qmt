#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

// ni_raw: 仅取 fs_quarter_index==4 年报 (aggregate 已过滤);
//   维护 map<report_date, {val, last_v}>, 取 last_v 最大 2 条均值 (smooth 阈值);
//   单条退 1 条; 0 条 NaN. 给 dividend_st 阈值用.

namespace feature::def {

inline void ts_ni_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec ni_raw_spec{
    "ni_raw", Kind::Inter, Axis::TimeSeries, {}, &ts_ni_raw, nullptr};

inline void ts_ni_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(ni_raw_spec, a);
  std::fill(out.begin(), out.end(), std::nanf(""));

  struct Cell {
    float val;
    int last_v;
  };
  std::vector<std::pair<std::int32_t, Cell>> annuals;
  std::size_t ev_ptr = 0;
  const auto &events = pool.financial_income_annual[a];

  auto annuals_find = [&](std::int32_t k) -> int {
    for (std::size_t i = 0; i < annuals.size(); ++i)
      if (annuals[i].first == k)
        return static_cast<int>(i);
    return -1;
  };

  for (int d = 0; d < n_d; ++d) {
    while (ev_ptr < events.size() && events[ev_ptr].v <= d) {
      const auto &e = events[ev_ptr++];
      if (!is_finite(e.net_profit_to_parent_shareholders))
        continue;
      int idx = annuals_find(e.report_date);
      if (idx < 0)
        annuals.emplace_back(e.report_date,
                             Cell{e.net_profit_to_parent_shareholders, e.v});
      else
        annuals[idx].second = Cell{e.net_profit_to_parent_shareholders, e.v};
    }
    if (annuals.empty())
      continue;

    int i0 = -1, i1 = -1;
    int v0 = -1, v1 = -1;
    for (std::size_t i = 0; i < annuals.size(); ++i) {
      int v = annuals[i].second.last_v;
      if (v > v0) {
        v1 = v0;
        i1 = i0;
        v0 = v;
        i0 = static_cast<int>(i);
      } else if (v > v1) {
        v1 = v;
        i1 = static_cast<int>(i);
      }
    }
    if (i0 >= 0 && i1 >= 0)
      out[d] = (annuals[i0].second.val + annuals[i1].second.val) * 0.5f;
    else if (i0 >= 0)
      out[d] = annuals[i0].second.val;
  }
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
