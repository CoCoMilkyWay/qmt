#pragma once

#include "package/yyjson/yyjson.h"

#include <arrow/array.h>
#include <arrow/table.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bigquant {

// ============================================================================
// arrow::Array -> yyjson 单值 helpers (供 store 切片用)
// ============================================================================

// 取 array[i] 的值, 构造 yyjson_mut_val (按 array->type_id 自动 dispatch).
// 类型映射:
//   timestamp[ns]   -> "YYYYMMDD" 字符串 (UTC; null -> null)
//   string          -> 原始字符串 (null -> null)
//   double / float  -> 数值 (NaN / Inf / null -> null)
//   int8/16/32/64   -> 整数 (null -> null)
//   uint8/16/32/64  -> 整数 (null -> null)
//   bool            -> bool (null -> null)
//   null (NA 列)    -> null
//   其余类型        -> assert (未在 api.md schema 中出现)
yyjson_mut_val *array_value_to_json(yyjson_mut_doc *doc,
                                    const arrow::Array &array, int64_t i);

// 取 array[i] 的值转字符串 (用于 PK key 构造)
//   timestamp[ns]   -> "YYYYMMDD"
//   string          -> 原始内容
//   int / uint      -> 十进制
//   bool            -> "0" / "1"
//   null            -> "" (空串)
//   double / float  -> assert (PK 字段不应为浮点)
std::string array_value_to_string(const arrow::Array &array, int64_t i);

// ============================================================================
// arrow::Table -> JSON (行式, 人眼可读)
// ============================================================================
//
// 输出形态 (行式, root = arr):
//   [
//     {"<col1>": v0, "<col2>": v0, ...},
//     {"<col1>": v1, "<col2>": v1, ...},
//     ...
//   ]
//
// 行数 = t->num_rows(); 每行一个 obj 含 t->num_columns() 个 key-value.
// 所有权: 返回 yyjson_mut_doc*, caller 调用 yyjson_mut_doc_free 释放.
//   建议序列化用 yyjson_mut_write(doc, ...).
yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t);

// ============================================================================
// arrow::Table -> JSON (行式, 按 row_idxs 子集)
// ============================================================================
//
// 输出形态同 table_to_json, 但仅包含 row_idxs 指定的行 (顺序 = row_idxs 顺序).
// row_idxs 必须 < t->num_rows(). 用于 store 按 visible_date 分桶后逐 day 输出.
yyjson_mut_doc *table_subset_to_json(const std::shared_ptr<arrow::Table> &t,
                                     const std::vector<int64_t> &row_idxs);

} // namespace bigquant
