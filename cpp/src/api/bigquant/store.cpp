#include "api/bigquant/store.hpp"

#include "api/bigquant/parse.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"

#include <arrow/array.h>
#include <arrow/chunked_array.h>
#include <arrow/table.h>
#include <arrow/type.h>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bigquant::store {

namespace fs = std::filesystem;

// ============================================================================
// scan_missing — Day / MonthFirst 自动分发 (Static 不应走此路径, assert 拦截)
// ============================================================================

std::vector<std::string> scan_missing(const TableSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days) {
  assert(spec.kind != FetchKind::Static &&
         "Static 表不走 per-day scan; 用 write_meta_table");
  if (spec.freq == FetchFreq::Day) {
    return misc::store::scan_missing_days(spec.name, start, end, lookback_days);
  }
  // MonthFirst: 返回 missing month segments 的首日列表, 调用方按段 fetch.
  // 这里给的是 segments 而不是 days; pipeline 自己解读. 但本函数签名是 days...
  // 为统一签名: 给"每月首日"代表月; pipeline 按月调度.
  // 不过 BigQuant MonthFirst 只有 industry_component 用, pipeline 单独按月调度,
  // 不走 scan_missing(days) 路径.
  assert(false && "MonthFirst 走 misc::store::scan_missing_months, pipeline 直调");
  return {};
}

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

namespace {

// 在 table->schema 找列下标; 缺列 assert.
int field_index_of(const arrow::Table &t, const std::string &name) {
  const auto &fields = t.schema()->fields();
  for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
    if (fields[i]->name() == name)
      return i;
  }
  assert(false && "bigquant::store: 字段名不在 schema 中");
  return -1;
}

// row_idx (全表) → 该 row 对应 chunked_array 的 (chunk_idx, in_chunk_idx)
struct ChunkLoc {
  std::vector<int64_t> offsets;
  void build(const arrow::ChunkedArray &c) {
    offsets.clear();
    offsets.reserve(c.num_chunks() + 1);
    int64_t acc = 0;
    offsets.push_back(0);
    for (int k = 0; k < c.num_chunks(); ++k) {
      acc += c.chunk(k)->length();
      offsets.push_back(acc);
    }
  }
  std::pair<int, int64_t> locate(int64_t row) const {
    int lo = 0, hi = static_cast<int>(offsets.size()) - 1;
    while (lo + 1 < hi) {
      int mid = (lo + hi) / 2;
      if (offsets[mid] <= row)
        lo = mid;
      else
        hi = mid;
    }
    return {lo, row - offsets[lo]};
  }
};

// 构造单行 PK key: pk 列值拼接, 字段间 '|' 分隔
std::string make_pk_key(const arrow::Table &t,
                        const std::vector<int> &pk_idxs,
                        const std::vector<ChunkLoc> &pk_locs, int64_t row) {
  std::string key;
  for (size_t k = 0; k < pk_idxs.size(); ++k) {
    int col_idx = pk_idxs[k];
    const auto &col = *t.column(col_idx);
    auto [ck, ci] = pk_locs[k].locate(row);
    key += array_value_to_string(*col.chunk(ck), ci);
    key += '|';
  }
  return key;
}

// 取 visible_date 列 row 的 YYYYMMDD 字符串值; null → 空串
std::string get_visible_date(const arrow::Table &t, int vd_idx,
                             const ChunkLoc &vd_loc, int64_t row) {
  const auto &col = *t.column(vd_idx);
  auto [ck, ci] = vd_loc.locate(row);
  return array_value_to_string(*col.chunk(ck), ci);
}

} // namespace

void write_table_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                                 const TableSpec &spec,
                                 std::string_view start,
                                 std::string_view end) {
  assert(t);
  assert(spec.kind != FetchKind::Static);
  assert(!spec.visible_date.empty());

  const int64_t n_rows = t->num_rows();
  int vd_idx = field_index_of(*t, spec.visible_date);
  ChunkLoc vd_loc;
  vd_loc.build(*t->column(vd_idx));

  // PK 列下标 + 各 ChunkLoc (用于按 row 取值)
  std::vector<int> pk_idxs;
  std::vector<ChunkLoc> pk_locs;
  pk_idxs.reserve(spec.pk.size());
  pk_locs.resize(spec.pk.size());
  for (size_t k = 0; k < spec.pk.size(); ++k) {
    int idx = field_index_of(*t, spec.pk[k]);
    pk_idxs.push_back(idx);
    pk_locs[k].build(*t->column(idx));
  }

  // ---- 分桶 by visible_date ----
  // 同时做"同次响应 PK upsert": 同 PK 末条胜; 桶内顺序按首次出现行号升序.
  struct DayBucket {
    std::vector<int64_t> rows;                      // 最终保留的 row_idxs
    std::unordered_map<std::string, size_t> pk2pos; // pk_key → rows 中的位置
  };
  std::unordered_map<std::string, DayBucket> by_day;

  std::string start_s(start), end_s(end);
  for (int64_t row = 0; row < n_rows; ++row) {
    std::string vd = get_visible_date(*t, vd_idx, vd_loc, row);
    if (vd.size() != 8)
      continue; // null / 异常值跳过
    if (vd < start_s || vd > end_s)
      continue; // 范围外
    DayBucket &bk = by_day[vd];
    std::string pk_key = make_pk_key(*t, pk_idxs, pk_locs, row);
    auto it = bk.pk2pos.find(pk_key);
    if (it == bk.pk2pos.end()) {
      bk.pk2pos.emplace(std::move(pk_key), bk.rows.size());
      bk.rows.push_back(row);
    } else {
      // 同次响应同 day 同 PK 多条 — BigQuant PIT 服务端应已 dedup;
      // 此处 fail-fast 暴露 spec.pk 推断错误或服务端异常.
      assert(false && "bigquant::store: 同次响应 day file 内 PK 冲突");
      bk.rows[it->second] = row;
    }
  }

  // ---- 写 day file ----
  for (auto &[vd, bk] : by_day) {
    if (bk.rows.empty())
      continue;
    yyjson_mut_doc *doc = table_subset_to_json(t, bk.rows);
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
  std::unordered_map<std::string, misc::store::EmptyMonth> dirty_months;
  auto days = misc::iter_days(start, end);
  for (auto &d : days) {
    bool has_data = by_day.count(d) > 0;
    bool day_exists =
        has_data || fs::exists(misc::store::day_data_path(d, spec.name));
    std::string yyyy = d.substr(0, 4);
    std::string mm = d.substr(4, 2);
    std::string dd = d.substr(6, 2);
    std::string ym = yyyy + mm;
    auto mit = dirty_months.find(ym);
    if (mit == dirty_months.end()) {
      mit = dirty_months
                .emplace(ym, misc::store::read_empty_month(yyyy, mm))
                .first;
    }
    misc::store::EmptySet &set = mit->second[spec.name];
    if (day_exists)
      set.erase(dd);
    else
      set.insert(dd);
  }

  for (auto &[ym, month] : dirty_months) {
    misc::store::write_empty_month(ym.substr(0, 4), ym.substr(4, 2), month);
  }
}

} // namespace bigquant::store
