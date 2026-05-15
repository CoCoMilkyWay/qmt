#include "api/bigquant/parse.hpp"

#include <arrow/array.h>
#include <arrow/chunked_array.h>
#include <arrow/table.h>
#include <arrow/type.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace bigquant {

namespace {

// timestamp[ns] -> "YYYYMMDD".
//   DAI 端日期列时间分量恒为 00:00:00, UTC gmtime_r 解 ns→y/m/d.
//   与 fetch.py `df[col].dt.strftime("%Y%m%d")` 同语义.
void format_ts_yyyymmdd(int64_t ns, char out[9]) {
  std::time_t secs = static_cast<std::time_t>(ns / 1'000'000'000LL);
  std::tm tm_utc{};
  gmtime_r(&secs, &tm_utc);
  int n = std::snprintf(out, 9, "%04d%02d%02d", tm_utc.tm_year + 1900,
                        tm_utc.tm_mon + 1, tm_utc.tm_mday);
  assert(n == 8);
}

} // namespace

yyjson_mut_val *array_value_to_json(yyjson_mut_doc *doc, const arrow::Array &a,
                                    int64_t i) {
  if (a.IsNull(i))
    return yyjson_mut_null(doc);
  using T = arrow::Type;
  switch (a.type_id()) {
  case T::TIMESTAMP: {
    char buf[9];
    format_ts_yyyymmdd(static_cast<const arrow::TimestampArray &>(a).Value(i),
                       buf);
    return yyjson_mut_strncpy(doc, buf, 8);
  }
  case T::STRING: {
    auto sv = static_cast<const arrow::StringArray &>(a).GetView(i);
    return yyjson_mut_strncpy(doc, sv.data(), sv.size());
  }
  case T::DOUBLE: {
    double v = static_cast<const arrow::DoubleArray &>(a).Value(i);
    if (std::isnan(v) || std::isinf(v))
      return yyjson_mut_null(doc);
    return yyjson_mut_real(doc, v);
  }
  case T::FLOAT: {
    double v = static_cast<double>(
        static_cast<const arrow::FloatArray &>(a).Value(i));
    if (std::isnan(v) || std::isinf(v))
      return yyjson_mut_null(doc);
    return yyjson_mut_real(doc, v);
  }
  case T::INT8:
    return yyjson_mut_int(
        doc, static_cast<const arrow::Int8Array &>(a).Value(i));
  case T::INT16:
    return yyjson_mut_int(
        doc, static_cast<const arrow::Int16Array &>(a).Value(i));
  case T::INT32:
    return yyjson_mut_int(
        doc, static_cast<const arrow::Int32Array &>(a).Value(i));
  case T::INT64:
    return yyjson_mut_int(
        doc, static_cast<const arrow::Int64Array &>(a).Value(i));
  case T::UINT8:
    return yyjson_mut_uint(
        doc, static_cast<const arrow::UInt8Array &>(a).Value(i));
  case T::UINT16:
    return yyjson_mut_uint(
        doc, static_cast<const arrow::UInt16Array &>(a).Value(i));
  case T::UINT32:
    return yyjson_mut_uint(
        doc, static_cast<const arrow::UInt32Array &>(a).Value(i));
  case T::UINT64:
    return yyjson_mut_uint(
        doc, static_cast<const arrow::UInt64Array &>(a).Value(i));
  case T::BOOL:
    return yyjson_mut_bool(
        doc, static_cast<const arrow::BooleanArray &>(a).Value(i));
  case T::NA:
    return yyjson_mut_null(doc);
  default:
    std::cerr << "[bigquant.parse] unsupported arrow type: "
              << a.type()->ToString() << " (type_id=" << a.type_id() << ")"
              << std::endl;
    assert(false && "bigquant::parse: unsupported arrow type");
    return yyjson_mut_null(doc);
  }
}

std::string array_value_to_string(const arrow::Array &a, int64_t i) {
  if (a.IsNull(i))
    return std::string{};
  using T = arrow::Type;
  switch (a.type_id()) {
  case T::TIMESTAMP: {
    char buf[9];
    format_ts_yyyymmdd(static_cast<const arrow::TimestampArray &>(a).Value(i),
                       buf);
    return std::string(buf, 8);
  }
  case T::STRING:
    return std::string(static_cast<const arrow::StringArray &>(a).GetView(i));
  case T::INT8:
    return std::to_string(static_cast<const arrow::Int8Array &>(a).Value(i));
  case T::INT16:
    return std::to_string(static_cast<const arrow::Int16Array &>(a).Value(i));
  case T::INT32:
    return std::to_string(static_cast<const arrow::Int32Array &>(a).Value(i));
  case T::INT64:
    return std::to_string(static_cast<const arrow::Int64Array &>(a).Value(i));
  case T::UINT8:
    return std::to_string(static_cast<const arrow::UInt8Array &>(a).Value(i));
  case T::UINT16:
    return std::to_string(static_cast<const arrow::UInt16Array &>(a).Value(i));
  case T::UINT32:
    return std::to_string(static_cast<const arrow::UInt32Array &>(a).Value(i));
  case T::UINT64:
    return std::to_string(static_cast<const arrow::UInt64Array &>(a).Value(i));
  case T::BOOL:
    return static_cast<const arrow::BooleanArray &>(a).Value(i) ? "1" : "0";
  case T::NA:
    return std::string{};
  default:
    std::cerr << "[bigquant.parse] PK 字段不应为类型: "
              << a.type()->ToString() << std::endl;
    assert(false && "bigquant::parse: PK 字段类型不支持");
    return std::string{};
  }
}

// ============================================================================
// ChunkLoc — 行式定位 (declared in parse.hpp)
// ============================================================================

void ChunkLoc::build(const arrow::ChunkedArray &c) {
  offsets.clear();
  offsets.reserve(c.num_chunks() + 1);
  int64_t acc = 0;
  offsets.push_back(0);
  for (int k = 0; k < c.num_chunks(); ++k) {
    acc += c.chunk(k)->length();
    offsets.push_back(acc);
  }
}

std::pair<int, int64_t> ChunkLoc::locate(int64_t row) const {
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

// ============================================================================
// 行式落盘共用 helpers
// ============================================================================

namespace {

// 构造单行 obj: 每列调 array_value_to_json + obj_add (key 拷贝, 解耦 table 生命周期)
yyjson_mut_val *build_row_obj(yyjson_mut_doc *doc,
                              const std::shared_ptr<arrow::Table> &t,
                              const std::vector<ChunkLoc> &locs,
                              const std::vector<std::string> &col_names,
                              int64_t row) {
  yyjson_mut_val *obj = yyjson_mut_obj(doc);
  const int n_cols = t->num_columns();
  for (int c = 0; c < n_cols; ++c) {
    const auto &col = *t->column(c);
    auto [ck, ci] = locs[c].locate(row);
    yyjson_mut_val *key = yyjson_mut_strncpy(doc, col_names[c].data(),
                                             col_names[c].size());
    yyjson_mut_val *val = array_value_to_json(doc, *col.chunk(ck), ci);
    yyjson_mut_obj_add(obj, key, val);
  }
  return obj;
}

void prep_cols(const std::shared_ptr<arrow::Table> &t,
               std::vector<ChunkLoc> &locs,
               std::vector<std::string> &col_names) {
  const int n_cols = t->num_columns();
  const auto &fields = t->schema()->fields();
  locs.assign(n_cols, ChunkLoc{});
  col_names.clear();
  col_names.reserve(n_cols);
  for (int c = 0; c < n_cols; ++c) {
    locs[c].build(*t->column(c));
    col_names.push_back(fields[c]->name());
  }
}

} // namespace

// ============================================================================
// table_to_json: 整表 → 行式 JSON 数组
// ============================================================================

yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t) {
  assert(t);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  std::vector<ChunkLoc> locs;
  std::vector<std::string> col_names;
  prep_cols(t, locs, col_names);

  const int64_t n_rows = t->num_rows();
  for (int64_t row = 0; row < n_rows; ++row) {
    yyjson_mut_arr_append(root, build_row_obj(doc, t, locs, col_names, row));
  }
  return doc;
}

// ============================================================================
// table_subset_to_json: 行子集 → 行式 JSON 数组
// ============================================================================

yyjson_mut_doc *table_subset_to_json(const std::shared_ptr<arrow::Table> &t,
                                     const std::vector<int64_t> &row_idxs) {
  assert(t);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  std::vector<ChunkLoc> locs;
  std::vector<std::string> col_names;
  prep_cols(t, locs, col_names);

  const int64_t n_rows = t->num_rows();
  for (int64_t row : row_idxs) {
    assert(row >= 0 && row < n_rows);
    yyjson_mut_arr_append(root, build_row_obj(doc, t, locs, col_names, row));
  }
  return doc;
}

} // namespace bigquant
