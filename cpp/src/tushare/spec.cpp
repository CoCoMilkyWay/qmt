#include "tushare/spec.hpp"
#include "tushare/http.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

namespace tushare {

namespace {

using namespace std::chrono;

sys_days parse_yyyymmdd(std::string_view s) {
  assert(s.size() == 8);
  int y = std::stoi(std::string(s.substr(0, 4)));
  int m = std::stoi(std::string(s.substr(4, 2)));
  int d = std::stoi(std::string(s.substr(6, 2)));
  return sys_days{year{y} / m / d};
}

std::string fmt_yyyymmdd(sys_days d) {
  year_month_day ymd{d};
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d", static_cast<int>(ymd.year()),
                static_cast<unsigned>(ymd.month()),
                static_cast<unsigned>(ymd.day()));
  return buf;
}

} // namespace

std::string today_yyyymmdd() {
  auto now = system_clock::now();
  auto t = system_clock::to_time_t(now);
  std::tm tm_buf{};
  localtime_r(&t, &tm_buf);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d", tm_buf.tm_year + 1900,
                tm_buf.tm_mon + 1, tm_buf.tm_mday);
  return buf;
}

std::vector<std::string> iter_days(std::string_view start,
                                   std::string_view end) {
  std::vector<std::string> out;
  sys_days s = parse_yyyymmdd(start);
  sys_days e = parse_yyyymmdd(end);
  for (sys_days d = s; d <= e; d += days{1}) {
    out.push_back(fmt_yyyymmdd(d));
  }
  return out;
}

std::string add_days(std::string_view yyyymmdd, int n) {
  return fmt_yyyymmdd(parse_yyyymmdd(yyyymmdd) + days{n});
}

// ============================================================================
// DateRangeStrategy
// ============================================================================

std::vector<FetchTask>
DateRangeStrategy::plan(const std::vector<std::string> &missing) const {
  if (missing.empty())
    return {};

  std::vector<FetchTask> tasks;
  sys_days seg_start = parse_yyyymmdd(missing[0]);
  sys_days seg_prev = seg_start;

  auto flush = [&](sys_days end) {
    sys_days cur = seg_start;
    while (cur <= end) {
      sys_days block_end = std::min(cur + days{max_days_ - 1}, end);
      tasks.push_back({fmt_yyyymmdd(cur), fmt_yyyymmdd(block_end)});
      cur = block_end + days{1};
    }
  };

  for (size_t i = 1; i < missing.size(); i++) {
    sys_days cur = parse_yyyymmdd(missing[i]);
    if (cur == seg_prev + days{1}) {
      seg_prev = cur;
    } else {
      flush(seg_prev);
      seg_start = cur;
      seg_prev = cur;
    }
  }
  flush(seg_prev);
  return tasks;
}

yyjson_doc *DateRangeStrategy::fetch(Http &http, const FetchTask &task,
                                     const InterfaceSpec &spec) const {
  return http.call(spec.api, {
                                 {"start_date", task.start},
                                 {"end_date", task.end},
                             });
}

// ============================================================================
// SingleDateStrategy
// ============================================================================

std::vector<FetchTask>
SingleDateStrategy::plan(const std::vector<std::string> &missing) const {
  std::vector<FetchTask> tasks;
  tasks.reserve(missing.size());
  for (auto &d : missing)
    tasks.push_back({d, d});
  return tasks;
}

yyjson_doc *SingleDateStrategy::fetch(Http &http, const FetchTask &task,
                                      const InterfaceSpec &spec) const {
  return http.call(spec.api, {{visible_param_, task.start}});
}

// ============================================================================
// SPECS
// ============================================================================

const std::vector<InterfaceSpec> SPECS = {
    {"forecast", "forecast_vip", "ann_date", {"ts_code", "end_date"},
     std::make_shared<DateRangeStrategy>()},
    {"express", "express_vip", "ann_date", {"ts_code", "end_date"},
     std::make_shared<DateRangeStrategy>()},
    {"disclosure",
     "disclosure_date",
     "actual_date",
     {"ts_code", "end_date"},
     std::make_shared<SingleDateStrategy>("actual_date")},
};

} // namespace tushare
