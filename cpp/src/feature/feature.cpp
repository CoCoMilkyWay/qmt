#include "feature/feature.hpp"

#include "feature/axis.hpp"
#include "feature/cs.hpp"
#include "feature/industry.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"
#include "feature/ts.hpp"

#include "config.hpp"
#include "misc/date.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ============================================================================
// feature.cpp 是「特征字段表单点」: 每个 feature 一个 compute fn (TsComputeFn /
//   CsComputeFn 签名), 末尾 FEATURES[] 表挂载. ts.cpp / cs.cpp / build.cpp
//   仅看 FEATURES[] 迭代调度, 不出现具体 feature 名.
//
//   增减 feature:
//     1) feature.hpp 在 F 枚举对应位置加一行 (位置即计算顺序)
//     2) feature.cpp 新增 ts_<name> / cs_<name> compute fn
//     3) FEATURES[] 末尾对应位置加一行挂载
//
//   compute fn 内部:
//     - TS 全部签名 (int a, axes, pool, meta, T); 不需要的子集忽略.
//     - CS 全部签名 (int d, axes, T, bufs); bufs.a/b/c 是 thread-local n_a-长 buffer.
//     - 写自己的 ts_row(F::self, a) 或 scatter_cs_row(F::self, d, ...);
//     - 可读 PitPool / StockMeta / 已写就的 T.ts_row(prior_f, a) (顺序由 enum 保证).
// ============================================================================

namespace feature {

// ============================================================================
// 局部 helper (跨多个 state machine 共用)
// ============================================================================

namespace {

// pool_b 板块/行业白名单查表 (集中定义见 config.hpp).
//   未覆盖 (空串) 一律不命中 → pool_b 全期 0.
template <std::size_t N>
inline bool in_whitelist(std::string_view v,
                         const std::array<std::string_view, N> &wl) {
  if (v.empty())
    return false;
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

// list_sector 整数白名单查表 (config::POOL_LIST_SECTOR_WHITELIST 是 int8 集合).
template <std::size_t N>
inline bool in_int_whitelist(int8_t v, const std::array<int8_t, N> &wl) {
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

// industry_l1 ID 白名单 mask: 把 config::POOL_INDUSTRY_L1_WHITELIST (中文 string_view)
//   转 array<bool, 32> mask, mask[id]=true 表该 SW2021 一级行业 ID 在白名单内.
//   首次调用 lazy 构建, 后续只读. 失败的中文名 (拼写不在 SW2021_L1_NAMES 内) → id=0
//   被忽略 (不会触发 mask[0], 因为 industry_l1=0 = "未知" 永远不该入白名单).
inline const std::array<bool, SW2021_L1_COUNT> &industry_l1_whitelist_mask() {
  static const auto mask = []() {
    std::array<bool, SW2021_L1_COUNT> m{};
    for (auto name : ::config::POOL_INDUSTRY_L1_WHITELIST) {
      uint8_t id = sw2021_l1_name_to_id(name);
      if (id != 0)
        m[id] = true;
    }
    return m;
  }();
  return mask;
}

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

} // namespace

// ============================================================================
// TS: raw 网格 — PitPool dense 直读 (row D 已 cutoff). 形状统一 [n_a, n_d].
//   每个 raw 网格 feature = 一行 helper. 对仗: (src grid 字段) → (out span) [× scale].
// ============================================================================

namespace impl {

namespace {

// 模板: 网格 float 字段 → ts_row; NaN 透传保留数据问题.
//   fill_pre_list=true: 上市前哨兵置 0 (close/mcap/share 等估值类);
//   fill_pre_list=false: 全期保留 NaN (margin balance 等"不存在=NaN").
//   src 用 PoolArr<float> (与 PitPool 字段一致, 兼容 mmap view 与 owned 两态).
inline void grid_copy(int a, const Axes &axes, const StockMeta &meta, Tensor &T,
                      F dst, const PoolArr<float> &src,
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
inline void grid_copy_bool(int a, const Axes &axes, Tensor &T, F dst,
                           const PoolArr<std::uint8_t> &src) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(dst, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d)] ? 1.0f : 0.0f;
  }
}

} // namespace

// close_raw ← cn_stock_real_bar1d.close (不复权 [元/股]).
//   row D = D-1 实际收盘 (CUTOFF=-1). 不复权口径: close 是当日真实成交价,
//   PIT-immutable (历史不随任何除权动作改写), 与 limit_price / total_shares
//   同口径. daily_return 直接基于该 close 链式, 除权日含分红/送股的真实跳跃.
void ts_close_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::close_raw, pool.bar1d.close);
}

// mcap_raw = close_raw[d] × shares.total_shares[a, d]  ([元])
//   close_raw 即不复权 close (← cn_stock_real_bar1d.close), 与 total_shares 同口径,
//   mcap_raw = 真实总市值 [元]; cross-section 排序与 low_mc 阈值判定均正确.
void ts_mcap_raw(int a, const Axes &axes, const PitPool &pool,
                 const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::mcap_raw, a);
  const auto &sh = pool.shares.total_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// fmcap_raw = close_raw[d] × shares.a_float_shares[a, d]  ([元])
