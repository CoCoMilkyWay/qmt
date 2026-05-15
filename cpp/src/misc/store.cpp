#include "misc/store.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <utility>

namespace misc::store {

namespace fs = std::filesystem;
using std::chrono::days;
using std::chrono::sys_days;
using std::chrono::year;
using std::chrono::year_month;
using std::chrono::year_month_day;
using std::chrono::month;
using std::chrono::last;

// ============================================================================
// 路径
// ============================================================================

fs::path day_data_path(std::string_view yyyymmdd, std::string_view name) {
  assert(yyyymmdd.size() == 8);
  return git_root() / "data" / std::string(yyyymmdd.substr(0, 4)) /
         std::string(yyyymmdd.substr(4, 2)) /
         std::string(yyyymmdd.substr(6, 2)) /
         (std::string(name) + ".json");
}

fs::path meta_data_path(std::string_view name) {
  return git_root() / "data" / "_meta" / (std::string(name) + ".json");
}

fs::path lastupdate_path(std::string_view name) {
  return git_root() / "data" / "_meta" / (std::string(name) + ".lastupdate");
}

fs::path empty_month_path(std::string_view yyyy, std::string_view mm) {
  return git_root() / "data" / std::string(yyyy) / std::string(mm) /
         "_empty.json";
}

// ============================================================================
// _empty.json
// ============================================================================

EmptyMonth read_empty_month(std::string_view yyyy, std::string_view mm) {
  EmptyMonth out;
  fs::path path = empty_month_path(yyyy, mm);
  if (!fs::exists(path))
    return out;
  std::string buf = read_file_all(path);
  if (buf.empty())
    return out;
  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_obj(root));
  size_t i, n;
  yyjson_val *key, *arr;
  yyjson_obj_foreach(root, i, n, key, arr) {
    assert(yyjson_is_str(key) && yyjson_is_arr(arr));
    EmptySet &set = out[yyjson_get_str(key)];
    size_t j, m;
    yyjson_val *dd_v;
    yyjson_arr_foreach(arr, j, m, dd_v) {
      assert(yyjson_is_str(dd_v));
      set.emplace(yyjson_get_str(dd_v));
    }
  }
  yyjson_doc_free(doc);
  return out;
}

void write_empty_month(std::string_view yyyy, std::string_view mm,
                       const EmptyMonth &data) {
  // 收集非空 itf, 按 name 升序输出
  std::vector<std::string> itf_names;
  itf_names.reserve(data.size());
  for (auto &[k, v] : data) {
    if (!v.empty())
      itf_names.push_back(k);
  }
  std::sort(itf_names.begin(), itf_names.end());

  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  // dds_storage 需要在 yyjson_mut_write 之前保活 (字符串非拷贝)
  std::vector<std::vector<std::string>> dds_storage;
  dds_storage.reserve(itf_names.size());
  for (auto &name : itf_names) {
    auto &set = data.at(name);
    std::vector<std::string> dds(set.begin(), set.end());
    std::sort(dds.begin(), dds.end());
    dds_storage.push_back(std::move(dds));
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    for (auto &dd : dds_storage.back()) {
      yyjson_mut_arr_add_str(doc, arr, dd.c_str());
    }
    yyjson_mut_obj_add_val(doc, root, name.c_str(), arr);
  }

  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  atomic_write(empty_month_path(yyyy, mm), json_str, out_len);
  std::free(json_str);
  yyjson_mut_doc_free(doc);
}

void update_empty_for_range(std::string_view name, std::string_view start,
                            std::string_view end,
                            const std::function<bool(const std::string &)> &has_data) {
  std::unordered_map<std::string, EmptyMonth> dirty_months;
  std::string name_s(name);
  for (auto &d : iter_days(start, end)) {
    bool data = has_data(d);
    bool day_exists = data || fs::exists(day_data_path(d, name));
    std::string yyyy = d.substr(0, 4);
    std::string mm = d.substr(4, 2);
    std::string dd = d.substr(6, 2);
    std::string ym = yyyy + mm;
    auto it = dirty_months.find(ym);
    if (it == dirty_months.end())
      it = dirty_months.emplace(ym, read_empty_month(yyyy, mm)).first;
    EmptySet &set = it->second[name_s];
    if (day_exists)
      set.erase(dd);
    else
      set.insert(dd);
  }
  for (auto &[ym, month] : dirty_months) {
    write_empty_month(ym.substr(0, 4), ym.substr(4, 2), month);
  }
}

// ============================================================================
// lastupdate 去重
// ============================================================================

bool should_skip_api(std::string_view name, int window_seconds) {
  assert(window_seconds >= 0);
  fs::path p = lastupdate_path(name);
  if (!fs::exists(p))
    return false;
  std::string buf = read_file_all(p);
  while (!buf.empty() &&
         (buf.back() == '\n' || buf.back() == '\r' || buf.back() == ' ')) {
    buf.pop_back();
  }
  assert(!buf.empty());
  int64_t last_ts = std::stoll(buf);
  int64_t now = static_cast<int64_t>(std::time(nullptr));
  return (now - last_ts) < window_seconds;
}

