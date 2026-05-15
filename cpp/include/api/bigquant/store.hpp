#pragma once

#include "api/bigquant/spec.hpp"

#include <arrow/table.h>

#include <memory>
#include <string_view>

// ============================================================================
// BigQuant 行式 JSON 落盘:
//   - 路径 / _empty.json / lastupdate 通用语义委托给 misc::store
//   - 本模块负责 "arrow::Table → data/.../<name>.json (行式)" 这一段
//
// Static 表 (basic_info / ...) 走 _meta 单文件全量;
// 其余 (Partition / Where, Day / MonthFirst) 按 visible_date 切日, 维护 _empty.json 三态.
// 缺失日扫描调用方直走 misc::store::scan_missing_days / scan_missing_months.
// ============================================================================
namespace bigquant::store {

// Static 表 → data/_meta/<name>.json (行式 JSON 直接覆盖刷新)
void write_meta_table(const std::shared_ptr<arrow::Table> &t,
                      const TableSpec &spec);

// Partition / Where × Day / MonthFirst → 按 visible_date 分桶, 每天写盘
//   - 同次响应内 PK upsert: 同 PK 多条 → assert fail-fast (服务端 PIT 应已 dedup)
//   - 有数据 → 行式 JSON 写 day file; _empty 中清除该 DD
//   - 无数据 → 不写 day file; 该月 _empty 加入该 DD
//   - 维护 _empty.json: [start, end] 范围内全 days
//   - tmp+rename 原子写
//
// MonthFirst 调用方按月调度, visible_date 落在该月最早日; store 不区分 freq.
void write_table_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                                 const TableSpec &spec,
                                 std::string_view start,
                                 std::string_view end);

} // namespace bigquant::store
