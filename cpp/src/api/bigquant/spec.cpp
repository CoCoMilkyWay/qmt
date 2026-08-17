#include "api/bigquant/spec.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace bigquant {

// ============================================================================
// SPECS — 顺序严格对齐 doc/bigquant/used/api.md 自上而下.
// 落盘 = 服务端响应原样 (行结构 / 去重语义信任服务端 PIT, 与 archive 同源同构).
// ============================================================================
const std::vector<TableSpec> SPECS = {
    // axis 源 (trading_days/holidays): 普通月度表, axis.cpp 直接扫月 parquet 读 D 轴.
    {"trading_days", "date", FetchKind::Partition, FetchFreq::Day},
    {"holidays", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_instruments", "date", FetchKind::Partition, FetchFreq::Day},
    // industry_component: 月初快照 (低频), 月内变动靠 industry_change 增量 cover.
    {"cn_stock_industry_component", "date", FetchKind::Partition, FetchFreq::MonthFirst},
    // industry_change: 行业进出事件 (pit.cpp 仅取 change_flag=1 进入新行业一侧).
    {"cn_stock_industry_change", "date", FetchKind::Partition, FetchFreq::Day},
    // industry_real_bar1d: 行业不复权日行情 (instrument 列实际是 industry_code).
    {"cn_stock_industry_real_bar1d", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_industry_valuation", "date", FetchKind::Partition, FetchFreq::Day},
    // basic_info: 静态全市场快照 (Static, 无 date 列, 走 _meta 单文件).
    {"cn_stock_basic_info", "", FetchKind::Static, FetchFreq::Day},
    {"cn_stock_capital", "publish_date", FetchKind::Where, FetchFreq::Day},
    {"cn_stock_dividend", "publish_date", FetchKind::Where, FetchFreq::Day},
    {"cn_stock_allotment", "publish_date", FetchKind::Where, FetchFreq::Day},
    {"cn_stock_margin_trading_detail", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_margin_trading_market", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_shareholder", "publish_date", FetchKind::Where, FetchFreq::Day},
    {"cn_stock_shares", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_status", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_suspend", "date", FetchKind::Partition, FetchFreq::Day},
    // name_change: visible=end_date (简称失效日, 此后才确知本段区间).
    {"cn_stock_name_change", "end_date", FetchKind::Where, FetchFreq::Day},
    {"cn_stock_dragon_list", "date", FetchKind::Partition, FetchFreq::Day},
    // real_bar1d: 股票不复权日行情 (项目统一走未复权).
    {"cn_stock_real_bar1d", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_limit_price", "date", FetchKind::Partition, FetchFreq::Day},
    // static_data: 真盘前 09:00 全市场快照. Snapshot kind → 只取 [s,e] 内最新一天
    //   (MAX(date)) 一份, 落 data/_meta/cn_stock_static_data.parquet 单文件;
    //   pit overlay 阶段填 row=last_d, 给实盘当日提供真盘前可见数据.
    {"cn_stock_static_data", "date", FetchKind::Snapshot, FetchFreq::Day},
    {"cn_stock_financial_income_general_pit", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_financial_cashflow_general_pit", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_financial_balance_general_pit", "date", FetchKind::Partition, FetchFreq::Day},
    // shift 字段定位某 (date, instrument, report_date) 的偏移序列, 各 shift 独立行.
    {"cn_stock_financial_ttm_shift", "date", FetchKind::Partition, FetchFreq::Day},
    {"cn_stock_financial_notes_shift", "date", FetchKind::Partition, FetchFreq::Day},
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

  if (spec.kind == FetchKind::Snapshot) {
    // Snapshot: 取窗口内最新一天的全量行 (假日则顺延前; 与 MonthFirst MIN 对仗).
    //   start 仅决定服务端分区扫描下界, 不影响 MAX(<vd>) 结果, 也不影响配额
    //   (配额按返回 cell 数计, 与扫描窗口无关) — 故窗口原样透传.
    assert(spec.freq == FetchFreq::Day && "Snapshot 当前仅支持 FetchFreq::Day");
    std::string sql = "SELECT * FROM " + spec.name + " WHERE " +
                      spec.visible_date + " = (SELECT MAX(" +
                      spec.visible_date + ") FROM " + spec.name + " WHERE " +
                      spec.visible_date + " >= '" + ds + "' AND " +
                      spec.visible_date + " <= '" + de + "')";
    return client.query(sql, {{"date", {ds, de}}});
  }

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
