#pragma once

// def/ 节点实现共享 helper (跨节点复用的纯计算/扫描逻辑, 不是节点本身).
//   仅被 def/{factor,filter}/*.hpp 节点文件 include; 不是公共消费面.

#include "feature/axis.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <span>
#include <string>

namespace feature::def::detail {

// 计算股票 a 的上市日索引 (在 axes.dates 中的 lower_bound).
// 返回 n_d 表示无有效 list_date (视为永远未上市).
inline int get_list_d(int a, const Axes &axes, const StockMeta &meta) {
  const std::string &ld = meta.list_date[a];
  if (ld.size() != 8)
    return axes.n_d();
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), ld);
  return static_cast<int>(std::distance(axes.dates.begin(), it));
}

// 仅上市前 raw 用 0 哨兵；上市后 NaN/Inf 必须保留，用来暴露数据问题。
inline void fill_before_list(std::span<float> out, int a, const Axes &axes,
                             const StockMeta &meta) {
  int hi = std::min(get_list_d(a, axes, meta), axes.n_d());
  std::fill(out.begin(), out.begin() + hi, 0.0f);
}

// 计算股票 a 的退市日索引 (在 axes.dates 中的 lower_bound)
// 返回 n_d 表示无有效 delist_date (视为永不退市)
inline int get_delist_d(int a, const Axes &axes, const StockMeta &meta) {
  const std::string &dd = meta.delist_date[a];
  if (dd.size() != 8)
    return axes.n_d(); // 无 delist_date → 永不退市
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), dd);
  return static_cast<int>(std::distance(axes.dates.begin(), it));
}

// 不再填 0，保留 NaN 让后续 stage 判断数据可用性
inline void fill_after_delist(std::span<float>, int, const Axes &,
                              const StockMeta &) {
}

// forecast 触发 → 终止 d:
//   off = min(对应 report_date 的正式 PIT 财报首次可见 row, 下一年 4 月 30 日 ceil)
//   注: r.v 已在 replay 时应用 cutoff = 首次可见 row D.
template <class FinancialEv>
int find_forecast_off_d(const ForecastEv &fe,
                        std::span<const FinancialEv> financials,
                        const Axes &axes) {
  int n_d = axes.n_d();
  int financial_d = -1;
  for (const auto &r : financials) {
    if (r.report_date == fe.end_date) {
      financial_d = r.v;
      break;
    }
  }
  int Y = year_of(fe.end_date);
  int deadline_d = -1;
  if (Y > 0) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d0430", Y + 1);
    auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(),
                               std::string(buf));
    deadline_d = (it == axes.dates.end())
                     ? n_d
                     : static_cast<int>(std::distance(axes.dates.begin(), it));
  }
  int off = n_d;
  if (financial_d >= 0)
    off = std::min(off, financial_d);
  if (deadline_d >= 0)
    off = std::min(off, deadline_d);
  return off;
}

// 模板: 网格 float 字段 → ts_row; NaN 透传保留数据问题.
//   fill_pre_list=true: 上市前哨兵置 0 (close/mcap/share 等估值类);
//   fill_pre_list=false: 全期保留 NaN (margin balance 等"不存在=NaN").
//   src 用 PoolArr<float> (与 PitPool 字段一致, 兼容 mmap view 与 owned 两态).
inline void grid_copy(int a, const Axes &axes, const StockMeta &meta, Tensor &T,
                      const FeatureSpec &dst, const PoolArr<float> &src,
                      bool fill_pre_list = true) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(dst, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d)];
  }
  if (fill_pre_list)
    fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// 模板: 网格 uint8_t bool 字段 → ts_row (1.0 / 0.0); 不调 fill_* (bool 0 = "无", 与 NaN 不同).
inline void grid_copy_bool(int a, const Axes &axes, Tensor &T,
                           const FeatureSpec &dst,
                           const PoolArr<std::uint8_t> &src) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(dst, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d)] ? 1.0f : 0.0f;
  }
}

// ----------------------------------------------------------------------------
// 财务 raw 共享 helper (BigQuant cn_stock_financial_*).
//
// 设计:
//   - ttm 流 (cn_stock_financial_ttm_shift, shift=0): per-A 沿 v 升序, 取 latest event
//     (max v); shift=0 已锁定"该 visible_date 的最新报告期 TTM", 跨披露天直接 max v.
//   - balance 流 (cn_stock_financial_balance_general_pit): per-A 沿 v 升序, 维护
//     map<report_date, latest_ev>; 取 max(report_date) 当 latest event (瞬时估值 / MRQ).
//     反例: max v 可能取到对旧 report_date 的修正 (低于新 quarter), 会错位.
//   - income annual 流 (cn_stock_financial_income_general_pit, fs_quarter_index=4):
//     per-A 沿 v 升序, 维护 map<report_date, {val, last_v}>; ni_raw 取 last_v 最大
//     的 2 条均值, 用于 dividend_st 阈值稳定 (单年 NI 波动大易误触发).
//
// PIT 安全: 网格 / 事件 ev.v 已在 pit.cpp replay 时应用 raw cutoff, ev.v <= d 即
//   "T 当日已可见". 所有 helper / ts_* 不再做时间偏移.
//
// 上市前事件丢弃 (ev.v < list_d): BigQuant 在上市日前就把招股书口径的财务行标为
//   可见 (实测 ttm 10.9% / balance 14.5% 的事件 visible_date < list_date), 且这批
//   值不可信 (例 300417 上市前 rev_ttm = -1247 万). 这里按 list_d 截断而非事后
//   fill_before_list — 后者填 0 哨兵会让 revenue_st 的 "rev_raw < 阈值" 恒真.
//   丢弃后上市前自然留 NaN = "无数据".
// ----------------------------------------------------------------------------

