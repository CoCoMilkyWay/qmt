#include "api/tushare/pipeline.hpp"

#include "api/tushare/http.hpp"
#include "api/tushare/store.hpp"
#include "config.hpp"
#include "misc/store.hpp"

#include <cassert>
#include <iostream>

namespace tushare {

// ============================================================================
// Tushare fallback 流水线
//   - 仅 3 张事件表 (forecast/express/disclosure); 其余 itf 一律走 bigquant
//   - axis/static (stock_basic / industry / namechange) 全部移交 bigquant
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs, int lookback_days) {
  Http http(::config::TUSHARE_TOKEN);

  std::cout << "[tushare.update] " << start << " ~ " << end << " ("
            << specs.size() << " interfaces, lookback=" << lookback_days
            << "d, dedup=" << ::config::API_DEDUP_WINDOW_SECONDS << "s)"
            << std::endl;

  for (const auto &spec : specs) {
    if (misc::store::should_skip_api(spec.name,
                                     ::config::API_DEDUP_WINDOW_SECONDS)) {
      std::cout << "\n[" << spec.name << "] skip (recently updated)"
                << std::endl;
      continue;
    }

    std::cout << "\n[" << spec.name << "] scan ..." << std::flush;
    auto missing = store::scan_missing(spec, start, end, lookback_days);
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
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *data = yyjson_obj_get(root, "data");
        assert(data);
        yyjson_val *fields_arr = yyjson_obj_get(data, "fields");
        yyjson_val *items_arr = yyjson_obj_get(data, "items");
        assert(fields_arr && items_arr);

        size_t n_records = yyjson_arr_size(items_arr);
        store::write_by_visible_date(fields_arr, items_arr, spec, task);
        yyjson_doc_free(doc);

        std::cout << n_records << " records" << std::endl;
      }
    }

    misc::store::mark_api_updated(spec.name);
  }

  std::cout << "\n[tushare.update] done" << std::endl;
}

} // namespace tushare
