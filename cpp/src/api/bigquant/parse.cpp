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

namespace bigquant {

namespace {

// timestamp[ns] -> "YYYYMMDD" 字符串.
//   DAI 端日期列时间分量恒为 00:00:00, 直接 UTC gmtime_r 解 ns→y/m/d 即可,
//   与 fetch.py `df[col].dt.strftime("%Y%m%d")` 同语义.
void append_timestamp(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                      const arrow::TimestampArray &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i)) {
      yyjson_mut_arr_add_null(doc, arr);
      continue;
    }
    int64_t ns = a.Value(i);
    std::time_t secs = static_cast<std::time_t>(ns / 1'000'000'000LL);
    std::tm tm_utc{};
    gmtime_r(&secs, &tm_utc);
    char buf[16];
    int n = std::snprintf(buf, sizeof(buf), "%04d%02d%02d",
                          tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday);
    assert(n == 8);
    yyjson_mut_arr_add_strncpy(doc, arr, buf, 8);
  }
}

template <typename ArrayT>
void append_int(yyjson_mut_doc *doc, yyjson_mut_val *arr, const ArrayT &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_int(doc, arr, static_cast<int64_t>(a.Value(i)));
  }
}

template <typename ArrayT>
void append_uint(yyjson_mut_doc *doc, yyjson_mut_val *arr, const ArrayT &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_uint(doc, arr, static_cast<uint64_t>(a.Value(i)));
  }
}

template <typename ArrayT>
void append_real(yyjson_mut_doc *doc, yyjson_mut_val *arr, const ArrayT &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i)) {
      yyjson_mut_arr_add_null(doc, arr);
      continue;
    }
    double v = static_cast<double>(a.Value(i));
    if (std::isnan(v) || std::isinf(v))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_real(doc, arr, v);
  }
}

void append_string(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                   const arrow::StringArray &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i)) {
      yyjson_mut_arr_add_null(doc, arr);
    } else {
      auto sv = a.GetView(i);
      yyjson_mut_arr_add_strncpy(doc, arr, sv.data(), sv.size());
    }
  }
}

void append_bool(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                 const arrow::BooleanArray &a) {
  for (int64_t i = 0; i < a.length(); ++i) {
    if (a.IsNull(i))
      yyjson_mut_arr_add_null(doc, arr);
    else
      yyjson_mut_arr_add_bool(doc, arr, a.Value(i));
  }
}

void append_all_null(yyjson_mut_doc *doc, yyjson_mut_val *arr, int64_t n) {
  for (int64_t i = 0; i < n; ++i)
    yyjson_mut_arr_add_null(doc, arr);
}

void dispatch_chunk(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                    const arrow::Array &chunk) {
  using T = arrow::Type;
  switch (chunk.type_id()) {
  case T::TIMESTAMP:
    append_timestamp(doc, arr, static_cast<const arrow::TimestampArray &>(chunk));
    break;
  case T::STRING:
    append_string(doc, arr, static_cast<const arrow::StringArray &>(chunk));
    break;
  case T::DOUBLE:
    append_real(doc, arr, static_cast<const arrow::DoubleArray &>(chunk));
    break;
  case T::FLOAT:
    append_real(doc, arr, static_cast<const arrow::FloatArray &>(chunk));
    break;
  case T::INT8:
    append_int(doc, arr, static_cast<const arrow::Int8Array &>(chunk));
    break;
  case T::INT16:
    append_int(doc, arr, static_cast<const arrow::Int16Array &>(chunk));
    break;
  case T::INT32:
    append_int(doc, arr, static_cast<const arrow::Int32Array &>(chunk));
    break;
  case T::INT64:
    append_int(doc, arr, static_cast<const arrow::Int64Array &>(chunk));
    break;
  case T::UINT8:
    append_uint(doc, arr, static_cast<const arrow::UInt8Array &>(chunk));
    break;
  case T::UINT16:
    append_uint(doc, arr, static_cast<const arrow::UInt16Array &>(chunk));
    break;
  case T::UINT32:
    append_uint(doc, arr, static_cast<const arrow::UInt32Array &>(chunk));
    break;
  case T::UINT64:
    append_uint(doc, arr, static_cast<const arrow::UInt64Array &>(chunk));
    break;
  case T::BOOL:
    append_bool(doc, arr, static_cast<const arrow::BooleanArray &>(chunk));
    break;
  case T::NA:
    append_all_null(doc, arr, chunk.length());
    break;
  default:
    std::cerr << "[bigquant.parse] unsupported arrow type: "
              << chunk.type()->ToString() << " (type_id=" << chunk.type_id() << ")"
              << std::endl;
    assert(false && "bigquant::parse: unsupported arrow type");
  }
}

} // namespace

yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t) {
  assert(t);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  const auto &fields = t->schema()->fields();
  for (int i = 0; i < t->num_columns(); ++i) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    const auto &col = t->column(i);
    for (int c = 0; c < col->num_chunks(); ++c) {
      dispatch_chunk(doc, arr, *col->chunk(c));
    }
    // key 必须复制 (字段名生命周期绑 Table; doc 生命周期可能超过 Table).
    const std::string &name = fields[i]->name();
    yyjson_mut_val *key = yyjson_mut_strncpy(doc, name.data(), name.size());
    yyjson_mut_obj_add(root, key, arr);
  }
  return doc;
}

} // namespace bigquant
