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
//   Static    : 无 date 维度, 全量 SELECT *.
//               (basic_info, financial_changedate ...)
//   Partition : date 列是分区列, 也是 visible_date. 用 filters={"date":[s,e]}
//               做服务端分区裁剪, 性能最佳.
//   Where     : visible_date 不是分区列. 用 SQL WHERE 过滤
//               (publish_date / end_date 等事件列).
enum class FetchKind { Static, Partition, Where };

// 抓取频率 — 当前仅一张表 (industry_component) 用 MonthFirst.
//   Day        : 在 [start, end] 闭区间内全量拉.
//   MonthFirst : 在 [start, end] 内最早一天的全量行 (月度快照).
enum class FetchFreq { Day, MonthFirst };

// 分类 — 与 api.md 表头"类别"一一对应, 便于按类别过滤 / 调度.
enum class Category {
  General,         // 通用数据
  IndustryInfo,    // 行业板块-行业信息
  IndustryQuote,   // 行业板块-行业行情
  StockInfo,       // 股票数据-股票信息
  StockQuote,      // 股票数据-股票行情
  FinancialRaw,    // 财务数据-原始数据
  FinancialDerive, // 财务数据-衍生数据
};

struct TableSpec {
  std::string name;         // DAI 表名 = SQL FROM 子句
  std::string visible_date; // 因果安全可见日列名; Static 为空 ""
  FetchKind kind;
  FetchFreq freq;
  Category category;
  // 同次响应去重 PK (字段名列表). 同一 visible_date day file 内, 同 PK 多条 → assert
  // (BigQuant PIT 服务端通常保证单次响应 PK 唯一, 此处 fail-fast 兜底).
  // Static 表 PK = 表唯一身份键 (e.g. instrument); 其他表通常含 (visible_date, instrument).
  std::vector<std::string> pk;
};

// api.md 中"需要支持"的全部表 (26 张), 顺序与 api.md 自上而下一致.
extern const std::vector<TableSpec> SPECS;

// 按名查 spec; 不存在直接 assert.
const TableSpec &spec_of(std::string_view name);

// ============================================================================
// 查询调度 — 自动按 kind/freq 生成 SQL + filters
// ============================================================================

struct PreparedQuery {
  std::string sql;
  DaiFilters filters;
};

// 按 spec 构造一次性 SQL + filters.
//   Static                : "SELECT * FROM <name>",  filters={}, start/end 必须为空
//   Partition + Day       : "SELECT * FROM <name>",  filters={"date":[start,end]}
//   Partition + MonthFirst: WHERE <vd>=(SELECT MIN(<vd>) ... WHERE <vd> BETWEEN s AND e),
//                           filters={"date":[start,end]}
//   Where + Day           : "SELECT * FROM <name> WHERE <vd>>=start AND <vd><=end"
//                           filters={}
//   Where + MonthFirst    : (暂未用到, 直接 assert)
// 日期字符串格式: "YYYY-MM-DD" (DAI 接受的标准格式, 与 fetch.py 一致).
PreparedQuery prepare_query(const TableSpec &spec, std::string_view start = {},
                            std::string_view end = {});

// 一步式: prepare_query + DaiClient::query.
std::shared_ptr<arrow::Table> fetch(DaiClient &client, const TableSpec &spec,
                                    std::string_view start = {}, std::string_view end = {});

} // namespace bigquant
