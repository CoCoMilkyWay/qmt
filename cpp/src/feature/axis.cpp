#include "feature/axis.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

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

// 扫 data/YYYY/MM/DD/calendar.json 全量, 收集 SSE/SZSE 且 is_open=1 的 cal_date
std::set<std::string> scan_trading_dates() {
  std::set<std::string> out;
  fs::path data_root = misc::git_root() / "data";
  assert(fs::exists(data_root));

  for (auto &y_ent : fs::directory_iterator(data_root)) {
    if (!y_ent.is_directory()) continue;
    std::string y = y_ent.path().filename().string();
    if (y.size() != 4 || !std::isdigit(static_cast<unsigned char>(y[0]))) continue;

    for (auto &m_ent : fs::directory_iterator(y_ent.path())) {
      if (!m_ent.is_directory()) continue;
      std::string m = m_ent.path().filename().string();
      if (m.size() != 2) continue;

      for (auto &d_ent : fs::directory_iterator(m_ent.path())) {
        if (!d_ent.is_directory()) continue;
        std::string dd = d_ent.path().filename().string();
        if (dd.size() != 2) continue;

        fs::path cal_file = d_ent.path() / "calendar.json";
        if (!fs::exists(cal_file)) continue;

        std::string buf = misc::read_file_all(cal_file);
        if (buf.empty()) continue;
        yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
        assert(doc);
        yyjson_val *root = yyjson_doc_get_root(doc);
        assert(yyjson_is_arr(root));

        size_t i, n;
        yyjson_val *item;
        yyjson_arr_foreach(root, i, n, item) {
          yyjson_val *exch = yyjson_obj_get(item, "exchange");
          yyjson_val *cal = yyjson_obj_get(item, "cal_date");
          yyjson_val *iso = yyjson_obj_get(item, "is_open");
          if (!exch || !cal || !iso) continue;
          if (!yyjson_is_str(exch) || !yyjson_is_str(cal)) continue;
          const char *ex = yyjson_get_str(exch);
          if (std::strcmp(ex, "SSE") != 0 && std::strcmp(ex, "SZSE") != 0)
            continue;
          if (!yyjson_is_int(iso) || yyjson_get_int(iso) != 1) continue;
          out.emplace(yyjson_get_str(cal));
        }
        yyjson_doc_free(doc);
      }
    }
  }
  return out;
}

} // namespace

Axes load_axes() {
  Axes a;

  // ---- D 轴: 走盘所有 calendar.json 收集 SSE∪SZSE 且 is_open=1 ----
  std::set<std::string> dates_set = scan_trading_dates();
  assert(!dates_set.empty());
  a.dates.assign(dates_set.begin(), dates_set.end());

  a.date_days.reserve(a.dates.size());
  a.date_idx.reserve(a.dates.size() * 2);
  for (size_t i = 0; i < a.dates.size(); ++i) {
    a.date_days.push_back(misc::parse_yyyymmdd(a.dates[i]));
    a.date_idx.emplace(a.dates[i], static_cast<int>(i));
  }

  // ---- A 轴: _meta/stock_basic.json 全量 ts_code ----
  fs::path sb_path = misc::git_root() / "data" / "_meta" / "stock_basic.json";
  std::string buf = misc::read_file_all(sb_path);
  assert(!buf.empty());
  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_arr(root));

  std::set<std::string> codes_set;
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    yyjson_val *tc = yyjson_obj_get(item, "ts_code");
    assert(tc && yyjson_is_str(tc));
    codes_set.emplace(yyjson_get_str(tc));
  }
  yyjson_doc_free(doc);
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
  m.market.assign(na, {});
  m.exchange.assign(na, {});
  m.industry_l1.assign(na, {});
  m.name_history.assign(na, {});

  auto get_str = [](yyjson_val *obj, const char *key) -> std::string {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!v || !yyjson_is_str(v)) return {};
    const char *s = yyjson_get_str(v);
    return s ? std::string(s) : std::string();
  };

  // ---- stock_basic: list_date / delist_date / market / exchange ----
  fs::path sb_path = misc::git_root() / "data" / "_meta" / "stock_basic.json";
  std::string buf = misc::read_file_all(sb_path);
  assert(!buf.empty());
  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_arr(root));

  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    std::string ts_code = get_str(item, "ts_code");
    auto it = ax.code_idx.find(ts_code);
    if (it == ax.code_idx.end()) continue;
    int a = it->second;
    m.name[a] = get_str(item, "name");
    m.list_date[a] = get_str(item, "list_date");
    m.delist_date[a] = get_str(item, "delist_date");
    m.market[a] = get_str(item, "market");
    m.exchange[a] = get_str(item, "exchange");
  }
  yyjson_doc_free(doc);

  // ---- index_member_all: industry_l1 (申万 SW2021 L1 中文名) ----
  fs::path im_path = misc::git_root() / "data" / "_meta" / "index_member_all.json";
  std::string im_buf = misc::read_file_all(im_path);
  assert(!im_buf.empty());
  yyjson_doc *im_doc = yyjson_read(im_buf.data(), im_buf.size(), 0);
  assert(im_doc);
  yyjson_val *im_root = yyjson_doc_get_root(im_doc);
  assert(yyjson_is_arr(im_root));

  size_t im_i, im_n;
  yyjson_val *im_item;
  yyjson_arr_foreach(im_root, im_i, im_n, im_item) {
    std::string ts_code = get_str(im_item, "ts_code");
    auto it = ax.code_idx.find(ts_code);
    if (it == ax.code_idx.end()) continue;
    int a = it->second;
    m.industry_l1[a] = get_str(im_item, "l1_name");
  }
  yyjson_doc_free(im_doc);

  // ---- namechange: per-A 改名时间线 (按 start_date 升序) ----
  //   _meta/namechange.json 格式: {ts_code: [{name, start_date, ann_date, change_reason}, ...]}
  //   refresh_namechange_meta 已保证内层数组按 start_date 升序.
  fs::path nc_path = misc::git_root() / "data" / "_meta" / "namechange.json";
  std::string nc_buf = misc::read_file_all(nc_path);
  assert(!nc_buf.empty());
  yyjson_doc *nc_doc = yyjson_read(nc_buf.data(), nc_buf.size(), 0);
  assert(nc_doc);
  yyjson_val *nc_root = yyjson_doc_get_root(nc_doc);
  assert(yyjson_is_obj(nc_root));

  yyjson_obj_iter nc_iter;
  yyjson_obj_iter_init(nc_root, &nc_iter);
  yyjson_val *nc_key;
  while ((nc_key = yyjson_obj_iter_next(&nc_iter)) != nullptr) {
    const char *ts_code_cstr = yyjson_get_str(nc_key);
    if (!ts_code_cstr) continue;
    auto it = ax.code_idx.find(ts_code_cstr);
    if (it == ax.code_idx.end()) continue;
    int a = it->second;
    yyjson_val *arr = yyjson_obj_iter_get_val(nc_key);
    if (!arr || !yyjson_is_arr(arr)) continue;

    size_t ai, an;
    yyjson_val *rec;
    yyjson_arr_foreach(arr, ai, an, rec) {
      NameChange nc;
      nc.start_date = get_str(rec, "start_date");
      nc.name = get_str(rec, "name");
      if (nc.start_date.size() != 8 || nc.name.empty()) continue;
      m.name_history[a].push_back(std::move(nc));
    }
  }
  yyjson_doc_free(nc_doc);

  return m;
}

} // namespace feature