//   BigQuant `float_market_cap` 实测口径 = close × a_float (A 股流通);
//   total_float 含 H 股, 002594/BYD 等 H+A 双重上市股大幅偏差.
void ts_fmcap_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::fmcap_raw, a);
  const auto &sh = pool.shares.a_float_shares;
  for (int d = 0; d < n_d; ++d) {
    float c = cl[d];
    float s = sh[base + static_cast<std::size_t>(d)];
    out[d] = (is_finite(c) && is_finite(s)) ? c * s : std::nanf("");
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}

// share_raw ← cn_stock_shares.total_shares ([股]; 直读, 无单位换算)
void ts_share_raw(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::share_raw, pool.shares.total_shares);
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
                            const StockMeta &meta, Tensor &T, F dst,
                            Compute compute) {
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
                                const StockMeta &meta, Tensor &T, F dst,
                                Compute compute) {
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

// pb_raw = mcap_raw / balance.total_equity_to_parent_shareholders (归母).
//   ttm1 瞬时估值 (MRQ); 支持负 PB (负权益); mcap<=0 是未上市/无价格哨兵, 不参与估值.
//   取归母而非含少数: 分子 mcap_raw 只是母公司股权市值, 分母含少数股东权益会两边
//   口径错配 (同一 mcap_raw 在 pe_raw 里配的也是归母净利). test/compare.py 实测
//   果仁亦用归母 (99.62% vs 含少数 50.16%).
void ts_pb_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(F::mcap_raw, a);
  scan_latest_balance(a, axes, pool, meta, T, F::pb_raw,
                      [&](int d, const FinancialBalanceEv &e) -> float {
                        float m = mcap[d];
                        float eq = e.total_equity_to_parent_shareholders;
                        return (is_finite(m) && m > 0.0f &&
                                is_finite(eq) && eq != 0.0f)
                                   ? m / eq
                                   : std::nanf("");
                      });
}

// ps_raw = mcap_raw / ttm.total_operating_revenue_ttm
//   分母用 total_operating_revenue_ttm (含利息/保费; BigQuant 实测口径) 而非
//   operating_revenue_ttm; 600519 茅台等 2% 误差排查得.
//   分母 <= 0 → NaN: 营收为负物理不可能, 是 BigQuant 脏值 (实测 1.35% 事件为负,
//   含 208 条已上市多年的, 例 600606 借壳期 rev_ttm = -312 亿), 不给排序含义.
//   mcap<=0 是未上市/无价格哨兵, 不参与估值.
void ts_ps_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(F::mcap_raw, a);
  scan_latest_ttm(a, axes, pool, meta, T, F::ps_raw,
                  [&](int d, const FinancialTtmEv &e) -> float {
                    float m = mcap[d];
                    float r = e.total_operating_revenue_ttm;
                    return (is_finite(m) && m > 0.0f &&
                            is_finite(r) && r > 0.0f)
                               ? m / r
                               : std::nanf("");
                  });
}

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
void ts_dy_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto mcap = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::dy_raw, a);
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
  fill_after_delist(out, a, axes, meta);
}

// up_lim / dn_lim 主动 -1: limit_price (CUTOFF=-1) 取的是 row D 当日适用涨跌停
//   (基于 D-1 close × 1.1/1.2 来); close_raw[D] (CUTOFF=-1) = D-1 实际收盘.
//   想判 "D-1 日是否封板" 应用 D-1 适用涨跌停, 即 limit_price[a, d-1]. ts_*_lim
//   主动 -1 完成此对齐. d=0 处无前一交易日 → NaN.
namespace {
inline void ts_lim_shift1(int a, const Axes &axes, const StockMeta &meta,
                          Tensor &T, F dst, const PoolArr<float> &src) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  auto out = T.ts_row(dst, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    out[d] = src[base + static_cast<std::size_t>(d - 1)];
  }
  fill_before_list(out, a, axes, meta);
  fill_after_delist(out, a, axes, meta);
}
} // namespace

void ts_up_lim(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  ts_lim_shift1(a, axes, meta, T, F::up_lim, pool.limit_price.upper_limit);
}

void ts_dn_lim(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  ts_lim_shift1(a, axes, meta, T, F::dn_lim, pool.limit_price.lower_limit);
}

// susp ← cn_stock_status.suspended (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充)
void ts_susp(int a, const Axes &axes, const PitPool &pool, const StockMeta &,
             Tensor &T) {
  grid_copy_bool(a, axes, T, F::susp, pool.status.suspended);
}

// is_margin ← margin_detail (D, A) 存在性 (CUTOFF=0)
void ts_is_margin(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  grid_copy_bool(a, axes, T, F::is_margin, pool.margin_detail.is_margin);
}

