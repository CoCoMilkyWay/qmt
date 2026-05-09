#include "feature/pit.hpp"

#include "feature/axis.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// pit.cpp 是「itf api 单点」: 每个 itf 一组 (prealloc + parse + post_sort)
//   集中定义 ⇒ 末尾 ITFS[] 表挂载. load.cpp 仅迭代该表, 不出现具体 itf 名.
//   增减 itf:  1) pit.hpp 加 typed Grid/Ev struct 与 PitPool 字段
//             2) pit.cpp 加一个 namespace itf_<name> 块 (prealloc/parse/[post_sort])
//             3) ITFS[] 末尾追加一行
// ============================================================================

namespace feature {

namespace {

// ---- yyjson helpers ----
// NaN = 数据缺失 (JSON 字段不存在或 null)
// +inf = 数据不合理 (存在但值违反业务约束)
inline float as_float_or_nan(yyjson_val *v) {
  if (!v) return std::nanf("");
  if (yyjson_is_real(v)) return static_cast<float>(yyjson_get_real(v));
  if (yyjson_is_int(v)) return static_cast<float>(yyjson_get_int(v));
  return std::nanf("");
}

// 校验: 值必须 > 0，否则返回 +inf
inline float positive_or_inf(float v) {
  return (std::isfinite(v) && v > 0.0f) ? v : std::numeric_limits<float>::infinity();
}

// 校验: 值必须 >= 0，否则返回 +inf
inline float non_negative_or_inf(float v) {
  return (std::isfinite(v) && v >= 0.0f) ? v : std::numeric_limits<float>::infinity();
}

inline std::string as_str(yyjson_val *v) {
  if (!v || !yyjson_is_str(v)) return {};
  const char *s = yyjson_get_str(v);
  return s ? std::string(s) : std::string();
}

inline const char *as_cstr_or_null(yyjson_val *v) {
  if (!v || !yyjson_is_str(v)) return nullptr;
  return yyjson_get_str(v);
}

inline int lookup_a(const Axes &axes, yyjson_val *ts_code_v) {
  const char *s = as_cstr_or_null(ts_code_v);
  if (!s) return -1;
  auto it = axes.code_idx.find(s);
  return it == axes.code_idx.end() ? -1 : it->second;
}

// 网格字段 prealloc 共用模板: length = n_a*n_d, 填 NaN
inline void grid_prealloc_float(std::vector<float> &v, std::size_t n) {
  v.assign(n, std::nanf(""));
}

// 事件 store prealloc: length = n_a, 空链
template <class Ev>
inline void event_prealloc(EventStore<Ev> &store, std::size_t n_a) {
  store.assign(n_a, {});
}

template <class Ev>
inline void event_post_sort(EventStore<Ev> &store) {
  for (auto &chain : store) {
    std::sort(chain.begin(), chain.end(),
              [](const Ev &a, const Ev &b) { return a.v < b.v; });
  }
}

// 网格字段 per-A forward fill:
//   - finite 值记为 last，可用作 fill 源
//   - NaN (数据缺失) 用 last 填充
//   - +inf (数据不合理) 不填充，保留标记
inline void grid_ffill(std::vector<float> &grid, int n_a, int n_d) {
  for (int a = 0; a < n_a; ++a) {
    std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
    float last = std::nanf("");
    for (int d = 0; d < n_d; ++d) {
      float v = grid[base + static_cast<std::size_t>(d)];
      if (std::isfinite(v)) {
        last = v;
      } else if (std::isnan(v) && std::isfinite(last)) {
        // 只填充 NaN，不填充 +inf
        grid[base + static_cast<std::size_t>(d)] = last;
      }
    }
  }
}

} // namespace

// ============================================================================
// 网格 itf
// ============================================================================

namespace itf_daily_basic {

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  grid_prealloc_float(p.daily_basic.close, n);
  grid_prealloc_float(p.daily_basic.total_mv, n);
  grid_prealloc_float(p.daily_basic.circ_mv, n);
  grid_prealloc_float(p.daily_basic.total_share, n);
  grid_prealloc_float(p.daily_basic.pe_ttm, n);
  grid_prealloc_float(p.daily_basic.pb, n);
  grid_prealloc_float(p.daily_basic.ps_ttm, n);
  grid_prealloc_float(p.daily_basic.dv_ttm, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0) return;
  int n_d = axes.n_d();
  std::size_t base_off = static_cast<std::size_t>(v_idx);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    // 校验: close/total_mv/circ_mv/total_share 必须 > 0
    pool.daily_basic.close[off]       = positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "close")));
    pool.daily_basic.total_mv[off]    = positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "total_mv")));
    pool.daily_basic.circ_mv[off]     = positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "circ_mv")));
    pool.daily_basic.total_share[off] = positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "total_share")));
    // pe_ttm 不再使用 (自己算), 但保留读取
    pool.daily_basic.pe_ttm[off]      = as_float_or_nan(yyjson_obj_get(item, "pe_ttm"));
    // pb 可正可负 (净资产正负), 保持原值
    pool.daily_basic.pb[off]          = as_float_or_nan(yyjson_obj_get(item, "pb"));
    // ps_ttm: NaN 保持（无营收数据正常），仅 <=0 不合理
    {
      float ps = as_float_or_nan(yyjson_obj_get(item, "ps_ttm"));
      pool.daily_basic.ps_ttm[off] = std::isnan(ps) ? ps : positive_or_inf(ps);
    }
    // dv_ttm: NaN 保持（无分红数据正常），仅负数不合理
    {
      float dv = as_float_or_nan(yyjson_obj_get(item, "dv_ttm"));
      pool.daily_basic.dv_ttm[off] = std::isnan(dv) ? dv : non_negative_or_inf(dv);
    }
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.daily_basic.close, n_a, n_d);
  grid_ffill(p.daily_basic.total_mv, n_a, n_d);
  grid_ffill(p.daily_basic.circ_mv, n_a, n_d);
  grid_ffill(p.daily_basic.total_share, n_a, n_d);
  grid_ffill(p.daily_basic.pe_ttm, n_a, n_d);
  grid_ffill(p.daily_basic.pb, n_a, n_d);
  grid_ffill(p.daily_basic.ps_ttm, n_a, n_d);
  grid_ffill(p.daily_basic.dv_ttm, n_a, n_d);
}

} // namespace itf_daily_basic

