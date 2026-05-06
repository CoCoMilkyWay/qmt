#include "tushare/store.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tushare::store {

namespace {

fs::path find_qmt_root() {
  fs::path p = fs::current_path();
  for (;;) {
    if (fs::exists(p / ".git") && fs::exists(p / "data")) return p;
    if (p == p.parent_path()) break;
    p = p.parent_path();
  }
  // 退回上一层尝试 (cwd 本身可能就是项目根)
  assert(false && "QMT root not found (need .git + data/)");
  return {};
}

std::string read_file_all(const fs::path &path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void atomic_write(const fs::path &path, const char *data, size_t len) {
  fs::create_directories(path.parent_path());
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(data, static_cast<std::streamsize>(len));
  }
  fs::rename(tmp, path);
}

std::string make_pk_key_from_obj(yyjson_val *obj,
                                 const std::vector<std::string> &pk_fields) {
  std::string key;
  for (auto &f : pk_fields) {
    yyjson_val *v = yyjson_obj_get(obj, f.c_str());
    if (v && yyjson_is_str(v)) key += yyjson_get_str(v);
    key += '|';
  }
  return key;
}

std::string make_pk_key_from_arr(yyjson_val *item,
                                 const std::vector<int> &pk_idxs) {
  std::string key;
  for (int idx : pk_idxs) {
    yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(idx));
    if (v && yyjson_is_str(v)) key += yyjson_get_str(v);
    key += '|';
  }
  return key;
}

void write_day(const InterfaceSpec &spec, const std::string &day,
               const std::vector<std::string> &field_names,
               const std::vector<int> &pk_idxs,
               const std::vector<yyjson_val *> *items_today) {
  fs::path path = data_path(day, spec.name);

  yyjson_mut_doc *mut_doc = yyjson_mut_doc_new(nullptr);

  std::vector<std::pair<std::string, yyjson_mut_val *>> records;
  std::unordered_map<std::string, size_t> pk_to_idx;

  auto upsert = [&](std::string &&pk, yyjson_mut_val *obj) {
    auto it = pk_to_idx.find(pk);
    if (it == pk_to_idx.end()) {
      pk_to_idx.emplace(pk, records.size());
      records.emplace_back(std::move(pk), obj);
    } else {
      records[it->second].second = obj;
    }
  };

  // ---- Existing file ----
  yyjson_doc *old_doc = nullptr;
  std::string old_buf;
  if (fs::exists(path)) {
    old_buf = read_file_all(path);
    old_doc = yyjson_read(old_buf.data(), old_buf.size(), 0);
    assert(old_doc);
    yyjson_val *old_root = yyjson_doc_get_root(old_doc);
    assert(yyjson_is_arr(old_root));
    size_t i, n;
    yyjson_val *obj;
    yyjson_arr_foreach(old_root, i, n, obj) {
      yyjson_mut_val *mut_obj = yyjson_val_mut_copy(mut_doc, obj);
      assert(mut_obj);
      upsert(make_pk_key_from_obj(obj, spec.pk), mut_obj);
    }
  }

  // ---- New records (array → object by field_names) ----
  if (items_today) {
    for (yyjson_val *item : *items_today) {
      yyjson_mut_val *obj = yyjson_mut_obj(mut_doc);
      for (size_t k = 0; k < field_names.size(); k++) {
        yyjson_val *v = yyjson_arr_get(item, k);
        yyjson_mut_val *mv = yyjson_val_mut_copy(mut_doc, v);
        // field_names[k].c_str() 在本函数生命周期内稳定，覆盖 mut_doc 序列化
        yyjson_mut_obj_add_val(mut_doc, obj, field_names[k].c_str(), mv);
      }
      upsert(make_pk_key_from_arr(item, pk_idxs), obj);
    }
  }

  // ---- Build root array ----
  yyjson_mut_val *mut_root = yyjson_mut_arr(mut_doc);
  yyjson_mut_doc_set_root(mut_doc, mut_root);
  for (auto &[k, v] : records) {
    yyjson_mut_arr_append(mut_root, v);
  }

  // ---- Serialize + atomic write ----
  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(mut_doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  atomic_write(path, json_str, out_len);
  std::free(json_str);

  yyjson_mut_doc_free(mut_doc);
  if (old_doc) yyjson_doc_free(old_doc);
}

} // namespace

fs::path qmt_root() {
  static fs::path root = find_qmt_root();
  return root;
}

fs::path data_path(std::string_view yyyymmdd, std::string_view name) {
  assert(yyyymmdd.size() == 8);
  return qmt_root() / "data" / std::string(yyyymmdd.substr(0, 4)) /
         std::string(yyyymmdd.substr(4, 2)) /
         std::string(yyyymmdd.substr(6, 2)) /
         (std::string(name) + ".json");
}

std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end) {
  auto days = iter_days(start, end);
  std::vector<std::string> missing;
  missing.reserve(days.size());
  for (auto &d : days) {
    if (!fs::exists(data_path(d, spec.name))) missing.push_back(d);
  }
  return missing;
}

void write_by_visible_date(yyjson_val *fields_arr, yyjson_val *items_arr,
                           const InterfaceSpec &spec, const FetchTask &task) {
  // ---- field_names, vd_idx, pk_idxs ----
  std::vector<std::string> field_names;
  size_t fn = yyjson_arr_size(fields_arr);
  field_names.reserve(fn);
  for (size_t i = 0; i < fn; i++) {
    yyjson_val *v = yyjson_arr_get(fields_arr, i);
    assert(yyjson_is_str(v));
    field_names.emplace_back(yyjson_get_str(v));
  }

  auto find_idx = [&](const std::string &f) -> int {
    for (size_t i = 0; i < field_names.size(); i++) {
      if (field_names[i] == f) return static_cast<int>(i);
    }
    return -1;
  };

  int vd_idx = find_idx(spec.visible_date_field);
  assert(vd_idx >= 0);

  std::vector<int> pk_idxs;
  pk_idxs.reserve(spec.pk.size());
  for (auto &p : spec.pk) {
    int idx = find_idx(p);
    assert(idx >= 0);
    pk_idxs.push_back(idx);
  }

  // ---- Bucket items by visible_date (drop null vd / out-of-range) ----
  std::unordered_map<std::string, std::vector<yyjson_val *>> by_day;
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(items_arr, i, n, item) {
    yyjson_val *vd_val = yyjson_arr_get(item, static_cast<size_t>(vd_idx));
    if (!vd_val || !yyjson_is_str(vd_val)) continue;
    std::string vd = yyjson_get_str(vd_val);
    if (vd < task.start || vd > task.end) continue;
    by_day[vd].push_back(item);
  }

  // ---- Write every day in [task.start, task.end] (empty → []) ----
  auto days = iter_days(task.start, task.end);
  for (auto &d : days) {
    auto it = by_day.find(d);
    const std::vector<yyjson_val *> *items_today =
        (it == by_day.end()) ? nullptr : &it->second;
    write_day(spec, d, field_names, pk_idxs, items_today);
  }
}

} // namespace tushare::store
