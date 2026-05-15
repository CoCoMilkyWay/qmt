#pragma once

#include "package/yyjson/yyjson.h"

#include <arrow/table.h>

#include <memory>

namespace bigquant {

// ============================================================================
// arrow::Table -> JSON (列式)
// ============================================================================
//
// 输出形态 (列式, root = obj):
//   {
//     "<col1>": [v, v, ...],
//     "<col2>": [v, v, ...],
//     ...
//   }
//
// 类型映射 (覆盖 api.md schema 中出现的所有 arrow 类型):
//   timestamp[ns]   -> "YYYYMMDD" 字符串 (UTC; NaT/null -> null)
//   string          -> 原始字符串 (null -> null)
//   double / float  -> 数值 (NaN / Inf / null -> null)
//   int8/16/32/64   -> 整数 (null -> null)
//   uint8/16/32/64  -> 整数 (null -> null)
//   bool            -> bool (null -> null)
//   null (NA 列)    -> 元素全 null
//   其余类型        -> assert (未在 api.md schema 中出现)
//
// 行数一致性: 每个数组长度 = t->num_rows().
//
// 所有权: 返回 yyjson_mut_doc*, caller 调用 yyjson_mut_doc_free 释放.
//   建议序列化用 yyjson_mut_write(doc, ...).
yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t);

} // namespace bigquant