namespace itf_stk_limit {

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  grid_prealloc_float(p.stk_limit.up_limit, n);
  grid_prealloc_float(p.stk_limit.down_limit, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0) return;
  int n_d = axes.n_d();
  std::size_t base_off = static_cast<std::size_t>(v_idx);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    // 涨跌停价校验 (哨兵: up>=1e6 无涨停, dn<=0.01 无跌停):
    //   - 数据问题: up=0 (极少量) → 1e6
    //   - 数据问题: dn=0 (北交所开市首日 20211115, 248条) → 0.01
    {
      float up = as_float_or_nan(yyjson_obj_get(item, "up_limit"));
      pool.stk_limit.up_limit[off] = (up == 0.0f) ? 1e6f : positive_or_inf(up);
    }
    {
      float dn = as_float_or_nan(yyjson_obj_get(item, "down_limit"));
      pool.stk_limit.down_limit[off] = (dn == 0.0f) ? 0.01f : positive_or_inf(dn);
    }
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.stk_limit.up_limit, n_a, n_d);
  grid_ffill(p.stk_limit.down_limit, n_a, n_d);
}

} // namespace itf_stk_limit

namespace itf_suspend_d {

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  p.suspend_d.susp.assign(n, 0u); // 0 = 无停牌
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0) return;
  int n_d = axes.n_d();
  std::size_t base_off = static_cast<std::size_t>(v_idx);

  // suspend_d 文件存在记录 = 当日有停/复牌动作; suspend_type='S' 表停牌, 'R' 复牌.
  // susp 取 1 (停牌中) 仅当 suspend_type=='S'; 复牌 (R) 不视为停牌当日.
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    const char *st = as_cstr_or_null(yyjson_obj_get(item, "suspend_type"));
    if (!st) continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    if (st[0] == 'S') pool.suspend_d.susp[off] = 1;
  }
}

} // namespace itf_suspend_d

// ============================================================================
// 事件 itf  (per-A mutex emplace; post_sort 末段 sort by v)
// ============================================================================

