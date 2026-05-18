#include "feature/pit.hpp"

#include "feature/axis.hpp"
#include "feature/industry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// pit.cpp 是「itf api 单点」: 每个 itf 一组 (prealloc + parse + post_sort +
//   post_ffill) 集中定义 ⇒ 末尾 ITFS[] 表挂载. load.cpp 仅迭代该表, 不出现
//   具体 itf 名.
//   增减 itf:  1) pit.hpp 加 typed Grid/Ev struct 与 PitPool 字段
//             2) pit.cpp 加一个 namespace itf_<name> 块 (prealloc/parse/[post_sort/post_ffill])
//             3) ITFS[] 末尾追加一行
//
// 【raw cutoff 单点真理】每个 itf namespace 内 constexpr CUTOFF = 0 / -1:
//   row D 的合法数据 = visible_date <= D + CUTOFF 的最新值
//   ⇒ parse 时把 v_idx (= floor_date(visible_date)) 调整为 row = v_idx - CUTOFF:
//     CUTOFF=0  (盘前 / 状态生效日): row = v_idx       (当日生效)
//     CUTOFF=-1 (盘后 / 公告实时): row = v_idx + 1     (下一交易日生效)
//   网格 itf 写 pool[a*n_d + row]; 事件 itf ev.v = row.
//   pool 即「row D 已 cutoff 的合法数据」, 下游 feature 直读 pool[base + d],
//   不再关心 cutoff. 业务需要原始 visible 日期 (dividend 判 ann_y) 时,
//   axes.dates[ev.v - 1] 还原 floor_date(visible_date) (仅 CUTOFF=-1 适用).
//
// 数据源 (lookup 字段差异):
//   BigQuant 表统一用 "instrument";  Tushare 表 (forecast/...) 仍用 "ts_code".
//   每个 itf 内部 inline 选择字段, 不抽公共 helper (避免运行时分支).
// ============================================================================

namespace feature {

namespace {

// ---- yyjson helpers ----
// NaN = 数据缺失 (JSON 字段不存在或 null)
// +inf = 数据不合理 (存在但值违反业务约束)
inline float as_float_or_nan(yyjson_val *v) {
  if (!v)
    return std::nanf("");
  if (yyjson_is_real(v))
    return static_cast<float>(yyjson_get_real(v));
  if (yyjson_is_int(v))
    return static_cast<float>(yyjson_get_int(v));
  return std::nanf("");
}

// NaN (数据缺失: JSON null / 字段不存在) → NaN 透传, 留给 grid_ffill 用前值兜;
// finite ∧ 满足约束 → 原值; finite ∧ 违反约束 (e.g. close ≤ 0) → +inf 保留"业务异常"标记.
// 把 NaN 也无差别转 +inf 会绕过 ffill (ffill 不兜 +inf), 导致停牌日 close=+inf 残留
// 全程污染下游 daily_return / mcap_raw / limit_up/dn 等.
inline float positive_or_inf(float v) {
  if (std::isnan(v))
    return v;
  return (std::isfinite(v) && v > 0.0f) ? v : std::numeric_limits<float>::infinity();
}

inline float non_negative_or_inf(float v) {
  if (std::isnan(v))
    return v;
  return (std::isfinite(v) && v >= 0.0f) ? v : std::numeric_limits<float>::infinity();
}

inline std::string as_str(yyjson_val *v) {
  if (!v || !yyjson_is_str(v))
    return {};
  const char *s = yyjson_get_str(v);
  return s ? std::string(s) : std::string();
}

inline const char *as_cstr_or_null(yyjson_val *v) {
  if (!v || !yyjson_is_str(v))
    return nullptr;
  return yyjson_get_str(v);
}

inline int as_int_or_default(yyjson_val *v, int def) {
  if (!v) return def;
  if (yyjson_is_int(v)) return static_cast<int>(yyjson_get_int(v));
  return def;
}

// 按字段名查 a 索引. 字段缺失 / 非 string / 不在 code_idx → -1.
inline int lookup_a(const Axes &axes, yyjson_val *item, const char *field) {
  yyjson_val *v = yyjson_obj_get(item, field);
  const char *s = as_cstr_or_null(v);
  if (!s)
    return -1;
  auto it = axes.code_idx.find(s);
  return it == axes.code_idx.end() ? -1 : it->second;
}

// 网格字段 prealloc 共用模板: length = n_a*n_d, 填 NaN
inline void grid_prealloc_float(std::vector<float> &v, std::size_t n) {
  v.assign(n, std::nanf(""));
}

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
        grid[base + static_cast<std::size_t>(d)] = last;
      }
    }
  }
}