void mark_api_updated(std::string_view name) {
  int64_t now = static_cast<int64_t>(std::time(nullptr));
  std::string content = std::to_string(now) + "\n";
  atomic_write(lastupdate_path(name), content.data(), content.size());
}

// ============================================================================
// scan_missing_days
// ============================================================================

std::vector<std::string> scan_missing_days(std::string_view name,
                                           std::string_view start,
                                           std::string_view end,
                                           int lookback_days) {
  assert(lookback_days >= 0);
  auto all_days = iter_days(start, end);
  std::string lookback_from = lookback_days > 0
                                  ? add_days(end, -(lookback_days - 1))
                                  : std::string{};

  // 按月缓存 _empty.json 中本 itf 对应的 DD 集合 (key = "YYYYMM")
  std::unordered_map<std::string, EmptySet> empty_by_month;
  std::string name_s(name);
  auto get_empty_set = [&](const std::string &d) -> const EmptySet & {
    std::string ym = d.substr(0, 6);
    auto it = empty_by_month.find(ym);
    if (it == empty_by_month.end()) {
      EmptyMonth m = read_empty_month(d.substr(0, 4), d.substr(4, 2));
      it = empty_by_month.emplace(ym, std::move(m[name_s])).first;
    }
    return it->second;
  };

  std::vector<std::string> missing;
  missing.reserve(all_days.size());
  for (auto &d : all_days) {
    bool in_lookback = lookback_days > 0 && d >= lookback_from;
    if (in_lookback) {
      missing.push_back(d);
      continue;
    }
    if (fs::exists(day_data_path(d, name)))
      continue;
    if (get_empty_set(d).count(d.substr(6, 2)))
      continue;
    missing.push_back(d);
  }
  return missing;
}

// ============================================================================
// scan_missing_months
// ============================================================================

namespace {

// (yyyy, mm) 区间内所有 (yyyy_str, mm_str) 升序, 闭区间.
struct YM { std::string yyyy; std::string mm; };
std::vector<YM> iter_months(std::string_view start, std::string_view end) {
  assert(start.size() == 8 && end.size() == 8);
  std::vector<YM> out;
  sys_days s = parse_yyyymmdd(start);
  sys_days e = parse_yyyymmdd(end);
  year_month_day ymd_s{s};
  year_month_day ymd_e{e};
  year_month cur{ymd_s.year(), ymd_s.month()};
  year_month last_ym{ymd_e.year(), ymd_e.month()};
  while (cur <= last_ym) {
    char yy[5], mm[3];
    std::snprintf(yy, sizeof(yy), "%04d", static_cast<int>(cur.year()));
    std::snprintf(mm, sizeof(mm), "%02d", static_cast<unsigned>(cur.month()));
    out.push_back({std::string(yy, 4), std::string(mm, 2)});
    cur += std::chrono::months{1};
  }
  return out;
}

// 该月内是否存在任一 day file (data/YYYY/MM/DD/<name>.json)
bool month_has_any_day(std::string_view yyyy, std::string_view mm,
                       std::string_view name) {
  fs::path dir = git_root() / "data" / std::string(yyyy) / std::string(mm);
  if (!fs::exists(dir))
    return false;
  std::string fname = std::string(name) + ".json";
  for (auto &entry : fs::directory_iterator(dir)) {
    if (!entry.is_directory())
      continue;
    if (fs::exists(entry.path() / fname))
      return true;
  }
  return false;
}

// 该月第一天 (YYYYMMDD)
std::string month_first(std::string_view yyyy, std::string_view mm) {
  return std::string(yyyy) + std::string(mm) + "01";
}

// 该月最后一天 (YYYYMMDD)
std::string month_last(std::string_view yyyy, std::string_view mm) {
  int y = std::stoi(std::string(yyyy));
  unsigned m = static_cast<unsigned>(std::stoi(std::string(mm)));
  year_month_day ymd{year{y} / month{m} / last};
  return fmt_yyyymmdd(sys_days{ymd});
}

} // namespace

std::vector<MonthSeg> scan_missing_months(std::string_view name,
                                          std::string_view start,
                                          std::string_view end,
                                          int lookback_days) {
  assert(lookback_days >= 0);
  std::string lookback_from = lookback_days > 0
                                  ? add_days(end, -(lookback_days - 1))
                                  : std::string{};
  std::vector<MonthSeg> out;
  for (auto &ym : iter_months(start, end)) {
    std::string mfirst = month_first(ym.yyyy, ym.mm);
    std::string mlast = month_last(ym.yyyy, ym.mm);
    // clamp 到 outer [start, end]
    std::string s = std::max(mfirst, std::string(start));
    std::string e = std::min(mlast, std::string(end));

    bool in_lookback =
        lookback_days > 0 &&
        e >= lookback_from; // 该月任一天进 lookback → 重拉
    if (in_lookback) {
      out.push_back({s, e});
      continue;
    }
    if (month_has_any_day(ym.yyyy, ym.mm, name))
      continue;
    out.push_back({s, e});
  }
  return out;
}

} // namespace misc::store
