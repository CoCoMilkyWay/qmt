#include "tushare/pipeline.hpp"
#include "config.hpp"
#include "tushare/http.hpp"
#include "tushare/meta.hpp"
#include "tushare/store.hpp"

#include <cassert>
#include <iostream>

namespace tushare {

void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs, int lookback_days) {
  Http http(::config::TUSHARE_TOKEN);

  std::cout << "[update] " << start << " ~ " << end << " ("
            << specs.size() << " interfaces, lookback=" << lookback_days
            << "d)" << std::endl;

  // 全局 meta：每次 update 全量刷新，与 per-day SPECS 体系独立
  refresh_stock_basic(http);

  for (const auto &spec : specs) {
    std::cout << "\n[" << spec.name << "] scan ..." << std::flush;
    auto missing = store::scan_missing(spec, start, end, lookback_days);
    std::cout << " " << missing.size() << " day(s) to fetch" << std::endl;

    if (missing.empty()) continue;

    auto tasks = spec.strategy->plan(missing);
    std::cout << "[" << spec.name << "] plan -> " << tasks.size()
              << " fetch task(s)" << std::endl;

    for (size_t i = 0; i < tasks.size(); i++) {
      const auto &task = tasks[i];
      std::cout << "  [" << (i + 1) << "/" << tasks.size() << "] " << task.start;
      if (task.end != task.start) std::cout << "~" << task.end;
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

  std::cout << "\n[update] done" << std::endl;
}

} // namespace tushare