// 网格 int8/uint8 字段 per-A forward fill:
//   - 非 sentinel 值记 last
//   - sentinel 值若 last 非 sentinel → 填充
//   注意 GridStatus.suspended 是 0/1 (无 sentinel, 不需 ffill); st_status 0/1/2
//   每日盘前全量快照, 0 表示当日不在 ST 名单 — 按"盘前快照保底"语义不做 ffill.
//   预留 helper, 当前未使用.

} // namespace

// ============================================================================
// 网格 itf (新基建)
// ============================================================================

namespace itf_cn_stock_bar1d {

// cn_stock_bar1d: 后复权 OHLCV + adjust_factor; 当前张量层只用 close.
//   入库时机: 盘后 17:00–19:30; CUTOFF=-1 → row D 拿 D-1 实际收盘.
constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  grid_prealloc_float(p.bar1d.close, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0)
    return;
  int n_d = axes.n_d();
  int row = v_idx - CUTOFF;
  if (row >= n_d)
    return;
  std::size_t base_off = static_cast<std::size_t>(row);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    pool.bar1d.close[off] = positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "close")));
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  grid_ffill(p.bar1d.close, axes.n_a(), axes.n_d());
}

} // namespace itf_cn_stock_bar1d

namespace itf_cn_stock_shares {

// cn_stock_shares: 总股本 / 流通股 [股]; 入库盘后, CUTOFF=-1.
constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  grid_prealloc_float(p.shares.total_shares, n);
  grid_prealloc_float(p.shares.total_float_shares, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0)
    return;
  int n_d = axes.n_d();
  int row = v_idx - CUTOFF;
  if (row >= n_d)
    return;
  std::size_t base_off = static_cast<std::size_t>(row);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    pool.shares.total_shares[off] =
        positive_or_inf(as_float_or_nan(yyjson_obj_get(item, "total_shares")));
    pool.shares.total_float_shares[off] = positive_or_inf(
        as_float_or_nan(yyjson_obj_get(item, "total_float_shares")));
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.shares.total_shares, n_a, n_d);
  grid_ffill(p.shares.total_float_shares, n_a, n_d);
}

} // namespace itf_cn_stock_shares

namespace itf_cn_stock_limit_price {

// cn_stock_limit_price: 当日适用涨跌停价 [元/股]. 盘前 08:40 入库, CUTOFF=0.
constexpr int CUTOFF = 0;

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  grid_prealloc_float(p.limit_price.upper_limit, n);
  grid_prealloc_float(p.limit_price.lower_limit, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0)
    return;
  int n_d = axes.n_d();
  int row = v_idx - CUTOFF;
  if (row >= n_d)
    return;
  std::size_t base_off = static_cast<std::size_t>(row);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    // 哨兵: up==0 (异常) → 1e6 (无涨停), dn==0 → 0.01 (无跌停).
    {
      float up = as_float_or_nan(yyjson_obj_get(item, "upper_limit"));
      pool.limit_price.upper_limit[off] = (up == 0.0f) ? 1e6f : positive_or_inf(up);
    }
    {
      float dn = as_float_or_nan(yyjson_obj_get(item, "lower_limit"));
      pool.limit_price.lower_limit[off] = (dn == 0.0f) ? 0.01f : positive_or_inf(dn);
    }
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  grid_ffill(p.limit_price.upper_limit, n_a, n_d);
  grid_ffill(p.limit_price.lower_limit, n_a, n_d);
}

} // namespace itf_cn_stock_limit_price

