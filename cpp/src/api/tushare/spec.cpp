#include "api/tushare/spec.hpp"
#include "api/tushare/http.hpp"
#include "config.hpp"
#include "misc/date.hpp"

namespace tushare {

// ============================================================================
// 通用 fetch：task.params 已是完整 query
// ============================================================================

yyjson_doc *FetchStrategy::fetch(Http &http, const FetchTask &task,
                                 const InterfaceSpec &spec) const {
  return http.call(spec.api, task.params);
}

// ============================================================================
// RangeStrategy
// ============================================================================

std::vector<FetchTask>
RangeStrategy::plan(const std::vector<std::string> &missing) const {
  auto segments = misc::split_segments(missing, max_days_);

  std::vector<FetchTask> tasks;
  size_t n_variants = variant_values_.empty() ? 1 : variant_values_.size();
  tasks.reserve(segments.size() * n_variants);

  for (auto &[s, e] : segments) {
    if (variant_values_.empty()) {
      tasks.push_back({s, e, {{"start_date", s}, {"end_date", e}}});
    } else {
      for (auto &v : variant_values_) {
        tasks.push_back(
            {s, e, {{"start_date", s}, {"end_date", e}, {variant_key_, v}}});
      }
    }
  }
  return tasks;
}

// ============================================================================
// PerDayStrategy
// ============================================================================

std::vector<FetchTask>
PerDayStrategy::plan(const std::vector<std::string> &missing) const {
  std::vector<FetchTask> tasks;
  tasks.reserve(missing.size() * day_params_.size());
  for (auto &d : missing) {
    for (auto &p : day_params_) {
      tasks.push_back({d, d, {{p, d}}});
    }
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
     std::make_shared<RangeStrategy>(::config::TUSHARE_FETCH_MAX_DAYS_PER_CALL)},
    {"express",
     "express_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::TUSHARE_FETCH_MAX_DAYS_PER_CALL)},
    {"disclosure",
     "disclosure_date",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date"}),
     {"actual_date", "modify_date"}},
};

} // namespace tushare