void ts_mr_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::mr_bal_raw,
            pool.margin_detail.financing_balance, false);
}

void ts_ms_bal_raw(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  grid_copy(a, axes, meta, T, F::ms_bal_raw,
            pool.margin_detail.securities_lending_balance, false);
}

// industry_l1: SW2021 一级行业 ID per (D, A), 0=未知, 1..31 见 industry.hpp.
//   合并 industry_component (月初快照) + industry_change (月内 change_flag=1 进入)
//   两个事件流, per-A 按 v 升序回放, last_l1_id 写每行. 上市前/无事件期保持 0.
//   退市后 last_l1_id 残留 (pool_b 已用 ¬is_finite(delist_age) 兜底排除, 不影响下游).
void ts_industry_l1(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::industry_l1, a);
  std::fill(out.begin(), out.end(), 0.0f);

  const auto &comp = pool.industry_component[a];
  const auto &chg = pool.industry_change[a];
  std::size_t ic = 0, ig = 0;
  uint8_t last_id = 0;

  for (int d = 0; d < n_d; ++d) {
    while (ic < comp.size() && comp[ic].v <= d) {
      last_id = comp[ic].l1_id;
      ++ic;
    }
    while (ig < chg.size() && chg[ig].v <= d) {
      last_id = chg[ig].l1_id;
      ++ig;
    }
    out[d] = static_cast<float>(last_id);
  }
}

// ============================================================================
// TS: raw 自算 — BigQuant 财务 itf 直读 (依赖 mcap_raw 已就绪 → enum 顺序保证)
// ============================================================================

// rev_raw ← ttm.total_operating_revenue_ttm; 给 revenue_st 过滤用 (同 ps_raw 分母).
//   <= 0 → NaN (同 ps_raw: 负营收是 BigQuant 脏值). 这里尤其要紧 — revenue_st 判
//   "rev_raw < 3e8/1e8", 负值会让阈值恒真, 把脏数据直接变成误报的退市预警.
void ts_rev_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  scan_latest_ttm(a, axes, pool, meta, T, F::rev_raw,
                  [](int /*d*/, const FinancialTtmEv &e) -> float {
                    float r = e.total_operating_revenue_ttm;
                    return (is_finite(r) && r > 0.0f) ? r : std::nanf("");
                  });
}

// ni_raw: 仅取 fs_quarter_index==4 年报 (aggregate 已过滤);
//   维护 map<report_date, {val, last_v}>, 取 last_v 最大 2 条均值 (smooth 阈值);
//   单条退 1 条; 0 条 NaN. 给 dividend_st 阈值用.
void ts_ni_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::ni_raw, a);
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
  fill_after_delist(out, a, axes, meta);
}

// pe_raw = mcap_raw / ttm.net_profit_to_parent_shareholders_ttm
//   支持负 PE (亏损); mcap<=0 是未上市/无价格哨兵; n==0 → NaN.
void ts_pe_raw(int a, const Axes &axes, const PitPool &pool,
               const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(F::mcap_raw, a);
  scan_latest_ttm(a, axes, pool, meta, T, F::pe_raw,
                  [&](int d, const FinancialTtmEv &e) -> float {
                    float m = mcap[d];
                    float n = e.net_profit_to_parent_shareholders_ttm;
                    return (is_finite(m) && m > 0.0f &&
                            is_finite(n) && n != 0.0f)
                               ? m / n
                               : std::nanf("");
                  });
}

// pcf_raw = mcap_raw / ttm.net_cffoa_ttm; 经营性现金流可负, 不剔;
//   mcap<=0 是未上市/无价格哨兵; c==0 → NaN.
void ts_pcf_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  auto mcap = T.ts_row(F::mcap_raw, a);
  scan_latest_ttm(a, axes, pool, meta, T, F::pcf_raw,
                  [&](int d, const FinancialTtmEv &e) -> float {
                    float m = mcap[d];
                    float c = e.net_cffoa_ttm;
                    return (is_finite(m) && m > 0.0f &&
                            is_finite(c) && c != 0.0f)
                               ? m / c
                               : std::nanf("");
                  });
}

// roe_raw / roa_raw: 同时读 ttm + balance 两路事件流; 单独 helper 处理 dual scan.
//   ROE = NP_parent_ttm / avg(equity_to_parent) × 100  (经典归母 ROE, 分子分母同归母)
//   ROA = NP_ttm(含少数) / avg(total_assets) × 100
//     ROA 分子取含少数: 总资产由全体股东 (含少数) 与债权人共同支撑, 配归母净利是
//     两边错配 (母公司只享部分子公司权益却摊全部资产). 果仁亦用含少数, 换过来后
//     roa_raw 对 test/1.csv 的命中 (5% 容差) 18.2% → 94%.
// 分母走 TTM 窗口平均而非期末值: 分子是 12 个月的流量, 分母必须是同窗口的平均
//   存量才同口径 (教科书 ROE = NI / average equity). 期末值配 TTM 分子会在权益
//   快速变动 (增发 / 回购 / 大额分红) 的股票上系统性失真.

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

