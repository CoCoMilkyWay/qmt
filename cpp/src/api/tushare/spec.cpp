#include "api/tushare/spec.hpp"
#include "api/tushare/http.hpp"

#include <cassert>

namespace tushare {

// ============================================================================
// FetchStrategy::segment_to_tasks — 单一判别式 dispatch
// ============================================================================

std::vector<FetchTask>
FetchStrategy::segment_to_tasks(const misc::FetchSegment &seg) const {
  // range-capable: 段 [s, e] → 1 task (start_date / end_date 闭区间)
  if (day_params.empty()) {
    return {{seg.start, seg.end,
             {{"start_date", seg.start}, {"end_date", seg.end}}}};
  }
  // per-day-only: 段必为 [d, d] (scheduler 已用 can_range=false 强制); 每 day_param 一 task
  assert(seg.start == seg.end);
  std::vector<FetchTask> tasks;
  tasks.reserve(day_params.size());
  for (auto &p : day_params) {
    tasks.push_back({seg.start, seg.end, {{p, seg.start}}});
  }
  return tasks;
}

// ============================================================================
// SPECS — BigQuant 无等价的事件型 fallback (3 张)
//   forecast    业绩预告:        visible=ann_date, PK=(ts_code, end_date)
//   express     业绩快报:        visible=ann_date, PK=(ts_code, end_date)
//   disclosure  财报披露计划:    visible=ann_date, PK=(ts_code, end_date)
//               drop actual_date/modify_date (在 ann_date 之后由 tushare 回填 = 未来信息)
// ============================================================================

const std::vector<InterfaceSpec> SPECS = {
    {"forecast",   "forecast_vip",   {"ann_date"}, {"ts_code", "end_date"}, /*strategy=*/{}},
    {"express",    "express_vip",    {"ann_date"}, {"ts_code", "end_date"}, /*strategy=*/{}},
    {"disclosure", "disclosure_date", {"ann_date"}, {"ts_code", "end_date"},
     /*strategy=*/{{"ann_date"}}, /*drop=*/{"actual_date", "modify_date"}},
};

// ============================================================================
// fetch — 一步式 HTTP 查询 (与 bigquant::fetch 对仗)
// ============================================================================

yyjson_doc *fetch(Http &http, const InterfaceSpec &spec, const FetchTask &task) {
  return http.call(spec.api, task.params);
}

} // namespace tushare