namespace itf_cn_stock_status {

// cn_stock_status: 盘前 09:20 全量快照. CUTOFF=0.
//   st_status int8: 0=正常, 1=ST, 2=*ST  → 直接读, 无需 ffill (盘前每日全量).
//   suspended uint8: 0=正常, 1=停牌      → 直接读.
//   (is_risk_warning / price_limit_status / exdr 暂未入张量)
constexpr int CUTOFF = 0;

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  p.status.st_status.assign(n, int8_t{0});
  p.status.suspended.assign(n, uint8_t{0});
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0)
    return;
  int n_d = axes.n_d();
  int row = v_idx - CUTOFF;
  if (row >= n_d)
    return;
  std::size_t base_off = static_cast<std::size_t>(row);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    int st = as_int_or_default(yyjson_obj_get(item, "st_status"), 0);
    int sp = as_int_or_default(yyjson_obj_get(item, "suspended"), 0);
    pool.status.st_status[off] = static_cast<int8_t>(st);
    pool.status.suspended[off] = (sp != 0) ? uint8_t{1} : uint8_t{0};
  }
}

// 不做 ffill — cn_stock_status 是盘前全量快照, 缺日 (无文件) = 数据起点前/拉取漏日,
//   一律保持 0 (按"未知=正常"处理). risk_warn / susp 直接读 row D.

} // namespace itf_cn_stock_status

namespace itf_cn_stock_margin_trading_detail {

// cn_stock_margin_trading_detail: 当日两融明细 (盘前入库, CUTOFF=0).
//   字段: financing_balance, securities_lending_balance ≥ 0.
//   派生: is_margin = 1 当 (D, A) 存在记录 (= 当日两融标的).
constexpr int CUTOFF = 0;

void prealloc(const Axes &axes, PitPool &p) {
  std::size_t n = static_cast<std::size_t>(axes.n_a()) *
                  static_cast<std::size_t>(axes.n_d());
  p.margin_detail.is_margin.assign(n, uint8_t{0});
  grid_prealloc_float(p.margin_detail.financing_balance, n);
  grid_prealloc_float(p.margin_detail.securities_lending_balance, n);
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> * /*mu*/) {
  assert(arr && yyjson_is_arr(arr));
  if (v_idx < 0)
    return;
  int n_d = axes.n_d();
  int row = v_idx - CUTOFF;
  if (row >= n_d)
    return;
  std::size_t base_off = static_cast<std::size_t>(row);

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    std::size_t off = static_cast<std::size_t>(a) *
                          static_cast<std::size_t>(n_d) +
                      base_off;
    pool.margin_detail.is_margin[off] = 1;
    pool.margin_detail.financing_balance[off] = non_negative_or_inf(
        as_float_or_nan(yyjson_obj_get(item, "financing_balance")));
    pool.margin_detail.securities_lending_balance[off] = non_negative_or_inf(
        as_float_or_nan(yyjson_obj_get(item, "securities_lending_balance")));
  }
}

void post_ffill(const Axes &axes, PitPool &p) {
  int n_a = axes.n_a(), n_d = axes.n_d();
  // is_margin 不做 ffill — 缺席日 (周末/节假日已被 floor; 真正的 0 表示当日不在两融名单)
  grid_ffill(p.margin_detail.financing_balance, n_a, n_d);
  grid_ffill(p.margin_detail.securities_lending_balance, n_a, n_d);
}

} // namespace itf_cn_stock_margin_trading_detail

// ============================================================================
// 事件 itf (新基建; per-A mutex emplace; post_sort 末段 sort by v)
// ============================================================================