template <class Compute>
inline void scan_latest_ttm_and_balance(int a, const Axes &axes,
                                        const PitPool &pool,
                                        const StockMeta &meta, Tensor &T, F dst,
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

void ts_roe_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  scan_latest_ttm_and_balance(
      a, axes, pool, meta, T, F::roe_raw,
      [](int /*d*/, const FinancialTtmEv &t,
         const std::map<std::int32_t, FinancialBalanceEv> &by_rd) -> float {
        float n = t.net_profit_to_parent_shareholders_ttm;
        float eq = ttm_window_avg(
            t.report_date, by_rd,
            &FinancialBalanceEv::total_equity_to_parent_shareholders);
        return (is_finite(n) && is_finite(eq) && eq > 0.0f)
                   ? (n / eq) * 100.0f
                   : std::nanf("");
      });
}

void ts_roa_raw(int a, const Axes &axes, const PitPool &pool,
                const StockMeta &meta, Tensor &T) {
  scan_latest_ttm_and_balance(
      a, axes, pool, meta, T, F::roa_raw,
      [](int /*d*/, const FinancialTtmEv &t,
         const std::map<std::int32_t, FinancialBalanceEv> &by_rd) -> float {
        float n = t.net_profit_ttm;
        float as = ttm_window_avg(t.report_date, by_rd,
                                  &FinancialBalanceEv::total_assets);
        return (is_finite(n) && is_finite(as) && as > 0.0f)
                   ? (n / as) * 100.0f
                   : std::nanf("");
      });
}

// ============================================================================
// TS: raw meta 派生 (per-A 动态: D - list_date / D - delist_date)
// ============================================================================

// list_age / delist_age: D - {list,delist}_date if D ≥ anchor else NaN.
//   PIT: 不写"距事件天数" (未来信息). 下游用 is_finite 判 "已上市 / 已退市".
namespace {
inline void ts_age_from(int a, const Axes &axes, Tensor &T, F dst,
                        const std::string &anchor) {
  int n_d = axes.n_d();
  auto out = T.ts_row(dst, a);
  if (anchor.size() != 8) {
    std::fill(out.begin(), out.end(), std::nanf(""));
    return;
  }
  auto ad = misc::parse_yyyymmdd(anchor);
  for (int d = 0; d < n_d; ++d) {
    float age = static_cast<float>((axes.date_days[d] - ad).count());
    out[d] = (age >= 0.0f) ? age : std::nanf("");
  }
}
} // namespace

void ts_list_age(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
                 Tensor &T) {
  ts_age_from(a, axes, T, F::list_age, meta.list_date[a]);
}

void ts_delist_age(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
                   Tensor &T) {
  ts_age_from(a, axes, T, F::delist_age, meta.delist_date[a]);
}

// ============================================================================
// TS: derived — 由 raw 推 (TS 内依赖)
// ============================================================================

// daily_return: 后复权 close 链式日收益 (含分红再投入的真持有收益).
//   tensor 顶层 close_raw 是不复权真价 (mcap / limit / low_p 等需要真值);
//   "复权"是 daily_return 自身需要的内部细节 — 这里直接从 PitPool 读
//   {close, adjust_factor} 在内部叠出 close_hfq[d] = close[d] × adjust_factor[d],
//   再链式: ret[d] = close_hfq[d] / close_hfq[d-1] - 1.
//   除权日 close 真跳, adjust_factor 反向跳, close × af 平滑 ⇒ ret 仅含日内真实涨跌
//   (无除权日 -5%~-10% 假负跳; 与"持有不减仓"账户的真实收益对齐).
//   前复权不 causal (起点会随未来除权事件追溯调整, 历史回测会泄漏未来), 不采用.
//   d==0 或前一日 close/af 非 finite/0 → NaN.
//   下游 benchmark = pool 内等权 mean.
void ts_daily_return(int a, const Axes &axes, const PitPool &pool,
                     const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  const auto &cl = pool.bar1d.close;
  const auto &af = pool.bar1d.adjust_factor;
  auto out = T.ts_row(F::daily_return, a);
  if (n_d > 0)
    out[0] = std::nanf("");
  for (int d = 1; d < n_d; ++d) {
    std::size_t i0 = base + static_cast<std::size_t>(d - 1);
    std::size_t i1 = base + static_cast<std::size_t>(d);
    float c0 = cl[i0], c1 = cl[i1];
    float a0 = af[i0], a1 = af[i1];
    if (is_finite(c0) && is_finite(c1) && is_finite(a0) && is_finite(a1) &&
        c0 != 0.0f && a0 != 0.0f) {
      float h0 = c0 * a0;
      float h1 = c1 * a1;
      out[d] = (h0 != 0.0f) ? (h1 / h0 - 1.0f) : std::nanf("");
    } else {
      out[d] = std::nanf("");
    }
  }
}

