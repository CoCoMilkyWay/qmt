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

void array_value_append(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                        const arrow::Array &a, int64_t i) {
  if (a.IsNull(i)) {
    yyjson_mut_arr_add_null(doc, arr);
    return;
  }
  using T = arrow::Type;
  switch (a.type_id()) {
  case T::TIMESTAMP: {
    char buf[9];
    format_ts_yyyymmdd(static_cast<const arrow::TimestampArray &>(a).Value(i),
                       buf);
    yyjson_mut_arr_add_strncpy(doc, arr, buf, 8);
    return;
  }
  case T::STRING: {
    auto sv = static_cast<const arrow::StringArray &>(a).GetView(i);
    yyjson_mut_arr_add_strncpy(doc, arr, sv.data(), sv.size());
    return;
  }
  case T::DOUBLE: {
    double v = static_cast<const arrow::DoubleArray &>(a).Value(i);
    if (std::isnan(v) || std::isinf(v))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_real(doc, arr, v);
    return;
  }
  case T::FLOAT: {
    double v = static_cast<double>(static_cast<const arrow::FloatArray &>(a).Value(i));
    if (std::isnan(v) || std::isinf(v))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_real(doc, arr, v);
    return;
  }
  case T::INT8:
    yyjson_mut_arr_add_int(doc, arr,
                           static_cast<const arrow::Int8Array &>(a).Value(i));
    return;
  case T::INT16:
    yyjson_mut_arr_add_int(doc, arr,
                           static_cast<const arrow::Int16Array &>(a).Value(i));
    return;
  case T::INT32:
    yyjson_mut_arr_add_int(doc, arr,
                           static_cast<const arrow::Int32Array &>(a).Value(i));
    return;
  case T::INT64:
    yyjson_mut_arr_add_int(doc, arr,
                           static_cast<const arrow::Int64Array &>(a).Value(i));
    return;
  case T::UINT8:
    yyjson_mut_arr_add_uint(doc, arr,
                            static_cast<const arrow::UInt8Array &>(a).Value(i));
    return;
  case T::UINT16:
    yyjson_mut_arr_add_uint(doc, arr,
                            static_cast<const arrow::UInt16Array &>(a).Value(i));
    return;
  case T::UINT32:
    yyjson_mut_arr_add_uint(doc, arr,
                            static_cast<const arrow::UInt32Array &>(a).Value(i));
    return;
  case T::UINT64:
    yyjson_mut_arr_add_uint(doc, arr,
                            static_cast<const arrow::UInt64Array &>(a).Value(i));
    return;
  case T::BOOL:
    yyjson_mut_arr_add_bool(doc, arr,
                            static_cast<const arrow::BooleanArray &>(a).Value(i));
    return;
  case T::NA:
    yyjson_mut_arr_add_null(doc, arr);
    return;
  default:
    std::cerr << "[bigquant.parse] unsupported arrow type: "
              << a.type()->ToString() << " (type_id=" << a.type_id() << ")"
              << std::endl;
    assert(false && "bigquant::parse: unsupported arrow type");
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
// table_to_json: 整表
//   ChunkedArray 在内部 dispatch_chunk 内迭代每 chunk 的所有 row.
// ============================================================================

yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t) {
  assert(t);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  const auto &fields = t->schema()->fields();
  for (int c = 0; c < t->num_columns(); ++c) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    const auto &col = t->column(c);
    for (int k = 0; k < col->num_chunks(); ++k) {
      const auto &chunk = *col->chunk(k);
      for (int64_t i = 0; i < chunk.length(); ++i) {
        array_value_append(doc, arr, chunk, i);
      }
    }
    const std::string &name = fields[c]->name();
    yyjson_mut_val *key = yyjson_mut_strncpy(doc, name.data(), name.size());
    yyjson_mut_obj_add(root, key, arr);
  }
  return doc;
}

// ============================================================================
// table_subset_to_json: 行子集
//   先 CombineChunks 把每列合成 single chunk, 按 row_idxs 取值.
// ============================================================================

namespace {

// 找到 row_idx (全表) 落入 chunked array 的 (chunk_idx, in_chunk_idx).
// 简化: 把所有 chunk length 累加成 offsets 二分查找.
struct ChunkLocator {
  std::vector<int64_t> offsets; // size = num_chunks + 1, offsets[k] = chunk k 起始全表 row
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
  // 二分定位 row → (chunk_idx, in_chunk_idx).
  std::pair<int, int64_t> locate(int64_t row) const {
    // upper_bound - 1 = 包含 row 的 chunk
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

} // namespace

yyjson_mut_doc *table_subset_to_json(const std::shared_ptr<arrow::Table> &t,
                                     const std::vector<int64_t> &row_idxs) {
  assert(t);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  const auto &fields = t->schema()->fields();
  const int64_t n_rows = t->num_rows();
  for (int c = 0; c < t->num_columns(); ++c) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    const auto &col = *t->column(c);
    ChunkLocator loc;
    loc.build(col);
    for (int64_t row : row_idxs) {
      assert(row >= 0 && row < n_rows);
      auto [ck, ci] = loc.locate(row);
      array_value_append(doc, arr, *col.chunk(ck), ci);
    }
    const std::string &name = fields[c]->name();
    yyjson_mut_val *key = yyjson_mut_strncpy(doc, name.data(), name.size());
    yyjson_mut_obj_add(root, key, arr);
  }
  return doc;
}

} // namespace bigquant
