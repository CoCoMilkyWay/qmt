#pragma once

#include "package/yyjson/yyjson.h"
#include "api/tushare/spec.hpp"

#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// Tushare 行式 JSON 落盘:
//   - 路径 / _empty.json / lastupdate 通用语义委托给 misc::store
//   - 本模块只负责 "yyjson 行式响应 → data/.../<name>.json" 这一段
// ============================================================================
namespace tushare::store {

// scan_missing: 透传 misc::store::scan_missing_days (spec.name 作 key).
std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days);

// 把 fetch 回来的 (fields, items) 按 visible_date 分桶, 每天写盘
//   - has data → 写 day file (PK upsert + drop_fields 剥离); _empty 中清除该 DD
//   - no data  → 不写 day file; 该月 _empty 加入该 DD
//   - tmp+rename 原子写
void write_by_visible_date(yyjson_val *fields_arr, yyjson_val *items_arr,
                           const InterfaceSpec &spec, const FetchTask &task);

} // namespace tushare::store