// per-A 走 ttm 事件流, 对每个 d 拿到 latest event 指针 (或 nullptr 没就绪).
// compute(d, ev*) 写 out[d]; ev==nullptr 时 helper 自动写 NaN.
template <class Compute>
inline void scan_latest_ttm(int a, const Axes &axes, const PitPool &pool,
                            const StockMeta &meta, Tensor &T,
                            const FeatureSpec &dst, Compute compute) {
  int n_d = axes.n_d();
  int list_d = get_list_d(a, axes, meta);
  auto out = T.ts_row(dst, a);
  const auto &events = pool.financial_ttm[a];
  std::size_t ep = 0;
  int last_idx = -1;
  for (int d = 0; d < n_d; ++d) {
    while (ep < events.size() && events[ep].v <= d) {
      if (events[ep].v >= list_d)
        last_idx = static_cast<int>(ep);
      ++ep;
    }
    out[d] = (last_idx >= 0) ? compute(d, events[last_idx]) : std::nanf("");
  }
  fill_after_delist(out, a, axes, meta);
}

// per-A 走 balance 事件流, 维护 map<rd, ev>; 对每个 d 拿到 max(report_date) event.
template <class Compute>
inline void scan_latest_balance(int a, const Axes &axes, const PitPool &pool,
                                const StockMeta &meta, Tensor &T,
                                const FeatureSpec &dst, Compute compute) {
  int n_d = axes.n_d();
  int list_d = get_list_d(a, axes, meta);
  auto out = T.ts_row(dst, a);
  const auto &events = pool.financial_balance[a];
  std::map<std::int32_t, FinancialBalanceEv> latest_by_rd;
  std::size_t ep = 0;
  for (int d = 0; d < n_d; ++d) {
    while (ep < events.size() && events[ep].v <= d) {
      if (events[ep].v >= list_d)
        latest_by_rd[events[ep].report_date] = events[ep];
      ++ep;
    }
    if (latest_by_rd.empty()) {
      out[d] = std::nanf("");
      continue;
    }
    out[d] = compute(d, latest_by_rd.rbegin()->second);
  }
  fill_after_delist(out, a, axes, meta);
}

// report_date (yyyymmdd) 的上一个季末; 非标准季末 → 0 (调用方视为链断).
inline std::int32_t prev_quarter_end(std::int32_t rd) {
  std::int32_t y = rd / 10000, md = rd % 10000;
  switch (md) {
  case 1231:
    return y * 10000 + 930;
  case 930:
    return y * 10000 + 630;
  case 630:
    return y * 10000 + 331;
  case 331:
    return (y - 1) * 10000 + 1231;
  default:
    return 0;
  }
}

// TTM 窗口 5 点平均: anchor (= ttm 事件的 report_date) + 前 4 个季末.
//   任一点缺失 / 非有限 → NaN (窗口不完整不给近似值; 次新股上市头 ~15 个月
//   自然落在这里). map 存的是各 report_date 当前可见的最新版本 ⇒ PIT 自洽.
inline float ttm_window_avg(
    std::int32_t anchor,
    const std::map<std::int32_t, FinancialBalanceEv> &by_rd,
    float FinancialBalanceEv::*field) {
  double sum = 0.0;
  std::int32_t rd = anchor;
  for (int i = 0; i < 5; ++i) {
    if (rd == 0)
      return std::nanf("");
    auto it = by_rd.find(rd);
    if (it == by_rd.end())
      return std::nanf("");
    float v = it->second.*field;
    if (!is_finite(v))
      return std::nanf("");
    sum += static_cast<double>(v);
    rd = prev_quarter_end(rd);
  }
  return static_cast<float>(sum / 5.0);
}

// per-A 同时走 ttm + balance 两路事件流 (roe_raw / roa_raw 用).
template <class Compute>
inline void scan_latest_ttm_and_balance(int a, const Axes &axes,
                                        const PitPool &pool,
                                        const StockMeta &meta, Tensor &T,
                                        const FeatureSpec &dst,
                                        Compute compute) {
  int n_d = axes.n_d();
  int list_d = get_list_d(a, axes, meta);
  auto out = T.ts_row(dst, a);
  const auto &ttms = pool.financial_ttm[a];
  const auto &bals = pool.financial_balance[a];
  std::map<std::int32_t, FinancialBalanceEv> latest_by_rd;
  std::size_t tp = 0, bp = 0;
  int last_ttm = -1;
  for (int d = 0; d < n_d; ++d) {
    while (tp < ttms.size() && ttms[tp].v <= d) {
      if (ttms[tp].v >= list_d)
        last_ttm = static_cast<int>(tp);
      ++tp;
    }
    while (bp < bals.size() && bals[bp].v <= d) {
      if (bals[bp].v >= list_d)
        latest_by_rd[bals[bp].report_date] = bals[bp];
      ++bp;
    }
    if (last_ttm < 0 || latest_by_rd.empty()) {
      out[d] = std::nanf("");
      continue;
    }
    out[d] = compute(d, ttms[last_ttm], latest_by_rd);
  }
  fill_after_delist(out, a, axes, meta);
}

} // namespace feature::def::detail