void ts_low_p(int a, const Axes &axes, const PitPool &, const StockMeta &,
              Tensor &T) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto out = T.ts_row(F::low_p, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(cl[d]) && cl[d] > 0.0f && cl[d] < 1.0f) ? 1.0f : 0.0f;
  }
}

// low_mc: 主板阈值 5e8, 非主板 3e8. mb 判定 = meta.list_sector[a]==1 (asset 静态).
//   list_sector 编码: 1=主板, 2=创业板, 3=科创板, 4=北交所, 0=未知.
void ts_low_mc(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto mc = T.ts_row(F::mcap_raw, a);
  auto out = T.ts_row(F::low_mc, a);
  float thr = (meta.list_sector[a] == 1) ? 5e8f : 3e8f;
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(mc[d]) && mc[d] > 0.0f && mc[d] < thr) ? 1.0f : 0.0f;
  }
}

// limit_up / limit_dn: close 与 D-1 适用涨跌停同向触碰判定.
//   lim==+inf (数据不合理: BigQuant upper_limit==0 等) 或 NaN → is_finite=false → 不视为封板.
namespace {
template <class Cmp>
inline void ts_limit_cmp(int a, const Axes &axes, Tensor &T, F lim_f, F dst, Cmp cmp) {
  int n_d = axes.n_d();
  auto cl = T.ts_row(F::close_raw, a);
  auto lm = T.ts_row(lim_f, a);
  auto out = T.ts_row(dst, a);
  for (int d = 0; d < n_d; ++d) {
    out[d] = (is_finite(cl[d]) && cl[d] > 0.0f &&
              is_finite(lm[d]) && lm[d] > 0.0f && cmp(cl[d], lm[d]))
                 ? 1.0f
                 : 0.0f;
  }
}
} // namespace

void ts_limit_up(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  ts_limit_cmp(a, axes, T, F::up_lim, F::limit_up,
               [](float c, float l) { return c >= l - 1e-4f; });
}

void ts_limit_dn(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  ts_limit_cmp(a, axes, T, F::dn_lim, F::limit_dn,
               [](float c, float l) { return c <= l + 1e-4f; });
}

// ============================================================================
// TS: state machines
// ============================================================================

void ts_profit_st(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  std::vector<ForecastEv> trig;
  trig.reserve(pool.forecast[a].size());
  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != ForecastType::FirstLoss &&
        e.type != ForecastType::ContinueLoss)
      continue;
    if (!is_finite(e.last_parent_net) || e.last_parent_net >= 0.0f)
      continue;
    trig.push_back(e);
  }
  state_machine_intervals(
      trig, axes.n_d(),
      [&](const ForecastEv &fe) {
        return find_forecast_off_d(fe, pool.financial_income_annual[a], axes);
      },
      T.ts_row(F::profit_st, a));
}

void ts_revenue_st(int a, const Axes &axes, const PitPool &pool,
                   const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::revenue_st, a);
  auto rev_raw = T.ts_row(F::rev_raw, a);
  std::fill(out.begin(), out.end(), 0.0f);

  bool mb_a = (meta.list_sector[a] == 1); // 仅主板适用 (asset 静态, 1=主板)
  if (!mb_a)
    return;

  for (const auto &e : pool.forecast[a]) {
    if (month_of(e.end_date) != 12)
      continue;
    if (e.type != ForecastType::FirstLoss &&
        e.type != ForecastType::ContinueLoss)
      continue;
    int end_y = year_of(e.end_date);
    if (end_y < 2021)
      continue;
    if (e.v < 1 || e.v >= n_d)
      continue;
    // 业务判 forecast.ann_date >= 20210101: e.v 是 row D, e.v-1 是 visible_d
    if (axes.dates[e.v - 1] < "20210101")
      continue;

    int on_d = e.v;
    int off_d = find_forecast_off_d(e, pool.financial_income_annual[a], axes);
    if (on_d < 0)
      on_d = 0;
    if (off_d > n_d)
      off_d = n_d;
    float thr = (end_y >= 2024) ? 3e8f : 1e8f;
    for (int d = on_d; d < off_d; ++d) {
      if (is_finite(rev_raw[d]) && rev_raw[d] < thr) {
        out[d] = 1.0f;
      }
    }
  }
}

