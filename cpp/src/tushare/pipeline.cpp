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
            << "d, dedup=" << ::config::API_DEDUP_WINDOW_SECONDS << "s)"
            << std::endl;

  // 全局 meta：每次 update 全量刷新，与 per-day SPECS 体系独立
  // 单 API 去重在 pipeline 入口处统一拦截 (refresh_* / 每个 spec)
  if (should_skip_api("stock_basic", ::config::API_DEDUP_WINDOW_SECONDS)) {
    std::cout << "\n[stock_basic] skip (recently updated)" << std::endl;
  } else {
    refresh_stock_basic(http);
    mark_api_updated("stock_basic");
  }

  if (should_skip_api("index_member_all",
                      ::config::API_DEDUP_WINDOW_SECONDS)) {
    std::cout << "\n[index_member_all] skip (recently updated)" << std::endl;
  } else {
    refresh_index_member_all(http);
    mark_api_updated("index_member_all");
  }

  for (const auto &spec : specs) {
    if (should_skip_api(spec.name, ::config::API_DEDUP_WINDOW_SECONDS)) {
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

    // 整段 (scan + plan + fetch + write) 走完无 assert → 标记成功
    mark_api_updated(spec.name);
  }

  std::cout << "\n[update] done" << std::endl;
}

} // namespace tushare
