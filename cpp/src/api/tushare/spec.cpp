#include "api/tushare/spec.hpp"
#include "config.hpp"
#include "misc/date.hpp"
#include "api/tushare/http.hpp"

#include <algorithm>
#include <chrono>

namespace tushare {

using std::chrono::days;
using std::chrono::sys_days;

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

namespace {

// 把 missing (按字典序升序) 切成连续段，每段长度 ≤ max_days
std::vector<std::pair<std::string, std::string>>
split_segments(const std::vector<std::string> &missing, int max_days) {
  std::vector<std::pair<std::string, std::string>> segments;
  if (missing.empty())
    return segments;

  sys_days seg_start = misc::parse_yyyymmdd(missing[0]);
  sys_days seg_prev = seg_start;
  auto flush = [&](sys_days end) {
    sys_days cur = seg_start;
    while (cur <= end) {
      sys_days block_end = std::min(cur + days{max_days - 1}, end);
      segments.emplace_back(misc::fmt_yyyymmdd(cur),
                            misc::fmt_yyyymmdd(block_end));
      cur = block_end + days{1};
    }
  };
  for (size_t i = 1; i < missing.size(); i++) {
    sys_days cur = misc::parse_yyyymmdd(missing[i]);
    if (cur == seg_prev + days{1}) {
      seg_prev = cur;
    } else {
      flush(seg_prev);
      seg_start = cur;
      seg_prev = cur;
    }
  }
  flush(seg_prev);
  return segments;
}

} // namespace

std::vector<FetchTask>
RangeStrategy::plan(const std::vector<std::string> &missing) const {
  auto segments = split_segments(missing, max_days_);

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
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    {"express",
     "express_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    {"disclosure",
     "disclosure_date",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date"}),
     {"actual_date", "modify_date"}},
};

} // namespace tushare