namespace itf_cn_stock_industry_component {

// cn_stock_industry_component: 月初 sw2021 一级行业归属快照 (MonthFirst, CUTOFF=0).
//   每月仅 1 个 DD 文件 (visible_date = MIN(date) of month); 同 (D, instrument)
//   下 industry∈{cs,sw2014,sw2021} 三套, 仅取 sw2021 入 ev.
constexpr int CUTOFF = 0;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.industry_component, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    const char *ind = as_cstr_or_null(yyjson_obj_get(item, "industry"));
    if (!ind || std::strcmp(ind, "sw2021") != 0)
      continue;
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    const char *l1n = as_cstr_or_null(yyjson_obj_get(item, "industry_level1_name"));
    uint8_t l1_id = sw2021_l1_name_to_id(l1n ? std::string_view(l1n) : std::string_view());
    IndustryComponentEv ev{row, l1_id};
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.industry_component[a].push_back(ev);
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.industry_component); }

} // namespace itf_cn_stock_industry_component

namespace itf_cn_stock_industry_change {

// cn_stock_industry_change: 月内 sw2021 L1 行业切换事件 (Day, CUTOFF=-1, 盘后).
//   过滤: industry=='sw2021' AND industry_level==1 AND change_flag==1 (进入新行业).
//   change_flag==0 (退出旧行业) 不入 — 行业切换日两条同 D 同 A, 只取"进"侧.
constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.industry_change, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    const char *ind = as_cstr_or_null(yyjson_obj_get(item, "industry"));
    if (!ind || std::strcmp(ind, "sw2021") != 0)
      continue;
    int level = as_int_or_default(yyjson_obj_get(item, "industry_level"), 0);
    if (level != 1)
      continue;
    int flag = as_int_or_default(yyjson_obj_get(item, "change_flag"), -1);
    if (flag != 1)
      continue;
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    const char *nm = as_cstr_or_null(yyjson_obj_get(item, "industry_name"));
    uint8_t l1_id = sw2021_l1_name_to_id(nm ? std::string_view(nm) : std::string_view());
    IndustryChangeEv ev{row, l1_id};
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.industry_change[a].push_back(ev);
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.industry_change); }

} // namespace itf_cn_stock_industry_change

namespace itf_cn_stock_dividend {

// cn_stock_dividend: 分红事件 (Where on publish_date, CUTOFF=-1).
//   字段: instrument / report_date / cash_after_tax (税后每股分红 [元/股]).
constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.dividend, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "instrument");
    if (a < 0)
      continue;
    DividendEv ev;
    ev.v = row;
    ev.report_date = as_str(yyjson_obj_get(item, "report_date"));
    ev.cash_after_tax = as_float_or_nan(yyjson_obj_get(item, "cash_after_tax"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.dividend[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.dividend); }

} // namespace itf_cn_stock_dividend

// ============================================================================
// 事件 itf (Tushare 保留)
// ============================================================================

namespace itf_forecast {

// Tushare forecast: 业绩预告. 公告实时 (盘后), CUTOFF=-1. 字段用 ts_code.
constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.forecast, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "ts_code");
    if (a < 0)
      continue;
    ForecastEv ev;
    ev.v = row;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    ev.type = as_str(yyjson_obj_get(item, "type"));
    ev.last_parent_net = as_float_or_nan(yyjson_obj_get(item, "last_parent_net"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.forecast[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.forecast); }

} // namespace itf_forecast

// ============================================================================
// 事件 itf (财务 Tushare 占位 — 用户决策: 财务先不管, 暂保留旧实现)
//   实际新基建未落地这几张表, parse 不会被触发, EventStore 永远空.
//   留作占位, 待后续 BigQuant cn_stock_financial_* 迁移.
// ============================================================================

namespace itf_report {

constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.report, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "ts_code");
    if (a < 0)
      continue;
    ReportEv ev;
    ev.v = row;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    {
      std::lock_guard<std::mutex> lk((*mu)[a]);
      pool.report[a].push_back(std::move(ev));
    }
  }
}

void post_sort(PitPool &p) { event_post_sort(p.report); }

} // namespace itf_report

namespace itf_income {

constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.income, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "ts_code");
    if (a < 0)
      continue;
    IncomeEv ev;
    ev.v = row;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    ev.report_type = as_str(yyjson_obj_get(item, "report_type"));
    ev.revenue = as_float_or_nan(yyjson_obj_get(item, "revenue"));
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

constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.cashflow, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "ts_code");
    if (a < 0)
      continue;
    CashflowEv ev;
    ev.v = row;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    ev.report_type = as_str(yyjson_obj_get(item, "report_type"));
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

