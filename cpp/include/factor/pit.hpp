#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace factor {

// PIT 中间结构. Phase 1 写入, Phase 2 只读.
//
// 划分:
//   网格 itf (1 record / 交易日 / asset): dense 存. 字段独立向量, length = n_a() * n_d() (a-major, d-minor).
//                                          缺席用 NaN; 与 Tensor::ts_row 同 layout, Phase 2 可零 copy 取 span.
//   事件 itf (per A 时间线):              EventStore<Ev> = vector<vector<Ev>>, [a] 外, 按 v 升序的事件链.
//                                          v = visible_d_idx (Axes.floor_date(visible_date) 后的 D 索引);
//                                          周末/节假日的 visible_date 已 floor 到上一交易日.

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

struct STEv {
  int v;
  std::string st_name; // st.name (变更后名), 含 "ST" 子串 → 上线
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
  GridStkLimit   stk_limit;
  GridSuspendD   suspend_d;

  EventStore<ForecastEv> forecast;
  EventStore<ReportEv>   report;
  EventStore<STEv>       st;
  EventStore<DividendEv> dividend;
  EventStore<IncomeEv>   income;
  EventStore<CashflowEv> cashflow;
  EventStore<FinaIndEv>  fina_indicator;
};

} // namespace factor
