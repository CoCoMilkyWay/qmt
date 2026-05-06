#include "tushare/pipeline.hpp"
#include "tushare/http.hpp"
#include "tushare/store.hpp"

#include <cassert>
#include <iostream>

namespace tushare {

void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs) {
  Http http(load_token());

  std::cout << "[update] " << start << " ~ " << end << " ("
            << specs.size() << " interfaces)" << std::endl;

  for (const auto &spec : specs) {
    std::cout << "\n[" << spec.name << "] scan ..." << std::flush;
    auto missing = store::scan_missing(spec, start, end);
    std::cout << " " << missing.size() << " missing day(s)" << std::endl;

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
