#pragma once

#include "api/bigquant/spec.hpp"
#include "package/yyjson/yyjson.h"

#include <arrow/table.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// arrow::Table -> JSON (行式, 人眼可读)
// ============================================================================
//
// 输出形态 (行式, root = arr):
//   [ {"<col1>": v0, ...}, {"<col1>": v1, ...}, ... ]
//
// 行数 = t->num_rows(); 每行一个 obj 含 t->num_columns() 个 key-value.
// 类型映射:
//   timestamp[ns]   -> "YYYYMMDD" 字符串 (UTC; null -> null)
//   string          -> 原始字符串 (null -> null)
//   double / float  -> 数值 (NaN / Inf / null -> null)
//   int / uint      -> 整数 (null -> null)
//   bool            -> bool (null -> null)
//   null (NA 列)    -> null
//   其余类型        -> assert (未在 api.md schema 中出现)
//
// 所有权: 返回 yyjson_mut_doc*, caller 调用 yyjson_mut_doc_free 释放.
yyjson_mut_doc *table_to_json(const std::shared_ptr<arrow::Table> &t);

// 同 table_to_json, 但仅包含 row_idxs 指定的行 (顺序 = row_idxs 顺序).
// row_idxs 必须 < t->num_rows(). 用于 store 按 visible_date 分桶后逐 day 输出.
yyjson_mut_doc *table_subset_to_json(const std::shared_ptr<arrow::Table> &t,
                                     const std::vector<int64_t> &row_idxs);

// ============================================================================
// 按 spec.visible_date 分桶 [start, end] 闭区间内的行, 同次响应 PK upsert
// ============================================================================
//
// 行为:
//   - visible_date 列 null / 非 8 字符 → 跳过该行
//   - vd ∉ [start, end] → 跳过该行
//   - 同 vd 同 PK 多条 → assert (BigQuant PIT 服务端通常已 dedup, fail-fast 暴露异常)
//
// 输出:
//   map<vd_yyyymmdd, 升序 row_idxs> — vd 升序遍历; 调用方按 vd 取 sub-table 输出.
std::map<std::string, std::vector<int64_t>>
bucket_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                       const TableSpec &spec, std::string_view start,
                       std::string_view end);

} // namespace bigquant
