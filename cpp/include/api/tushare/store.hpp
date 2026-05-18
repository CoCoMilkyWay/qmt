#pragma once

#include "api/tushare/spec.hpp"
#include "package/yyjson/yyjson.h"

#include <string_view>

// ============================================================================
// Tushare 行式 JSON 落盘:
//   - 路径 / _empty.json / lastupdate 通用语义委托给 misc::store
//   - 本模块只负责 "yyjson 响应 (envelope: data.fields + data.items) → data/.../<name>.json"
//   - 与 bigquant::store::write_by_visible_date 完全对仗 (整文件覆盖语义).
//   - 段切分调用方直走 misc::plan_day_segments.
// ============================================================================
namespace tushare::store {

// 把 Tushare 整张响应 doc (root.data.fields / root.data.items) 按 visible_date 分桶, 每天写盘
//   - has data → 写 day file (整文件覆盖, 按 PK 同次去重, drop_fields 剥离); _empty 中清除该 DD
//   - no data  → 不写 day file; 该月 _empty 加入该 DD
//   - tmp+rename 原子写
//   - 入参 doc 由 caller 释放 (本函数不 free)
void write_by_visible_date(yyjson_doc *doc, const InterfaceSpec &spec,
                           std::string_view start, std::string_view end);

} // namespace tushare::store