// dividend_st: 阶梯 forward fill — 每 dividend event 重算 3y_sum,
//   3y_sum = Σ over 历史 events with report_date.Y in [ann_y-3, ann_y-1]
//            的 cash_after_tax × share_raw[event.v]; 区间 [e.v, next.v) 填.
//   注: e.v 已是首次可见 row D; ann_y 用 axes.dates[e.v - 1] 还原 visible 日期年份.
//
// 暖机期 (warmup_d 之前) 一律不命中, 避免 3y 回望不完整时偏严:
//   1) 数据轴边界: axes 起点前事件不可见 → 起点后 3 年内 3y 窗口必然不完整
//   2) 股票自身: 上市后 3 年内分红记录天然少, 不视为 ST
//   warmup_d = max(axes_warmup_d, stock_warmup_d), 取较晚者.
void ts_dividend_st(int a, const Axes &axes, const PitPool &pool,
                    const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::dividend_st, a);
  auto share_raw = T.ts_row(F::share_raw, a);
  auto ni_raw = T.ts_row(F::ni_raw, a);
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

// risk_warn: 直读 pool.status.st_status (CUTOFF=0, hybrid 伪装假装盘前, last_d 由 static_data 填充).
//   pit.cpp itf_cn_stock_status::replay + apply_meta_overlays 已派生 4 态:
//     0=正常 / 1=ST / 2=*ST / 3=退市整理期 (int8 → float 直接 cast).
//   数据起点前一律 0 (prealloc 为 0, 文件不存在时不写, 保持初值).
//   注: 退市整理期靠 4 态识别 — 交易所摘 *ST 标签后狭义 st_status 翻 0, 仅靠
//       原始 st_status 漏判会被 strategy 选中持有至退市 (实测 *ST大通 2023/06/19
//       进退市整理期 → 漏排 → 持有 11 个交易日至退市). 派生规则见 pit.cpp.
//   下游 cs_tradable 把 risk_warn > 0.5 视为排除 (1.0/2.0/3.0 均触发 filter).
void ts_risk_warn(int a, const Axes &axes, const PitPool &pool,
                  const StockMeta &, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::risk_warn, a);
  std::size_t base =
      static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
  for (int d = 0; d < n_d; ++d) {
    int8_t st = pool.status.st_status[base + static_cast<std::size_t>(d)];
    out[d] = static_cast<float>(st);
  }
}

// trading_st: rolling 20D (low_p ∨ low_mc).all(). 单调连续计数即可.
void ts_trading_st(int a, const Axes &axes, const PitPool &, const StockMeta &,
                   Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(F::trading_st, a);
  auto low_p = T.ts_row(F::low_p, a);
  auto low_mc = T.ts_row(F::low_mc, a);
  constexpr int W = 20;
  std::fill(out.begin(), out.end(), 0.0f);
  int run = 0;
  for (int d = 0; d < n_d; ++d) {
    bool ok = (low_p[d] > 0.5f) || (low_mc[d] > 0.5f);
    run = ok ? run + 1 : 0;
    out[d] = (run >= W) ? 1.0f : 0.0f;
  }
}

// ============================================================================
// TS: 杂项 filter / pool
// ============================================================================

void ts_new_list(int a, const Axes &axes, const PitPool &, const StockMeta &,
                 Tensor &T) {
  int n_d = axes.n_d();
  auto la = T.ts_row(F::list_age, a);
  auto out = T.ts_row(F::new_list, a);
  for (int d = 0; d < n_d; ++d) {
    // list_age PIT 契约: finite ⇒ ≥ 0; NaN = 未上市.
    out[d] = (is_finite(la[d]) && la[d] < 60.0f) ? 1.0f : 0.0f;
  }
}

// pool_b = exchange ∈ wl ∧ list_sector ∈ wl ∧ industry_l1 ∈ wl
//          ∧ 已上市 ∧ ¬susp ∧ ¬退市 ∧ (true if include_margin else ¬is_margin)
//   exchange / list_sector 是 asset 静态 (全 D 同值, 启动期判一次);
//   industry_l1 是时变 (per-D 读 T.ts_row(F::industry_l1, a) → ID → mask 查白名单).
//   industry_l1 ID 0 (未知) 不在 mask 任何位 → ¬ind_ok, 自然排除.
void ts_pool_b(int a, const Axes &axes, const PitPool &, const StockMeta &meta,
               Tensor &T) {
  int n_d = axes.n_d();
  auto susp_ = T.ts_row(F::susp, a);
  auto is_marg_ = T.ts_row(F::is_margin, a);
  auto list_age_ = T.ts_row(F::list_age, a);
  auto delist_age_ = T.ts_row(F::delist_age, a);
  auto industry_l1_ = T.ts_row(F::industry_l1, a);
  auto out = T.ts_row(F::pool_b, a);

  bool ex_ok = in_whitelist(meta.exchange[a], ::config::POOL_EXCHANGE_WHITELIST);
  bool sec_ok = in_int_whitelist(meta.list_sector[a],
                                 ::config::POOL_LIST_SECTOR_WHITELIST);
  bool asset_ok = ex_ok && sec_ok;
  const auto &mask = industry_l1_whitelist_mask();
  constexpr bool incl_margin = ::config::POOL_INCLUDE_MARGIN;

  for (int d = 0; d < n_d; ++d) {
    bool ind_ok = false;
    if (asset_ok) {
      int id = static_cast<int>(industry_l1_[d]);
      if (id > 0 && id < static_cast<int>(SW2021_L1_COUNT))
        ind_ok = mask[static_cast<std::size_t>(id)];
    }
    bool b = asset_ok && ind_ok && is_finite(list_age_[d]) &&
             !(susp_[d] > 0.5f) &&
             !is_finite(delist_age_[d]);
    if (!incl_margin)
      b = b && !(is_marg_[d] > 0.5f);
    out[d] = b ? 1.0f : 0.0f;
  }
}

