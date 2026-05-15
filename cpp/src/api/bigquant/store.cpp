#include "api/bigquant/store.hpp"

#include "api/bigquant/parse.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"

#include <arrow/array.h>
#include <arrow/chunked_array.h>
#include <arrow/table.h>
#include <arrow/type.h>

#include <cassert>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace bigquant::store {

// ============================================================================
// write_meta_table — Static 表全量刷新到 _meta/<name>.json
// ============================================================================

void write_meta_table(const std::shared_ptr<arrow::Table> &t,
                      const TableSpec &spec) {
  assert(spec.kind == FetchKind::Static);
  assert(t);
  yyjson_mut_doc *doc = table_to_json(t);
  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  misc::atomic_write(misc::store::meta_data_path(spec.name), json_str, out_len);
  std::free(json_str);
  yyjson_mut_doc_free(doc);
}

// ============================================================================
// write_table_by_visible_date — Partition / Where × Day / MonthFirst 共用
// ============================================================================
//
// 桶分逻辑 (含 PK upsert / 范围裁剪) 抽到 parse::bucket_by_visible_date 共享给
// bigquant::import_parquet, 本函数只负责 "桶 → day file" + "_empty 维护".
void write_table_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                                 const TableSpec &spec,
                                 std::string_view start,
                                 std::string_view end) {
  std::map<std::string, std::vector<int64_t>> by_day =
      bucket_by_visible_date(t, spec, start, end);

  // ---- 写 day file ----
  for (auto &[vd, rows] : by_day) {
    yyjson_mut_doc *doc = table_subset_to_json(t, rows);
    size_t out_len = 0;
    char *json_str =
        yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
    assert(json_str);
    misc::atomic_write(misc::store::day_data_path(vd, spec.name), json_str,
                       out_len);
    std::free(json_str);
    yyjson_mut_doc_free(doc);
  }

  // ---- 维护 _empty.json ([start, end] 全 days) ----
  misc::store::update_empty_for_range(
      spec.name, start, end,
      [&](const std::string &d) { return by_day.count(d) > 0; });
}

} // namespace bigquant::store
