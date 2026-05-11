#pragma once

#include "package/yyjson/yyjson.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace feature {

struct Axes; // fwd decl, 避免在 pit.hpp 把 axis.hpp 拉进来

// ============================================================================
// PIT 中间结构. Phase 1 写入, Phase 2 只读.
//
// 划分:
//   网格 itf (1 record / 交易日 / asset): dense 存. 字段独立向量, length = n_a() * n_d() (a-major, d-minor).
//                                          缺席用 NaN; 与 Tensor::ts_row 同 layout, Phase 2 可零 copy 取 span.
//   事件 itf (per A 时间线):              EventStore<Ev> = vector<vector<Ev>>, [a] 外, 按 v 升序的事件链.
//                                          v = visible_d_idx (Axes.floor_date(visible_date) 后的 D 索引);
//                                          周末/节假日的 visible_date 已 floor 到上一交易日.
//
// PitPool 是 typed struct (而非泛型 map<name, anything>) — 编译期类型安全, parse fn 直接写
// pool.<itf>.<field>; 增减 itf 只需改 PitPool 与 itf 模块的 dense block (pit.cpp 内).
// ============================================================================

struct GridDailyBasic {
  std::vector<float> close, total_mv, circ_mv, total_share;
  std::vector<float> pe_ttm, pb, ps_ttm, dv_ttm;
};

struct GridStkLimit {
  std::vector<float> up_limit, down_limit;
};

struct GridSuspendD {
  std::vector<uint8_t> susp; // 1 = 当日有 suspend_d 记录 (停牌); 0 = 无
};

// margin_secs: 当日两融标的名单 (per-A bool 网格).
//   1 = 当日 (ts_code, trade_date) 在 margin_secs 列表; 0 = 不在.
struct GridMarginSecs {
  std::vector<uint8_t> is_margin;
};

// margin_detail: 每日两融余额 (per-A 元 网格).
//   mr_bal ← rzye (融资余额)
//   ms_bal ← rqye (融券余额)
//   注: rzrqye = mr_bal + ms_bal (不入张量, 下游需要时自相加)
struct GridMarginDetail {
  std::vector<float> mr_bal, ms_bal;
};

struct ForecastEv {
  int v;
  std::string end_date;
  std::string type;
  float last_parent_net;
};

struct ReportEv {
  int v;
  std::string end_date;
};

// st.st_tpye (tushare 原字段, 拼写为 tpye) 的 13 个枚举.
// 状态机轴: 维护 (n_st, n_star) 双计数器 + (delist_period, high_risk) 两 latch.
// risk_warn 输出: 0=正常, 1=ST (仅 n_st>0), 2=*ST (n_star>0 ∨ delist ∨ high_risk).
enum class StType : uint8_t {
  st,                  // "ST"               n_st += 1
  st_overlay,          // "叠加ST"           n_st += 1
  st_revoke,           // "撤销ST"           n_st = max(0, n_st-1)
  st_overlay_revoke,   // "撤销叠加ST"       n_st = max(0, n_st-1)
  star,                // "*ST"              n_star += 1
  star_overlay,        // "叠加*ST"          n_star += 1
  star_revoke,         // "撤销*ST"          n_star = max(0, n_star-1)
  star_overlay_revoke, // "撤销叠加*ST"      n_star = max(0, n_star-1)
  st_to_star,          // "从ST变为*ST"      n_st = max(0, n_st-1); n_star += 1
  star_to_st,          // "撤消*ST并实行ST"  n_star = max(0, n_star-1); n_st += 1
  delist_period,       // "退市整理期"       delist_latch = true (永久)
  high_risk,           // "高风险警示"       high_risk_latch = true
  high_risk_revoke,    // "撤销高风险警示"   high_risk_latch = false
};

struct STEv {
  int v;
  StType type;
};

struct DividendEv {
  int v;
  std::string end_date;
  std::string div_proc;
  float cash_div_tax;
};

struct IncomeEv {
  int v;
  std::string end_date;
  std::string report_type;
  float revenue;
  float n_income_attr_p;
};

struct CashflowEv {
  int v;
  std::string end_date;
  std::string report_type;
  float n_cashflow_act;
};

struct FinaIndEv {
  int v;
  std::string end_date;
  float roe;
  float roa;
};

template <class Ev>
using EventStore = std::vector<std::vector<Ev>>;

struct PitPool {
  GridDailyBasic daily_basic;
  GridStkLimit stk_limit;
  GridSuspendD suspend_d;
  GridMarginSecs margin_secs;
  GridMarginDetail margin_detail;

  EventStore<ForecastEv> forecast;
  EventStore<ReportEv> report;
  EventStore<STEv> st;
  EventStore<DividendEv> dividend;
  EventStore<IncomeEv> income;
  EventStore<CashflowEv> cashflow;
  EventStore<FinaIndEv> fina_indicator;
};

// ============================================================================
// ItfDesc: 单 itf 的描述. 在 pit.cpp 内每个 itf 一组 fn (prealloc + parse + post_sort)
//   集中定义, 然后填进 ITFS[] 表. load.cpp 仅迭代该表, 不出现具体 itf 名.
//   增减 itf 只需 (1) PitPool 加字段 (2) pit.cpp 加一组 fn (3) ITFS[] 加一行.
//
// 函数职责:
//   prealloc(axes, pool):
//     初始化 pool 中此 itf 的字段. 网格 itf 把每个字段 vector 设为 length=n_a*n_d 的 NaN/0;
//     事件 itf 把 EventStore[a] 设为 length=n_a 的空链.
//   parse(arr, v_idx, axes, pool, mu):
//     解析单 (day, itf) json 数组到 pool. 网格场合 mu==nullptr, 直接 dense slot 写入无锁;
//     事件场合 mu 是 length=n_a 的 mutex 数组, 按 a 取锁 emplace.
//   post_sort(pool):
//     事件 itf 末段 sort by v 升序 (Phase 2 走单调指针扫). 网格 itf 留 nullptr.
// ============================================================================
struct ItfDesc {
  const char *file_name; // .json basename, 也是日志/标识用名
  bool is_event;         // false=网格 (无锁), true=事件 (per-A mutex)

  void (*prealloc)(const Axes &, PitPool &);
  void (*parse)(yyjson_val *arr, int v_idx, const Axes &, PitPool &,
                std::vector<std::mutex> *mu /* 网格场合可为 nullptr */);
  void (*post_sort)(PitPool &);                // 事件 itf 末段 sort by v; 网格 itf 留 nullptr
  void (*post_ffill)(const Axes &, PitPool &); // 网格 itf per-A forward fill; 事件 itf 留 nullptr
};

extern const ItfDesc ITFS[];
extern const int ITFS_COUNT;

} // namespace feature
