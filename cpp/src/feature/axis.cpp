#include "feature/axis.hpp"

#include "config.hpp"
#include "misc/date.hpp"
#include "misc/parquet.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>

// ============================================================================
// axis.cpp 是「Phase 0 axes 单点」: 从 parquet 读出 D 轴 / A 轴 + 静态 meta.
//   D 轴源: data/YYYY-MM/trading_days.parquet 全月扫描 (小表, 每月 KB 级),
//           过滤 market_code == "CN" 的 date 升序去重.
//   A 轴源: data/_meta/cn_stock_basic_info.parquet (BigQuant Static, 全市场含退市)
//           取 instrument 升序去重.
//   meta:   同上 basic_info, 取 name / list_date / delist_date / list_sector / exchange.
// ============================================================================

namespace feature {

int Axes::floor_date(std::string_view d) const {
  // dates 升序; 二分找最大 i 使 dates[i] <= d
  auto it = std::upper_bound(
      dates.begin(), dates.end(), d,
      [](std::string_view a, const std::string &b) { return a < b; });
  if (it == dates.begin()) return -1;
  return static_cast<int>(std::distance(dates.begin(), it - 1));
}

namespace {

// yyyymmdd int32 → "YYYYMMDD"; 0 (缺失) → 空串.
std::string ymd_str(std::int32_t v) {
  if (v <= 0) return {};
  char buf[9];
  std::snprintf(buf, sizeof(buf), "%08d", v);
  return std::string(buf, 8);
}

} // namespace

Axes load_axes() {
  Axes a;

  // ---- D 轴: 扫全部 trading_days 月 parquet, filter market_code='CN' ----
  auto td_files = misc::pq::list_month_files("trading_days");
  assert(!td_files.empty() &&
         "data/YYYY-MM/trading_days.parquet missing — 先跑 bigquant::update");

  std::set<std::string> dates_set;
  for (auto &[ym, path] : td_files) {
    misc::pq::TableView v(misc::pq::read_table(path));
    if (v.rows() == 0) continue;
    misc::pq::Col date = v.col("date");
    misc::pq::Col mc = v.col("market_code");
    for (std::int64_t i = 0, n = v.rows(); i < n; ++i) {
      if (mc.str(i) != "CN") continue;
      std::string d = ymd_str(date.yyyymmdd(i));
      if (d.empty()) continue;
      dates_set.insert(std::move(d));
    }
  }
  assert(!dates_set.empty());

  a.dates.assign(dates_set.begin(), dates_set.end());
  a.date_days.reserve(a.dates.size());
  a.date_idx.reserve(a.dates.size() * 2);
  for (size_t i = 0; i < a.dates.size(); ++i) {
    a.date_days.push_back(misc::parse_yyyymmdd(a.dates[i]));
    a.date_idx.emplace(a.dates[i], static_cast<int>(i));
  }

  // ---- A 轴: _meta/cn_stock_basic_info.parquet 全量 instrument ----
  auto bi_path = misc::pq::meta_path("cn_stock_basic_info");
  misc::pq::TableView bi(misc::pq::read_table(bi_path));
  assert(bi.rows() > 0 &&
         "data/_meta/cn_stock_basic_info.parquet missing — 先跑 bigquant::update");

  // PIPELINE_START_DATE 之前已退市的标的不入 A 轴 — 它们在整个 build window 内
  //   无数据可落, 永远 NaN, 是纯冗员. 下游 pool / tradable 已用 delist_age 兜过,
  //   这里只是 axis 级别清理, 让 describe / mcap_raw 等统计口径不掺全 NaN 行.
  misc::pq::Col ins = bi.col("instrument");
  misc::pq::Col dd = bi.col("delist_date");
  std::set<std::string> codes_set;
  for (std::int64_t i = 0, n = bi.rows(); i < n; ++i) {
    std::string_view s = ins.str(i);
    if (s.empty()) continue;
    std::string d = ymd_str(dd.yyyymmdd(i));
    if (!d.empty() && d < ::config::PIPELINE_START_DATE) continue;
    codes_set.emplace(s);
  }
  assert(!codes_set.empty());

  a.codes.assign(codes_set.begin(), codes_set.end());
  a.code_idx.reserve(a.codes.size() * 2);
  for (size_t i = 0; i < a.codes.size(); ++i) {
    a.code_idx.emplace(a.codes[i], static_cast<int>(i));
  }

  return a;
}

StockMeta load_stock_meta(const Axes &ax) {
  StockMeta m;
  size_t na = static_cast<size_t>(ax.n_a());
  m.name.assign(na, {});
  m.list_date.assign(na, {});
  m.delist_date.assign(na, {});
  m.list_sector.assign(na, int8_t{0});
  m.exchange.assign(na, {});

  misc::pq::TableView bi(
      misc::pq::read_table(misc::pq::meta_path("cn_stock_basic_info")));
  assert(bi.rows() > 0);

  misc::pq::Col ins = bi.col("instrument");
  misc::pq::Col name = bi.col("name");
  misc::pq::Col ld = bi.col("list_date");
  misc::pq::Col dd = bi.col("delist_date");
  misc::pq::Col ls = bi.col("list_sector");
  misc::pq::Col ex = bi.col("exchange");

  for (std::int64_t i = 0, n = bi.rows(); i < n; ++i) {
    auto it = ax.code_idx.find(std::string(ins.str(i)));
    if (it == ax.code_idx.end()) continue;
    int a = it->second;
    m.name[a] = std::string(name.str(i));
    m.list_date[a] = ymd_str(ld.yyyymmdd(i));
    m.delist_date[a] = ymd_str(dd.yyyymmdd(i));
    m.list_sector[a] = static_cast<int8_t>(ls.i32(i, 0));
    m.exchange[a] = std::string(ex.str(i));
  }

  return m;
}

} // namespace feature
