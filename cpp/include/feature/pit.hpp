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
  bool        is_event;  // false=网格 (无锁), true=事件 (per-A mutex)

  void (*prealloc)(const Axes &, PitPool &);
  void (*parse)(yyjson_val *arr, int v_idx, const Axes &, PitPool &,
                std::vector<std::mutex> *mu /* 网格场合可为 nullptr */);
  void (*post_sort)(PitPool &);              // 事件 itf 末段 sort by v; 网格 itf 留 nullptr
  void (*post_ffill)(const Axes &, PitPool &); // 网格 itf per-A forward fill; 事件 itf 留 nullptr
};

extern const ItfDesc ITFS[];
extern const int     ITFS_COUNT;

} // namespace feature
