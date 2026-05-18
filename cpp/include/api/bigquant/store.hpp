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
// 三条出口路径:
//   - Static (basic_info) / Snapshot (static_data) → write_meta (DAI 单段响应直写 _meta)
//   - Partition+Day / Where / MonthFirst           → write_by_visible_date (per-day day file)
//   - Partition+Day 且 emit_meta=true (axis 源)    → 上一行落 day file + 末尾 aggregate_meta
//
// 段切分调用方直走 misc::plan_day_segments / plan_month_segments.
// ============================================================================
namespace bigquant::store {

// _meta 单文件全量刷新 (Static / Snapshot 表; DAI 一次响应直写, 不依赖 day file).
//   行式 JSON 直接覆盖 data/_meta/<name>.json.
//   Static  : 整表全量 (e.g. basic_info).
//   Snapshot: 仅最新一天的全市场快照 (e.g. static_data).
void write_meta(const std::shared_ptr<arrow::Table> &t, const TableSpec &spec);

// Partition / Where × Day / MonthFirst → 按 visible_date 分桶, 每天写盘 (与 tushare::store::write_by_visible_date 对仗)
//   - 同次响应内 PK upsert: 同 PK 多条 → assert fail-fast (服务端 PIT 应已 dedup)
//   - 有数据 → 行式 JSON 写 day file (整文件覆盖); _empty 中清除该 DD
//   - 无数据 → 不写 day file; 该月 _empty 加入该 DD
//   - 维护 _empty.json: [start, end] 范围内全 days
//   - tmp+rename 原子写
//
// MonthFirst 调用方按月调度, visible_date 落在该月最早日; store 不区分 freq.
void write_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                           const TableSpec &spec, std::string_view start,
                           std::string_view end);

// emit_meta 表收尾: 扫 data/<Y>/<M>/<D>/<spec.name>.json 全部 day file (按日期升序),
// 把所有 records 拼成一个行式数组, atomic 写到 data/_meta/<spec.name>.json.
//   - spec.emit_meta 必须为 true (axis 源专用)
//   - day file 是真实源, _meta 是聚合产物; pipeline 每轮无条件调用一次
//   - 单文件输出供 feature/axis.cpp 直读 (D 轴 / holidays).
void aggregate_meta(const TableSpec &spec);

} // namespace bigquant::store