namespace itf_forecast {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.forecast, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    ForecastEv ev;
    ev.v = v_idx;
    ev.end_date        = as_str(yyjson_obj_get(item, "end_date"));
    ev.type            = as_str(yyjson_obj_get(item, "type"));
    ev.last_parent_net = as_float_or_nan(yyjson_obj_get(item, "last_parent_net"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.forecast[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.forecast); }

} // namespace itf_forecast

namespace itf_report {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.report, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    ReportEv ev;
    ev.v        = v_idx;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.report[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.report); }

} // namespace itf_report

namespace itf_st {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.st, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    STEv ev;
    ev.v       = v_idx;
    ev.st_name = as_str(yyjson_obj_get(item, "name"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.st[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.st); }

} // namespace itf_st

namespace itf_dividend {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.dividend, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    DividendEv ev;
    ev.v            = v_idx;
    ev.end_date     = as_str(yyjson_obj_get(item, "end_date"));
    ev.div_proc     = as_str(yyjson_obj_get(item, "div_proc"));
    ev.cash_div_tax = as_float_or_nan(yyjson_obj_get(item, "cash_div_tax"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.dividend[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.dividend); }

} // namespace itf_dividend

namespace itf_income {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.income, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    IncomeEv ev;
    ev.v               = v_idx;
    ev.end_date        = as_str(yyjson_obj_get(item, "end_date"));
    ev.report_type     = as_str(yyjson_obj_get(item, "report_type"));
    ev.revenue         = as_float_or_nan(yyjson_obj_get(item, "revenue"));
    ev.n_income_attr_p = as_float_or_nan(yyjson_obj_get(item, "n_income_attr_p"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.income[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.income); }

} // namespace itf_income

namespace itf_cashflow {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.cashflow, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    CashflowEv ev;
    ev.v              = v_idx;
    ev.end_date       = as_str(yyjson_obj_get(item, "end_date"));
    ev.report_type    = as_str(yyjson_obj_get(item, "report_type"));
    ev.n_cashflow_act = as_float_or_nan(yyjson_obj_get(item, "n_cashflow_act"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.cashflow[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.cashflow); }

} // namespace itf_cashflow

namespace itf_fina_indicator {

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.fina_indicator, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0) return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, yyjson_obj_get(item, "ts_code"));
    if (a < 0) continue;
    FinaIndEv ev;
    ev.v        = v_idx;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    ev.roe      = as_float_or_nan(yyjson_obj_get(item, "roe"));
    ev.roa      = as_float_or_nan(yyjson_obj_get(item, "roa"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.fina_indicator[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.fina_indicator); }

} // namespace itf_fina_indicator

// ============================================================================
// ITFS[] 表 — 单点真理
//   增减 itf 在此追加/删除一行 + 上方 itf_<name> 块
// ============================================================================

const ItfDesc ITFS[] = {
    // 网格 itf (无锁)
    {"daily_basic", false, &itf_daily_basic::prealloc, &itf_daily_basic::parse, nullptr, &itf_daily_basic::post_ffill},
    {"stk_limit",   false, &itf_stk_limit::prealloc,   &itf_stk_limit::parse,   nullptr, &itf_stk_limit::post_ffill},
    {"suspend_d",   false, &itf_suspend_d::prealloc,   &itf_suspend_d::parse,   nullptr, nullptr},
    // 事件 itf (per-A mutex)
    {"forecast",       true, &itf_forecast::prealloc,       &itf_forecast::parse,       &itf_forecast::post_sort,       nullptr},
    {"report",         true, &itf_report::prealloc,         &itf_report::parse,         &itf_report::post_sort,         nullptr},
    {"st",             true, &itf_st::prealloc,             &itf_st::parse,             &itf_st::post_sort,             nullptr},
    {"dividend",       true, &itf_dividend::prealloc,       &itf_dividend::parse,       &itf_dividend::post_sort,       nullptr},
    {"income",         true, &itf_income::prealloc,         &itf_income::parse,         &itf_income::post_sort,         nullptr},
    {"cashflow",       true, &itf_cashflow::prealloc,       &itf_cashflow::parse,       &itf_cashflow::post_sort,       nullptr},
    {"fina_indicator", true, &itf_fina_indicator::prealloc, &itf_fina_indicator::parse, &itf_fina_indicator::post_sort, nullptr},
};

const int ITFS_COUNT = static_cast<int>(sizeof(ITFS) / sizeof(ITFS[0]));

} // namespace feature
