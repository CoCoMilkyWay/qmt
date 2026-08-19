#pragma once

#include "feature/axis.hpp"
#include "feature/def/detail.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <vector>

// dy_raw = Σ(cash_before_tax × share_raw[ev.v] for ev.v ∈ (D-365d, D]) / mcap_raw[D]
//   窗口锚在事件自身的可见日 ev.v (← publish_date, 预案公告日), 不是 ex_date:
//     分红是公告即定价的信息事件, 除权日常滞后公告 2-4 个月, 用 ex_date 会让因子
//     整体延迟一个季度. test/diag_dy.py 八组合实测: 换公告日锚 +10.6pp (70.9→81.6).
//   每股金额用税前 cash_before_tax (行业通行的股息率口径, Wind/BigQuant 同), +2.5pp.
//   分红总额 = 每股 × 公告当日股本 (share_raw[ev.v]) 而非当前股本: 事后送转/增发
//     会把股本放大, 配当年的每股分红是两边错配 (公告日快照才等于真实派现总额), +6.6pp.
//     dividend_st 的 3y 累计现金分红早已是这个口径.
//   合计 70.9% → 88.2% (对 test/1.csv 的 股息率TTM).
//   PIT: ev.v 已含 CUTOFF=-1, 天然不含 T 当日才公告的方案.
//   无事件 → 0 (不是 NaN; 0 = "无近期分红", 横截面排最低位); mcap 缺/≤0 → NaN.

namespace feature::def {

inline void ts_dy_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T);

inline constexpr const FeatureSpec *dy_raw_deps[] = {&mcap_raw_spec};

inline constexpr FeatureSpec dy_raw_spec{
    "dy_raw", Kind::Inter, Axis::TimeSeries, dy_raw_deps, &ts_dy_raw, nullptr};

inline void ts_dy_raw(int a, const Axes &axes, const PitPool &pool,
                      const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto mcap = T.ts_row(mcap_raw_spec, a);
  auto out = T.ts_row(dy_raw_spec, a);
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const auto &shares = pool.shares.total_shares;
  const auto &divs = pool.dividend[a];

  // 事件已按 v 升序 (EventStore::sort_chains) ⇒ 直接双指针, 无需另排.
  //   amt[i] = 该次分红的派现总额 [元]; 股本快照缺失 / 非正每股 → 0 (不入窗).
  std::vector<float> amt(divs.size(), 0.0f);
  for (std::size_t i = 0; i < divs.size(); ++i) {
    float c = divs[i].cash_before_tax;
    if (!is_finite(c) || c <= 0.0f)
      continue;
    float sh = shares[base + static_cast<std::size_t>(divs[i].v)];
    if (is_finite(sh))
      amt[i] = c * sh;
  }

  // 双指针滑窗 cash_sum: hi 是即将入窗事件下标, lo 是已弹出事件下标 (= 当前窗
  //   含 [lo, hi) 区间事件). 窗口为空 (lo == hi) 时显式归零, 防 += / -= 浮点
  //   累计漂移 (实测后期年份 4-5% 样本会漂到 -1e-7 微负). 输出再 max(0, ·) 兜底
  //   非空窗内的漂移, 强约束 dy_raw ∈ [0, +∞).
  std::size_t lo = 0, hi = 0;
  float cash_sum = 0.0f;
  for (int d = 0; d < n_d; ++d) {
    auto Tlo = axes.date_days[d] - std::chrono::days{365};
    while (hi < divs.size() && divs[hi].v <= d) {
      cash_sum += amt[hi];
      ++hi;
    }
    while (lo < hi && axes.date_days[divs[lo].v] <= Tlo) {
      cash_sum -= amt[lo];
      ++lo;
    }
    if (lo >= hi)
      cash_sum = 0.0f;
    float m = mcap[d];
    if (!is_finite(m) || m <= 0.0f) {
      out[d] = std::nanf("");
    } else {
      float dy = cash_sum / m;
      out[d] = (dy > 0.0f) ? dy : 0.0f;
    }
  }
  detail::fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def
