#include "api/bigquant/spec.hpp"

#include "config.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

namespace bigquant {

// ============================================================================
// SPECS — 顺序严格对齐 doc/bigquant/used/api.md 自上而下
// PK 推断依据 doc/bigquant/used/schema.md; 同 PK 同 day file 多条 → store 层 assert.
// ============================================================================
const std::vector<TableSpec> SPECS = {
    // axis 源: 走 Partition+Day day file 正常落盘; pipeline 末尾把全部 day file 聚合到
    // data/_meta/<name>.json 供 axis.cpp 单文件直读. 因此 day file 完整时无需 API 重抓.
    {"trading_days", "date", FetchKind::Partition, FetchFreq::Day, {"date", "market_code"}, /*emit_meta=*/true},
    {"holidays", "date", FetchKind::Partition, FetchFreq::Day, {"date", "market_code"}, /*emit_meta=*/true},
    {"cn_stock_instruments", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    // industry_component: 月初快照 (低频), 月内变动靠 industry_change 增量 cover.
    // industry∈{sw2021, sw2014, cs} 三套并存, 故 PK 含 industry.
    {"cn_stock_industry_component", "date", FetchKind::Partition, FetchFreq::MonthFirst, {"date", "instrument", "industry"}},
    // industry_change: 行业进出事件; industry_level∈{1,2,3} 多级并存; change_flag∈{0进,1出} 同日同股同分类可双条.
    {"cn_stock_industry_change", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "industry", "industry_level", "change_flag"}},
    // industry_bar1d: instrument 列实际是 industry_code; method 是计算方式 (算术/总股本/流通加权).
    {"cn_stock_industry_bar1d", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "method"}},
    {"cn_stock_industry_valuation", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "industry", "industry_level"}},
    // basic_info: 静态全市场快照 (Static, 无 date 列, 不入 per-day, 走 _meta).
    {"cn_stock_basic_info", "", FetchKind::Static, FetchFreq::Day, {"instrument"}},
    // capital: 同 publish_date 可能多个 change_date (表内 PK).
    {"cn_stock_capital", "publish_date", FetchKind::Where, FetchFreq::Day, {"instrument", "publish_date", "change_date"}},
    // dividend: 同 publish_date 一公司一报告期一次实施.
    {"cn_stock_dividend", "publish_date", FetchKind::Where, FetchFreq::Day, {"instrument", "publish_date", "report_date"}},
    {"cn_stock_allotment", "publish_date", FetchKind::Where, FetchFreq::Day, {"instrument", "publish_date"}},
    {"cn_stock_margin_trading_detail", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    // margin_trading_market: 全市场聚合, method 是统计口径 (e.g. 沪市/深市/全市场).
    {"cn_stock_margin_trading_market", "date", FetchKind::Partition, FetchFreq::Day, {"date", "method"}},
    // shareholder: 一公司一公告日一报告期一条.
    {"cn_stock_shareholder", "publish_date", FetchKind::Where, FetchFreq::Day, {"instrument", "publish_date", "end_date"}},
    {"cn_stock_shares", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    {"cn_stock_status", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    {"cn_stock_suspend", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    // name_change: visible=end_date (简称失效日, 此后才确知本段区间).
    {"cn_stock_name_change", "end_date", FetchKind::Where, FetchFreq::Day, {"instrument", "start_date", "end_date"}},
    // dragon_list: 同 date 同股可多条 (不同上榜原因).
    {"cn_stock_dragon_list", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "reason"}},
    {"cn_stock_bar1d", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    {"cn_stock_limit_price", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument"}},
    // 同 (date, instrument, report_date) 通常单条 (服务端 PIT 已合并到最新 change_type).
    {"cn_stock_financial_income_general_pit", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "report_date"}},
    {"cn_stock_financial_cashflow_general_pit", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "report_date"}},
    {"cn_stock_financial_balance_general_pit", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "report_date"}},
    // shift 字段定位某 (date, instrument, report_date) 的偏移序列, 各 shift 独立行.
    {"cn_stock_financial_ttm_shift", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "report_date", "shift"}},
    {"cn_stock_financial_notes_shift", "date", FetchKind::Partition, FetchFreq::Day, {"date", "instrument", "report_date", "shift"}},
};