// ============================================================================
// CS: factor pipelines (gather → [1/x] → winsor_mad → z → pct_rank → scatter)
//   每个 feature 一行 factor_pipeline 调用; src/dst/invert 按字段表.
// ============================================================================

void cs_close(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::close_raw, F::close, /*invert=*/true, T, b);
}
void cs_mcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::mcap_raw, F::mcap, true, T, b);
}
void cs_fmcap(int d, const Axes &, Tensor &T, CsBufs &b) {
  factor_pipeline(d, F::fmcap_raw, F::fmcap, true, T, b);
}
void cs_pe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::pe_raw, F::pe_ttm12, true, T, b);
}
void cs_pb_ttm1(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::pb_raw, F::pb_ttm1, true, T, b);
}
void cs_ps_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::ps_raw, F::ps_ttm12, true, T, b);
}
void cs_pcf_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::pcf_raw, F::pcf_ttm12, true, T, b);
}
void cs_roe_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::roe_raw, F::roe_ttm12, /*invert=*/false, T, b);
}
void cs_roa_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::roa_raw, F::roa_ttm12, false, T, b);
}
void cs_dy_ttm12(int d, const Axes &, Tensor &T, CsBufs &b) {
  neutral_pipeline(d, F::dy_raw, F::dy_ttm12, false, T, b);
}

// pool: pool_b ∧ rank(mcap_raw asc within pool_b) ≤ POOL_UNIVERSE_SIZE
void cs_pool(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(F::pool_b, d, b.a);
  T.gather_cs_row(F::mcap_raw, d, b.b);

  std::vector<std::pair<float, int>> cands; // (mcap, a)
  cands.reserve(b.a.size());
  for (std::size_t a = 0; a < b.a.size(); ++a) {
    if (!(b.a[a] > 0.5f))
      continue;
    float mc = b.b[a];
    if (!is_finite(mc) || mc <= 0.0f)
      continue;
    cands.emplace_back(mc, static_cast<int>(a));
  }

  int n = static_cast<int>(cands.size());
  int k = std::min(::config::POOL_UNIVERSE_SIZE, n);
  // k == n 时 cands.begin()+k == cands.end(), std::nth_element 第二参传 end 是 UB;
  // 全部入选时无需分割, 跳过 nth_element.
  if (k > 0 && k < n) {
    std::nth_element(cands.begin(), cands.begin() + k, cands.end(),
                     [](const auto &x, const auto &y) { return x.first < y.first; });
  }

  std::fill(b.c.begin(), b.c.end(), 0.0f);
  for (int i = 0; i < k; ++i)
    b.c[static_cast<std::size_t>(cands[i].second)] = 1.0f;
  T.scatter_cs_row(F::pool, d, std::span<const float>(b.c.data(), b.c.size()));
}

// tradable: pool ∧ ¬OR(config::STRATEGY_ENABLED_FILTERS).
//   pool 是 cs (pct_rank / nth-smallest 母集), tradable 是策略实际 top-K 母集.
//   filter 子集集中在 config.hpp; 删一行即禁用该 filter.
void cs_tradable(int d, const Axes &, Tensor &T, CsBufs &b) {
  T.gather_cs_row(F::pool, d, b.a);
  std::size_t na = b.a.size();

  for (F src : ::config::STRATEGY_ENABLED_FILTERS) {
    T.gather_cs_row(src, d, b.b);
    for (std::size_t a = 0; a < na; ++a) {
      if (b.b[a] > 0.5f)
        b.a[a] = 0.0f;
    }
  }

  T.scatter_cs_row(F::tradable, d, std::span<const float>(b.a.data(), na));
}

// factor_score: 配置因子 finite-加权平均.
//   factor_pipeline 已保证每个 factor 全 finite; pool/tradable 由下游另行 mask.
void cs_factor_score(int d, const Axes &, Tensor &T, CsBufs &b) {
  std::size_t na = b.a.size();
  std::fill(b.a.begin(), b.a.end(), 0.0f);
  std::fill(b.b.begin(), b.b.end(), 0.0f);

  for (const auto &fw : ::config::STRATEGY_FACTOR_WEIGHTS) {
    T.gather_cs_row(fw.f, d, b.c);
    for (std::size_t a = 0; a < na; ++a) {
      float v = b.c[a];
      if (is_finite(v)) {
        b.a[a] += fw.w * v;
        b.b[a] += fw.w;
      }
    }
  }

  for (std::size_t a = 0; a < na; ++a) {
    assert(b.b[a] > 0.0f && "factor_score: no finite configured factor");
    b.a[a] = b.a[a] / b.b[a];
  }
  T.scatter_cs_row(F::factor_score, d, std::span<const float>(b.a.data(), na));
}

} // namespace impl

