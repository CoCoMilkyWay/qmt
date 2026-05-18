#include "api/tushare/spec.hpp"
#include "api/tushare/http.hpp"

#include <cassert>

namespace tushare {

// ============================================================================
// 通用 fetch：task.params 已是完整 query
// ============================================================================

yyjson_doc *FetchStrategy::fetch(Http &http, const FetchTask &task,
                                 const InterfaceSpec &spec) const {
  return http.call(spec.api, task.params);
}

// ============================================================================
// MonthStrategy — 段 [s, e] → 1 task (range query)
// ============================================================================

std::vector<FetchTask>
MonthStrategy::segment_to_tasks(const misc::FetchSegment &seg) const {
  return {{seg.start, seg.end,
           {{"start_date", seg.start}, {"end_date", seg.end}}}};
}

// ============================================================================
// PerDayStrategy — 段必为 [d, d]; 每个 day_param 一次 task
// ============================================================================

std::vector<FetchTask>
PerDayStrategy::segment_to_tasks(const misc::FetchSegment &seg) const {
  assert(seg.start == seg.end && "PerDay 段必为单日 (scheduler 已用 can_range=false 强制)");
  std::vector<FetchTask> tasks;
  tasks.reserve(day_params_.size());
  for (auto &p : day_params_) {
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
    {"forecast",
     "forecast_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<MonthStrategy>()},
    {"express",
     "express_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<MonthStrategy>()},
    {"disclosure",
     "disclosure_date",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date"}),
     {"actual_date", "modify_date"}},
};

} // namespace tushare
