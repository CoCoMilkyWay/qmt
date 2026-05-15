#include "api/bigquant/spec.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace bigquant {

// ============================================================================
// SPECS — 顺序严格对齐 doc/bigquant/used/api.md 自上而下
// ============================================================================
const std::vector<TableSpec> SPECS = {
    // -------------------- 通用数据 --------------------
    {"trading_days",                 "date",         FetchKind::Partition, FetchFreq::Day,        Category::General},
    {"holidays",                     "date",         FetchKind::Partition, FetchFreq::Day,        Category::General},
    {"cn_stock_instruments",         "date",         FetchKind::Partition, FetchFreq::Day,        Category::General},
    // -------------------- 行业板块-行业信息 --------------------
    // industry_component: 月初快照 (低频), 月内变动靠 industry_change 增量 cover.
    {"cn_stock_industry_component",  "date",         FetchKind::Partition, FetchFreq::MonthFirst, Category::IndustryInfo},
    {"cn_stock_industry_change",     "date",         FetchKind::Partition, FetchFreq::Day,        Category::IndustryInfo},
    // -------------------- 行业板块-行业行情 --------------------
    {"cn_stock_industry_bar1d",      "date",         FetchKind::Partition, FetchFreq::Day,        Category::IndustryQuote},
    {"cn_stock_industry_valuation",  "date",         FetchKind::Partition, FetchFreq::Day,        Category::IndustryQuote},
    // -------------------- 股票数据-股票信息 --------------------
    // basic_info: 静态快照, 无 date 维度, 全量取 (后续可加 date 列做版本切片, 当前不切).
    {"cn_stock_basic_info",          "",             FetchKind::Static,    FetchFreq::Day,        Category::StockInfo},
    // capital/dividend/allotment/shareholder: 事件型, visible=publish_date (非分区列, 用 WHERE).
    {"cn_stock_capital",             "publish_date", FetchKind::Where,     FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_dividend",            "publish_date", FetchKind::Where,     FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_allotment",           "publish_date", FetchKind::Where,     FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_margin_trading_detail","date",        FetchKind::Partition, FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_margin_trading_market","date",        FetchKind::Partition, FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_shareholder",         "publish_date", FetchKind::Where,     FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_shares",              "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_status",              "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockInfo},
    {"cn_stock_suspend",             "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockInfo},
    // name_change: visible=end_date (简称失效日, 此后才确知本段区间).
    {"cn_stock_name_change",         "end_date",     FetchKind::Where,     FetchFreq::Day,        Category::StockInfo},
    // -------------------- 股票数据-股票行情 --------------------
    {"cn_stock_dragon_list",         "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockQuote},
    {"cn_stock_bar1d",               "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockQuote},
    {"cn_stock_limit_price",         "date",         FetchKind::Partition, FetchFreq::Day,        Category::StockQuote},
    // -------------------- 财务数据-原始数据 (PIT) --------------------
    {"cn_stock_financial_income_general_pit",   "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw},
    {"cn_stock_financial_cashflow_general_pit", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw},
    {"cn_stock_financial_balance_general_pit",  "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw},
    // -------------------- 财务数据-衍生数据 --------------------
    {"cn_stock_financial_ttm_shift",   "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialDerive},
    {"cn_stock_financial_notes_shift", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialDerive},
};

const TableSpec &spec_of(std::string_view name) {
  for (const auto &s : SPECS) {
    if (s.name == name) return s;
  }
  assert(false && "bigquant::spec_of: unknown table name");
  __builtin_unreachable();
}

// ============================================================================
// PreparedQuery 构造 — 每种 (kind, freq) 组合一段 SQL 模板
// ============================================================================

PreparedQuery prepare_query(const TableSpec &spec, std::string_view start, std::string_view end) {
  PreparedQuery q;

  if (spec.kind == FetchKind::Static) {
    assert(spec.visible_date.empty() && "Static 表不应配 visible_date");
    assert(start.empty() && end.empty() && "Static 表不接受时间区间");
    q.sql = "SELECT * FROM " + spec.name;
    return q;
  }

  // 非 Static: 必须有 visible_date + 完整 [start, end]
  assert(!spec.visible_date.empty() && "非 Static 表必须配 visible_date");
  assert(!start.empty() && !end.empty() && "非 Static 表必须传 [start, end]");

  if (spec.kind == FetchKind::Partition) {
    // 分区裁剪走 filters; SQL 不带 WHERE.
    if (spec.freq == FetchFreq::Day) {
      q.sql = "SELECT * FROM " + spec.name;
    } else {
      // MonthFirst: 取窗口内最早一天的全量行 (月初遇假期则顺延).
      // 仍需 filters 做分区裁剪, 否则 DAI 拒绝扫全分区.
      q.sql = "SELECT * FROM " + spec.name +
              " WHERE " + spec.visible_date + " = (SELECT MIN(" + spec.visible_date +
              ") FROM " + spec.name + " WHERE " + spec.visible_date +
              " >= '" + std::string(start) + "' AND " + spec.visible_date +
              " <= '" + std::string(end) + "')";
    }
    q.filters["date"] = {std::string(start), std::string(end)};
    return q;
  }

  assert(spec.kind == FetchKind::Where);
  assert(spec.freq == FetchFreq::Day && "Where + MonthFirst 当前不支持");
  q.sql = "SELECT * FROM " + spec.name + " WHERE " + spec.visible_date +
          " >= '" + std::string(start) + "' AND " + spec.visible_date +
          " <= '" + std::string(end) + "'";
  return q;
}

std::shared_ptr<arrow::Table> fetch(DaiClient &client, const TableSpec &spec,
                                    std::string_view start, std::string_view end) {
  PreparedQuery q = prepare_query(spec, start, end);
  return client.query(q.sql, q.filters);
}

} // namespace bigquant