// ============================================================================
// FEATURES[] — 单点真理 (索引 = F 枚举值 = 计算顺序)
// ============================================================================

const std::array<FeatureMeta, static_cast<std::size_t>(F::COUNT)> FEATURES = {{
    // ============================== TS ==============================
    // raw 网格 — PitPool dense 直读 (per-A 动态)
    {"close_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_close_raw, nullptr},
    {"mcap_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_mcap_raw, nullptr},
    {"fmcap_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_fmcap_raw, nullptr},
    {"share_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_share_raw, nullptr},
    {"pb_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pb_raw, nullptr},
    {"ps_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ps_raw, nullptr},
    {"dy_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_dy_raw, nullptr},
    {"up_lim", Kind::Inter, Axis::TimeSeries, &impl::ts_up_lim, nullptr},
    {"dn_lim", Kind::Inter, Axis::TimeSeries, &impl::ts_dn_lim, nullptr},
    {"susp", Kind::Inter, Axis::TimeSeries, &impl::ts_susp, nullptr},
    {"is_margin", Kind::Inter, Axis::TimeSeries, &impl::ts_is_margin, nullptr},
    {"mr_bal_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_mr_bal_raw, nullptr},
    {"ms_bal_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ms_bal_raw, nullptr},
    // industry_l1 — sw2021 一级行业 ID per (D, A) (component 月初 + change 月内回放)
    {"industry_l1", Kind::Inter, Axis::TimeSeries, &impl::ts_industry_l1, nullptr},
    // raw 自算 — ttm12 拼接 (依赖 mcap_raw)
    {"rev_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_rev_raw, nullptr},
    {"ni_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_ni_raw, nullptr},
    {"pe_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pe_raw, nullptr},
    {"pcf_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_pcf_raw, nullptr},
    {"roe_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_roe_raw, nullptr},
    {"roa_raw", Kind::Inter, Axis::TimeSeries, &impl::ts_roa_raw, nullptr},
    // raw meta 派生 — per-A 动态 (D - list_date / D - delist_date)
    {"list_age", Kind::Inter, Axis::TimeSeries, &impl::ts_list_age, nullptr},
    {"delist_age", Kind::Inter, Axis::TimeSeries, &impl::ts_delist_age, nullptr},
    // derived — 由 raw 推
    {"daily_return", Kind::Inter, Axis::TimeSeries, &impl::ts_daily_return, nullptr},
    {"low_p", Kind::Inter, Axis::TimeSeries, &impl::ts_low_p, nullptr},
    {"low_mc", Kind::Inter, Axis::TimeSeries, &impl::ts_low_mc, nullptr},
    {"limit_up", Kind::Inter, Axis::TimeSeries, &impl::ts_limit_up, nullptr},
    {"limit_dn", Kind::Inter, Axis::TimeSeries, &impl::ts_limit_dn, nullptr},
    // filter — 1 = 排除 (D, A)
    {"profit_st", Kind::Filter, Axis::TimeSeries, &impl::ts_profit_st, nullptr},
    {"revenue_st", Kind::Filter, Axis::TimeSeries, &impl::ts_revenue_st, nullptr},
    {"dividend_st", Kind::Filter, Axis::TimeSeries, &impl::ts_dividend_st, nullptr},
    {"risk_warn", Kind::Filter, Axis::TimeSeries, &impl::ts_risk_warn, nullptr},
    {"trading_st", Kind::Filter, Axis::TimeSeries, &impl::ts_trading_st, nullptr},
    {"new_list", Kind::Filter, Axis::TimeSeries, &impl::ts_new_list, nullptr},
    // pool (TS) — universe 母集
    {"pool_b", Kind::Inter, Axis::TimeSeries, &impl::ts_pool_b, nullptr},
    // ============================== CS ==============================
    // factor — winsor_mad → z → pct_rank
    {"close", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_close},
    {"mcap", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_mcap},
    {"fmcap", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_fmcap},
    {"pe_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pe_ttm12},
    {"pb_ttm1", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pb_ttm1},
    {"ps_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_ps_ttm12},
    {"pcf_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_pcf_ttm12},
    {"roe_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roe_ttm12},
    {"roa_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_roa_ttm12},
    {"dy_ttm12", Kind::Factor, Axis::CrossSection, nullptr, &impl::cs_dy_ttm12},
    // pool (CS) — universe 母集
    {"pool", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_pool},
    {"tradable", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_tradable},
    {"factor_score", Kind::Inter, Axis::CrossSection, nullptr, &impl::cs_factor_score},
}};

} // namespace feature
