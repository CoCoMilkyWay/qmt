#include "misc/store.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <utility>

namespace misc::store {

namespace fs = std::filesystem;

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

void write_day_docs(std::string_view name, std::string_view start,
                    std::string_view end,
                    std::map<std::string, yyjson_mut_doc *> docs) {
  for (auto &[vd, doc] : docs) {
    atomic_write_json(day_data_path(vd, name), doc);
    yyjson_mut_doc_free(doc);
  }
  update_empty_for_range(name, start, end,
                         [&](const std::string &d) { return docs.count(d) > 0; });
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

bool should_skip_api(std::string_view name, int window_seconds,
                     const fs::path &verify_exists) {
  assert(window_seconds >= 0);
  // verify 路径非空且不存在 → 输出已丢失, 强制重抓 (lastupdate 不可信).
  if (!verify_exists.empty() && !fs::exists(verify_exists))
    return false;
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

} // namespace misc::store
