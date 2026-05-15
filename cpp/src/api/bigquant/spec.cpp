#include "api/bigquant/spec.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace bigquant {

// ============================================================================
// SPECS — 顺序严格对齐 doc/bigquant/used/api.md 自上而下
// ============================================================================
// PK 推断依据 doc/bigquant/used/schema.md (字段全检); 同 PK 同 day file 多条 → store 层 assert.
const std::vector<TableSpec> SPECS = {
    // -------------------- 通用数据 --------------------
    {"trading_days", "date", FetchKind::Partition, FetchFreq::Day, Category::General, {"date", "market_code"}},
    {"holidays", "date", FetchKind::Partition, FetchFreq::Day, Category::General, {"date", "market_code"}},
    {"cn_stock_instruments", "date", FetchKind::Partition, FetchFreq::Day, Category::General, {"date", "instrument"}},
    // -------------------- 行业板块-行业信息 --------------------
    // industry_component: 月初快照 (低频), 月内变动靠 industry_change 增量 cover.
    // industry∈{sw2021, sw2014, cs} 三套并存, 故 PK 含 industry.
    {"cn_stock_industry_component", "date", FetchKind::Partition, FetchFreq::MonthFirst, Category::IndustryInfo, {"date", "instrument", "industry"}},
    // industry_change: 行业进出事件; industry_level∈{1,2,3} 多级并存; change_flag∈{0进,1出} 同日同股同分类可双条.
    {"cn_stock_industry_change", "date", FetchKind::Partition, FetchFreq::Day, Category::IndustryInfo, {"date", "instrument", "industry", "industry_level", "change_flag"}},
    // -------------------- 行业板块-行业行情 --------------------
    // industry_bar1d: instrument 列实际是 industry_code; method 是计算方式 (算术/总股本/流通加权).
    {"cn_stock_industry_bar1d", "date", FetchKind::Partition, FetchFreq::Day, Category::IndustryQuote, {"date", "instrument", "method"}},
    {"cn_stock_industry_valuation", "date", FetchKind::Partition, FetchFreq::Day, Category::IndustryQuote, {"date", "instrument", "industry", "industry_level"}},
    // -------------------- 股票数据-股票信息 --------------------
    // basic_info: 静态全市场快照 (Static, 无 date 列, 不入 per-day, 走 _meta).
    {"cn_stock_basic_info", "", FetchKind::Static, FetchFreq::Day, Category::StockInfo, {"instrument"}},
    // capital: 同 publish_date 可能多个 change_date (表内 PK).
    {"cn_stock_capital", "publish_date", FetchKind::Where, FetchFreq::Day, Category::StockInfo, {"instrument", "publish_date", "change_date"}},
    // dividend: 同 publish_date 一公司一报告期一次实施.
    {"cn_stock_dividend", "publish_date", FetchKind::Where, FetchFreq::Day, Category::StockInfo, {"instrument", "publish_date", "report_date"}},
    {"cn_stock_allotment", "publish_date", FetchKind::Where, FetchFreq::Day, Category::StockInfo, {"instrument", "publish_date"}},
    {"cn_stock_margin_trading_detail", "date", FetchKind::Partition, FetchFreq::Day, Category::StockInfo, {"date", "instrument"}},
    // margin_trading_market: 全市场聚合, method 是统计口径 (e.g. 沪市/深市/全市场).
    {"cn_stock_margin_trading_market", "date", FetchKind::Partition, FetchFreq::Day, Category::StockInfo, {"date", "method"}},
    // shareholder: 一公司一公告日一报告期一条.
    {"cn_stock_shareholder", "publish_date", FetchKind::Where, FetchFreq::Day, Category::StockInfo, {"instrument", "publish_date", "end_date"}},
    {"cn_stock_shares", "date", FetchKind::Partition, FetchFreq::Day, Category::StockInfo, {"date", "instrument"}},
    {"cn_stock_status", "date", FetchKind::Partition, FetchFreq::Day, Category::StockInfo, {"date", "instrument"}},
    {"cn_stock_suspend", "date", FetchKind::Partition, FetchFreq::Day, Category::StockInfo, {"date", "instrument"}},
    // name_change: visible=end_date (简称失效日, 此后才确知本段区间).
    {"cn_stock_name_change", "end_date", FetchKind::Where, FetchFreq::Day, Category::StockInfo, {"instrument", "start_date", "end_date"}},
    // -------------------- 股票数据-股票行情 --------------------
    // dragon_list: 同 date 同股可多条 (不同上榜原因).
    {"cn_stock_dragon_list", "date", FetchKind::Partition, FetchFreq::Day, Category::StockQuote, {"date", "instrument", "reason"}},
    {"cn_stock_bar1d", "date", FetchKind::Partition, FetchFreq::Day, Category::StockQuote, {"date", "instrument"}},
    {"cn_stock_limit_price", "date", FetchKind::Partition, FetchFreq::Day, Category::StockQuote, {"date", "instrument"}},
    // -------------------- 财务数据-原始数据 (PIT) --------------------
    // 同 (date, instrument, report_date) 通常单条 (服务端 PIT 已合并到最新 change_type).
    {"cn_stock_financial_income_general_pit", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw, {"date", "instrument", "report_date"}},
    {"cn_stock_financial_cashflow_general_pit", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw, {"date", "instrument", "report_date"}},
    {"cn_stock_financial_balance_general_pit", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialRaw, {"date", "instrument", "report_date"}},
    // -------------------- 财务数据-衍生数据 --------------------
    // shift 字段定位某 (date, instrument, report_date) 的偏移序列, 各 shift 独立行.
    {"cn_stock_financial_ttm_shift", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialDerive, {"date", "instrument", "report_date", "shift"}},
    {"cn_stock_financial_notes_shift", "date", FetchKind::Partition, FetchFreq::Day, Category::FinancialDerive, {"date", "instrument", "report_date", "shift"}},
};

const TableSpec &spec_of(std::string_view name) {
  for (const auto &s : SPECS) {
    if (s.name == name)
      return s;
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