constexpr int CUTOFF = -1;

void prealloc(const Axes &axes, PitPool &p) {
  event_prealloc(p.fina_indicator, static_cast<std::size_t>(axes.n_a()));
}

void parse(yyjson_val *arr, int v_idx, const Axes &axes, PitPool &pool,
           std::vector<std::mutex> *mu) {
  assert(arr && yyjson_is_arr(arr));
  assert(mu);
  if (v_idx < 0)
    return;
  int row = v_idx - CUTOFF;
  if (row >= axes.n_d())
    return;

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(arr, i, n, item) {
    int a = lookup_a(axes, item, "ts_code");
    if (a < 0)
      continue;
    FinaIndEv ev;
    ev.v = row;
    ev.end_date = as_str(yyjson_obj_get(item, "end_date"));
    ev.roe = as_float_or_nan(yyjson_obj_get(item, "roe"));
    ev.roa = as_float_or_nan(yyjson_obj_get(item, "roa"));
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
//   file_name = data/YYYY/MM/DD/<file_name>.json basename, 也是日志/标识用名.
//   增减 itf 在此追加/删除一行 + 上方 itf_<name> 块.
// ============================================================================

const ItfDesc ITFS[] = {
    // ---- 网格 itf (新基建; 无锁) ----
    {"cn_stock_bar1d", false, &itf_cn_stock_bar1d::prealloc,
     &itf_cn_stock_bar1d::parse, nullptr, &itf_cn_stock_bar1d::post_ffill},
    {"cn_stock_shares", false, &itf_cn_stock_shares::prealloc,
     &itf_cn_stock_shares::parse, nullptr, &itf_cn_stock_shares::post_ffill},
    {"cn_stock_limit_price", false, &itf_cn_stock_limit_price::prealloc,
     &itf_cn_stock_limit_price::parse, nullptr,
     &itf_cn_stock_limit_price::post_ffill},
    {"cn_stock_status", false, &itf_cn_stock_status::prealloc,
     &itf_cn_stock_status::parse, nullptr, nullptr},
    {"cn_stock_margin_trading_detail", false,
     &itf_cn_stock_margin_trading_detail::prealloc,
     &itf_cn_stock_margin_trading_detail::parse, nullptr,
     &itf_cn_stock_margin_trading_detail::post_ffill},

    // ---- 事件 itf (新基建; per-A mutex) ----
    {"cn_stock_industry_component", true,
     &itf_cn_stock_industry_component::prealloc,
     &itf_cn_stock_industry_component::parse,
     &itf_cn_stock_industry_component::post_sort, nullptr},
    {"cn_stock_industry_change", true, &itf_cn_stock_industry_change::prealloc,
     &itf_cn_stock_industry_change::parse,
     &itf_cn_stock_industry_change::post_sort, nullptr},
    {"cn_stock_dividend", true, &itf_cn_stock_dividend::prealloc,
     &itf_cn_stock_dividend::parse, &itf_cn_stock_dividend::post_sort, nullptr},

    // ---- 事件 itf (Tushare 保留) ----
    {"forecast", true, &itf_forecast::prealloc, &itf_forecast::parse,
     &itf_forecast::post_sort, nullptr},

    // ---- 事件 itf (财务 Tushare 占位; 数据未落地, parse 不会触发) ----
    {"report", true, &itf_report::prealloc, &itf_report::parse,
     &itf_report::post_sort, nullptr},
    {"income", true, &itf_income::prealloc, &itf_income::parse,
     &itf_income::post_sort, nullptr},
    {"cashflow", true, &itf_cashflow::prealloc, &itf_cashflow::parse,
     &itf_cashflow::post_sort, nullptr},
    {"fina_indicator", true, &itf_fina_indicator::prealloc,
     &itf_fina_indicator::parse, &itf_fina_indicator::post_sort, nullptr},
};

const int ITFS_COUNT = static_cast<int>(sizeof(ITFS) / sizeof(ITFS[0]));

} // namespace feature
