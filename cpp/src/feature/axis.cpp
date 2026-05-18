#include "feature/axis.hpp"

#include "config.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

// ============================================================================
// axis.cpp 是「Phase 0 axes 单点」: 由 _meta 单文件读出 D 轴 / A 轴 + 静态 meta.
//   D 轴源: data/_meta/trading_days.json (BigQuant emit_meta 表, 每轮 update 末尾
//           从 data/<Y>/<M>/<D>/trading_days.json 全部 day file 聚合产生)
//           过滤 market_code == "CN" 的 date 升序去重.
//   A 轴源: data/_meta/cn_stock_basic_info.json (BigQuant Static, 全市场含退市)
//           取 instrument 字段升序去重.
//   meta:   同上 cn_stock_basic_info.json, 取 name / list_date / delist_date /
//           list_sector / exchange.
// ============================================================================

namespace feature {

namespace fs = std::filesystem;

int Axes::floor_date(std::string_view d) const {
  // dates 升序; 二分找最大 i 使 dates[i] <= d
  auto it = std::upper_bound(
      dates.begin(), dates.end(), d,
      [](std::string_view a, const std::string &b) { return a < b; });
  if (it == dates.begin()) return -1;
  return static_cast<int>(std::distance(dates.begin(), it - 1));
}

namespace {

inline std::string get_str(yyjson_val *obj, const char *key) {
  yyjson_val *v = yyjson_obj_get(obj, key);
  if (!v || !yyjson_is_str(v)) return {};
  const char *s = yyjson_get_str(v);
  return s ? std::string(s) : std::string();
}

inline int8_t get_int8(yyjson_val *obj, const char *key) {
  yyjson_val *v = yyjson_obj_get(obj, key);
  if (!v || !yyjson_is_int(v)) return 0;
  int64_t x = yyjson_get_int(v);
  return static_cast<int8_t>(x);
}

} // namespace

Axes load_axes() {
  Axes a;

  // ---- D 轴: data/_meta/trading_days.json filter market_code='CN' ----
  fs::path td_path = misc::git_root() / "data" / "_meta" / "trading_days.json";
  std::string td_buf = misc::read_file_all(td_path);
  assert(!td_buf.empty() && "data/_meta/trading_days.json missing — 先跑 bigquant::update");
  yyjson_doc *td_doc = yyjson_read(td_buf.data(), td_buf.size(), 0);
  assert(td_doc);
  yyjson_val *td_root = yyjson_doc_get_root(td_doc);
  assert(yyjson_is_arr(td_root));

  std::set<std::string> dates_set;
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(td_root, i, n, item) {
    yyjson_val *mc = yyjson_obj_get(item, "market_code");
    if (!mc || !yyjson_is_str(mc)) continue;
    if (std::strcmp(yyjson_get_str(mc), "CN") != 0) continue;
    yyjson_val *dt = yyjson_obj_get(item, "date");
    if (!dt || !yyjson_is_str(dt)) continue;
    const char *s = yyjson_get_str(dt);
    if (!s || std::strlen(s) != 8) continue;
    dates_set.emplace(s);
  }
  yyjson_doc_free(td_doc);
  assert(!dates_set.empty());

  a.dates.assign(dates_set.begin(), dates_set.end());
  a.date_days.reserve(a.dates.size());
  a.date_idx.reserve(a.dates.size() * 2);
  for (size_t i = 0; i < a.dates.size(); ++i) {
    a.date_days.push_back(misc::parse_yyyymmdd(a.dates[i]));
    a.date_idx.emplace(a.dates[i], static_cast<int>(i));
  }

  // ---- A 轴: data/_meta/cn_stock_basic_info.json 全量 instrument ----
  fs::path bi_path = misc::git_root() / "data" / "_meta" / "cn_stock_basic_info.json";
  std::string bi_buf = misc::read_file_all(bi_path);
  assert(!bi_buf.empty() && "data/_meta/cn_stock_basic_info.json missing — 先跑 bigquant::update");
  yyjson_doc *bi_doc = yyjson_read(bi_buf.data(), bi_buf.size(), 0);
  assert(bi_doc);
  yyjson_val *bi_root = yyjson_doc_get_root(bi_doc);
  assert(yyjson_is_arr(bi_root));

  // PIPELINE_START_DATE 之前已退市的标的不入 A 轴 — 它们在整个 build window 内
  //   无 day file 可拉, 永远 NaN, 是纯冗员 (~81 只, 占 axis 1.4%). 下游 pool / tradable
  //   已用 delist_age 兜过, 这里只是 axis 级别清理, 让 describe / mcap_raw 等
  //   统计口径不掺这 81 只全 NaN 行.
  std::set<std::string> codes_set;
  yyjson_arr_foreach(bi_root, i, n, item) {
    yyjson_val *ins = yyjson_obj_get(item, "instrument");
    if (!ins || !yyjson_is_str(ins)) continue;
    const char *s = yyjson_get_str(ins);
    if (!s || !*s) continue;
    yyjson_val *dd_v = yyjson_obj_get(item, "delist_date");
    if (dd_v && yyjson_is_str(dd_v)) {
      const char *dd = yyjson_get_str(dd_v);
      if (dd && std::strlen(dd) == 8 &&
          std::strcmp(dd, ::config::PIPELINE_START_DATE) < 0)
        continue;
    }
    codes_set.emplace(s);
  }
  yyjson_doc_free(bi_doc);
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

  fs::path bi_path = misc::git_root() / "data" / "_meta" / "cn_stock_basic_info.json";
  std::string bi_buf = misc::read_file_all(bi_path);
  assert(!bi_buf.empty());
  yyjson_doc *bi_doc = yyjson_read(bi_buf.data(), bi_buf.size(), 0);
  assert(bi_doc);
  yyjson_val *bi_root = yyjson_doc_get_root(bi_doc);
  assert(yyjson_is_arr(bi_root));

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(bi_root, i, n, item) {
    std::string ins = get_str(item, "instrument");
    auto it = ax.code_idx.find(ins);
    if (it == ax.code_idx.end()) continue;
    int a = it->second;
    m.name[a] = get_str(item, "name");
    m.list_date[a] = get_str(item, "list_date");
    m.delist_date[a] = get_str(item, "delist_date");
    m.list_sector[a] = get_int8(item, "list_sector");
    m.exchange[a] = get_str(item, "exchange");
  }
  yyjson_doc_free(bi_doc);

  return m;
}

} // namespace feature