// ============================================================================
// fetch — 自动按 (kind, freq) 选 SQL 模板, 一步式 DAI 查询
// ============================================================================

namespace {

// "YYYYMMDD" -> "YYYY-MM-DD" (DAI 接受格式)
std::string to_dashed(std::string_view yyyymmdd) {
  assert(yyyymmdd.size() == 8);
  std::string out;
  out.reserve(10);
  out.append(yyyymmdd.data(), 4);
  out.push_back('-');
  out.append(yyyymmdd.data() + 4, 2);
  out.push_back('-');
  out.append(yyyymmdd.data() + 6, 2);
  return out;
}

// 配额护栏: pre-cutoff 日期一律拒绝走 DAI (按日刷新的额度只留给近端增量).
void assert_post_cutoff(const TableSpec &spec, const std::string &start,
                        const std::string &end) {
  if (start >= ::config::BIGQUANT_API_MIN_DATE)
    return;
  std::cerr
      << "[bigquant.spec] BLOCK pre-cutoff DAI 调用: table=" << spec.name
      << " start=" << start << " end=" << end << "\n"
      << "  BigQuant DAI API 额度有限 (按日刷新), "
      << ::config::BIGQUANT_API_MIN_DATE
      << " 之前的历史数据不得走在线接口.\n"
      << "  应使用 doc/bigquant/fetch.py 在 BigQuant AI Studio 内离线导出 parquet 压缩 archive,\n"
      << "  置于 config::BIGQUANT_DATABASE (= " << ::config::BIGQUANT_DATABASE
      << ") 后, 启用 config::BIGQUANT_IMPORT=true 整月导入." << std::endl;
  assert(false && "BigQuant DAI pre-cutoff access blocked");
}

} // namespace

std::shared_ptr<arrow::Table> fetch(DaiClient &client, const TableSpec &spec,
                                    std::string_view start,
                                    std::string_view end) {
  // ---- Static: 整表全量, 忽略 start/end ----
  if (spec.kind == FetchKind::Static) {
    assert(spec.visible_date.empty() && "Static 表不应配 visible_date");
    return client.query("SELECT * FROM " + spec.name);
  }

  // ---- 非 Static: 必须有 visible_date + 完整 [start, end] ----
  assert(!spec.visible_date.empty() && "非 Static 表必须配 visible_date");
  assert(start.size() == 8 && end.size() == 8 && "start/end 须为 YYYYMMDD");

  std::string ds = to_dashed(start);
  std::string de = to_dashed(end);
  assert_post_cutoff(spec, ds, de);

  if (spec.kind == FetchKind::Partition) {
    // 分区裁剪走 filters; Day SQL 不带 WHERE, MonthFirst 仍带 sub-select.
    std::string sql;
    if (spec.freq == FetchFreq::Day) {
      sql = "SELECT * FROM " + spec.name;
    } else {
      // MonthFirst: 取窗口内最早一天的全量行 (月初遇假期则顺延).
      sql = "SELECT * FROM " + spec.name + " WHERE " + spec.visible_date +
            " = (SELECT MIN(" + spec.visible_date + ") FROM " + spec.name +
            " WHERE " + spec.visible_date + " >= '" + ds + "' AND " +
            spec.visible_date + " <= '" + de + "')";
    }
    return client.query(sql, {{"date", {ds, de}}});
  }

  assert(spec.kind == FetchKind::Where);
  assert(spec.freq == FetchFreq::Day && "Where + MonthFirst 当前不支持");
  std::string sql = "SELECT * FROM " + spec.name + " WHERE " +
                    spec.visible_date + " >= '" + ds + "' AND " +
                    spec.visible_date + " <= '" + de + "'";
  return client.query(sql);
}

} // namespace bigquant
