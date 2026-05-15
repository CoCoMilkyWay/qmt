#include "api/tushare/pipeline.hpp"

#include "api/tushare/http.hpp"
#include "api/tushare/store.hpp"
#include "config.hpp"
#include "misc/store.hpp"

#include <iostream>

namespace tushare {

// ============================================================================
// Tushare fallback 流水线
//   - 仅 3 张事件表 (forecast/express/disclosure); 其余 itf 一律走 bigquant
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

    std::cout << "\n[" << spec.name << "] scan ..." << std::flush;
    auto missing =
        misc::store::scan_missing_days(spec.name, start, end, lookback_days);
    std::cout << " " << missing.size() << " day(s) to fetch" << std::endl;

    if (!missing.empty()) {
      auto tasks = spec.strategy->plan(missing);
      std::cout << "[" << spec.name << "] plan -> " << tasks.size()
                << " fetch task(s)" << std::endl;

      for (size_t i = 0; i < tasks.size(); i++) {
        const auto &task = tasks[i];
        std::cout << "  [" << (i + 1) << "/" << tasks.size() << "] "
                  << task.start;
        if (task.end != task.start)
          std::cout << "~" << task.end;
        std::cout << " ... " << std::flush;

        yyjson_doc *doc = spec.strategy->fetch(http, task, spec);
        size_t n_records = yyjson_arr_size(
            yyjson_obj_get(yyjson_obj_get(yyjson_doc_get_root(doc), "data"),
                           "items"));
        store::write_by_visible_date(doc, spec, task);
        yyjson_doc_free(doc);

        std::cout << n_records << " records" << std::endl;
      }
    }

    misc::store::mark_api_updated(spec.name);
  }

  std::cout << "\n[tushare.update] done" << std::endl;
}

} // namespace tushare
