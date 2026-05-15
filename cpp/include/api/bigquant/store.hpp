#pragma once

#include "api/bigquant/spec.hpp"

#include <arrow/table.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// BigQuant 列式 JSON 落盘:
//   - 路径 / _empty.json / lastupdate 通用语义委托给 misc::store
//   - 本模块负责 "arrow::Table → data/.../<name>.json (列式)" 这一段
//
// Static 表 (basic_info / industry_index_mapper / ...) 走 _meta 单文件全量;
// 其余 (Partition / Where) 按 visible_date 切日, 维护 _empty.json 三态.
// ============================================================================
namespace bigquant::store {

// scan_missing: 透传 misc::store 对应入口 (Day / MonthFirst 自动按 spec.freq 分发)
std::vector<std::string> scan_missing(const TableSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days);

// Static 表 → data/_meta/<name>.json (列式 JSON 直接覆盖刷新)
void write_meta_table(const std::shared_ptr<arrow::Table> &t,
                      const TableSpec &spec);

// Partition / Where × Day → 按 visible_date 分桶, 每天写盘
//   - 同次响应内 PK upsert: 同 PK 多条 → assert fail-fast (服务端 PIT 应已 dedup)
//   - 有数据 → 列式 JSON 写 day file; _empty 中清除该 DD
//   - 无数据 → 不写 day file; 该月 _empty 加入该 DD
//   - 维护 _empty.json: [start, end] 范围内全 days
//   - tmp+rename 原子写
void write_table_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                                 const TableSpec &spec,
                                 std::string_view start,
                                 std::string_view end);

// Partition + MonthFirst → 行内 visible_date 实为该月最早日 (DAI 返回单一日期值);
// 同样按 visible_date 切日落地 (该月仅 1 个 DD 子目录有文件), 与 Day 路径共享逻辑.
// 调用方负责按月调度; store 不区分 Day/MonthFirst, 统一走 write_table_by_visible_date.

} // namespace bigquant::store
