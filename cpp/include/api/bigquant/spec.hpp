#pragma once

#include "api/bigquant/dai.hpp"

#include <arrow/table.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// TableSpec — api.md 中"需要支持"的 26 张表的元描述
// 与 doc/bigquant/fetch.py::TABLES 同构, 是 DAI 查询的最小调度单元.
// ============================================================================

// 抓取策略 — 决定 SQL 写法和 filters 是否启用.
//   Static    : 无 date 维度, 全量 SELECT *.        (basic_info)
//   Partition : date 列是分区列, 也是 visible_date. 用 filters={"date":[s,e]}
//               做服务端分区裁剪, 性能最佳.
//   Where     : visible_date 不是分区列. 用 SQL WHERE 过滤
//               (publish_date / end_date 等事件列).
//   Snapshot  : 有 date 列, 但仅取 [s,e] 内 MAX(<vd>) 一天的全量快照, 落
//               data/_meta/<name>.json 单文件 (像 Static 一样不走 day file).
//               用于"真盘前"表 (cn_stock_static_data 09:00) 给 PIT overlay
//               最后一天 row D 用. 与 MonthFirst 对仗 (MIN vs MAX).
enum class FetchKind { Static, Partition, Where, Snapshot };

// 抓取频率 — 当前仅一张表 (industry_component) 用 MonthFirst.
//   Day        : 在 [start, end] 闭区间内全量拉.
//   MonthFirst : 在 [start, end] 内最早一天的全量行 (月度快照).
enum class FetchFreq { Day, MonthFirst };

struct TableSpec {
  std::string name;         // DAI 表名 = SQL FROM 子句
  std::string visible_date; // 因果安全可见日列名; Static 为空 ""
  FetchKind kind;
  FetchFreq freq;
  // 同次响应去重 PK (字段名列表). 同一 visible_date day file 内, 同 PK 多条 → assert
  // (BigQuant PIT 服务端通常保证单次响应 PK 唯一, 此处 fail-fast 兜底).
  // Static 表 PK = 表唯一身份键 (e.g. instrument); 其他表通常含 (visible_date, instrument).
  std::vector<std::string> pk;
  // true ⇒ 该表是 axis 源 (trading_days / holidays), 走普通 Partition+Day day file 落盘
  // 路径, 但 pipeline 每轮末尾额外把所有 day file 聚合成 data/_meta/<name>.json 单文件,
  // 供 axis.cpp 一次性读出 D 轴. day file 由 dedup + lookback + 缺失扫描兜底完整性;
  // _meta 只是聚合产物, 每轮无条件重建 (廉价: 小表, 几千行). Static 表 (无 date 列) 不
  // 走 day file, 自有 write_meta 单文件全量整刷路径, emit_meta 必须 false.
  bool emit_meta = false;
};

// api.md 中"需要支持"的全部表 (26 张), 顺序与 api.md 自上而下一致.
extern const std::vector<TableSpec> SPECS;

// ============================================================================
// fetch — 自动按 (kind, freq) 选 SQL 模板; 一步式 DAI 查询入口.
//   Static                : "SELECT * FROM <name>",  filters={}, start/end 忽略
//   Partition + Day       : "SELECT * FROM <name>",  filters={"date":[start,end]}
//   Partition + MonthFirst: WHERE <vd>=(SELECT MIN(<vd>) ... WHERE <vd> BETWEEN s AND e),
//                           filters={"date":[start,end]}
//   Where + Day           : "SELECT * FROM <name> WHERE <vd>>=start AND <vd><=end",
//                           filters={}
//   Snapshot              : WHERE <vd>=(SELECT MAX(<vd>) ... WHERE <vd> BETWEEN s AND e),
//                           filters={"date":[start,end]}
// 日期格式: "YYYYMMDD" (内部 dash 成 DAI 接受的 "YYYY-MM-DD").
// ============================================================================
std::shared_ptr<arrow::Table> fetch(DaiClient &client, const TableSpec &spec,
                                    std::string_view start,
                                    std::string_view end);

} // namespace bigquant
