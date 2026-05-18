#include "api/tushare/pipeline.hpp"

#include "api/tushare/http.hpp"
#include "api/tushare/spec.hpp"
#include "api/tushare/store.hpp"
#include "config.hpp"
#include "misc/schedule.hpp"
#include "misc/store.hpp"

#include <iostream>

namespace tushare {

namespace {

// 单 task fetch + 落盘 (与 bigquant::fetch_and_write 对仗).
void fetch_and_write(Http &http, const InterfaceSpec &spec,
                     const FetchTask &task) {
  std::cout << "  " << task.start;
  if (task.end != task.start)
    std::cout << "~" << task.end;
  std::cout << " ... " << std::flush;

  yyjson_doc *doc = fetch(http, spec, task);
  size_t n_records = yyjson_arr_size(
      yyjson_obj_get(yyjson_obj_get(yyjson_doc_get_root(doc), "data"),
                     "items"));
  store::write_by_visible_date(doc, spec, task.start, task.end);
  yyjson_doc_free(doc);

  std::cout << n_records << " records" << std::endl;
}

} // namespace

// ============================================================================
// Tushare fallback 流水线
//   - 仅 3 张事件表 (forecast/express/disclosure); 其余 itf 一律走 bigquant
//   - 调度统一走 misc::plan_day_segments (整月空洞→月段 / 局部→日段 / lookback)
//   - 段 → task 由 strategy::segment_to_tasks 决定 (range API 1 task, per-day N tasks)
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs, int lookback_days) {
  Http http(::config::TUSHARE_TOKEN);

  std::cout << "[tushare.update] " << start << " ~ " << end << " ("
            << specs.size() << " interfaces, lookback=" << lookback_days
            << "d, dedup=" << ::config::PIPELINE_DEDUP_WINDOW_SECONDS << "s)"
            << std::endl;

  for (const auto &spec : specs) {
    if (misc::store::should_skip_api(spec.name,
                                     ::config::PIPELINE_DEDUP_WINDOW_SECONDS)) {
      std::cout << "\n[" << spec.name << "] skip (recently updated)"
                << std::endl;
      continue;
    }

    std::cout << "\n[" << spec.name << "] plan ..." << std::flush;
    auto segments = misc::plan_day_segments(spec.name, start, end,
                                            lookback_days,
                                            spec.strategy.can_range());
    std::cout << " " << segments.size() << " segment(s)" << std::endl;

    for (const auto &seg : segments) {
      for (const auto &task : spec.strategy.segment_to_tasks(seg)) {
        fetch_and_write(http, spec, task);
      }
    }

    misc::store::mark_api_updated(spec.name);
  }

  std::cout << "\n[tushare.update] done" << std::endl;
}

} // namespace tushare
