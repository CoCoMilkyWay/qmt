#include "tushare/store.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tushare::store {

namespace fs = std::filesystem;

namespace {

// ============================================================================
// _empty.json: data/YYYY/MM/_empty.json
// 反向稀疏标记：{itf_name: [DD, DD, ...]} = "该接口在这些天拉过且为空"
// 三态判定 (单源、互斥):
//   day file 存在            → 拉过有数据
//   day file 不存在 + 在 set → 拉过无数据
//   day file 不存在 + 不在 set → 未拉
// ============================================================================

using EmptySet = std::unordered_set<std::string>;        // {DD}
using EmptyMonth = std::unordered_map<std::string, EmptySet>; // {itf: {DD}}

fs::path empty_path(std::string_view yyyy, std::string_view mm) {
  return misc::git_root() / "data" / std::string(yyyy) / std::string(mm) /
         "_empty.json";
}

EmptyMonth read_empty_month(std::string_view yyyy, std::string_view mm) {
  EmptyMonth out;
  fs::path path = empty_path(yyyy, mm);
  if (!fs::exists(path)) return out;
  std::string buf = misc::read_file_all(path);
  if (buf.empty()) return out;
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
  // 排序使输出稳定 + 去除空 set 条目
  std::vector<std::string> itf_names;
  itf_names.reserve(data.size());
  for (auto &[k, v] : data) {
    if (!v.empty()) itf_names.push_back(k);
  }
  std::sort(itf_names.begin(), itf_names.end());

  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  // dds_storage 必须在 yyjson_mut_write 之前保持生命周期 (字符串非拷贝)
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
  misc::atomic_write(empty_path(yyyy, mm), json_str, out_len);
  std::free(json_str);
  yyjson_mut_doc_free(doc);
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
               const std::vector<yyjson_val *> &items_today) {
  assert(!items_today.empty());
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
    old_buf = misc::read_file_all(path);
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
  for (yyjson_val *item : items_today) {
    yyjson_mut_val *obj = yyjson_mut_obj(mut_doc);
    for (size_t k = 0; k < field_names.size(); k++) {
      yyjson_val *v = yyjson_arr_get(item, k);
      yyjson_mut_val *mv = yyjson_val_mut_copy(mut_doc, v);
      // field_names[k].c_str() 在本函数生命周期内稳定，覆盖 mut_doc 序列化
      yyjson_mut_obj_add_val(mut_doc, obj, field_names[k].c_str(), mv);
    }
    upsert(make_pk_key_from_arr(item, pk_idxs), obj);
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
  misc::atomic_write(path, json_str, out_len);
  std::free(json_str);

  yyjson_mut_doc_free(mut_doc);
  if (old_doc) yyjson_doc_free(old_doc);
}

} // namespace

fs::path data_path(std::string_view yyyymmdd, std::string_view name) {
  assert(yyyymmdd.size() == 8);
  return misc::git_root() / "data" / std::string(yyyymmdd.substr(0, 4)) /
         std::string(yyyymmdd.substr(4, 2)) /
         std::string(yyyymmdd.substr(6, 2)) /
         (std::string(name) + ".json");
}

std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days) {
  assert(lookback_days >= 0);
  auto days = misc::iter_days(start, end);
  // 进入 lookback 窗口的最早日期 (闭区间)：max(start, end - lookback_days + 1)
  std::string lookback_from =
      lookback_days > 0 ? misc::add_days(end, -(lookback_days - 1))
                        : std::string{};

  // 按月缓存 _empty.json 中 spec 对应的 DD 集合 (key = "YYYYMM")
  std::unordered_map<std::string, EmptySet> empty_by_month;
  auto get_empty_set = [&](const std::string &d) -> const EmptySet & {
    std::string ym = d.substr(0, 6);
    auto it = empty_by_month.find(ym);
    if (it == empty_by_month.end()) {
      EmptyMonth m = read_empty_month(d.substr(0, 4), d.substr(4, 2));
      it = empty_by_month.emplace(ym, std::move(m[spec.name])).first;
    }
    return it->second;
  };

  std::vector<std::string> missing;
  missing.reserve(days.size());
  for (auto &d : days) {
    bool in_lookback = lookback_days > 0 && d >= lookback_from;
    if (in_lookback) {
      missing.push_back(d);
      continue;
    }
    if (fs::exists(data_path(d, spec.name))) continue;
    if (get_empty_set(d).count(d.substr(6, 2))) continue;
    missing.push_back(d);
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

  std::vector<int> vd_idxs;
  vd_idxs.reserve(spec.visible_date_fields.size());
  for (auto &f : spec.visible_date_fields) {
    int idx = find_idx(f);
    assert(idx >= 0);
    vd_idxs.push_back(idx);
  }
  assert(!vd_idxs.empty());

  std::vector<int> pk_idxs;
  pk_idxs.reserve(spec.pk.size());
  for (auto &p : spec.pk) {
    int idx = find_idx(p);
    assert(idx >= 0);
    pk_idxs.push_back(idx);
  }

  // ---- Bucket items by visible_date (drop null vd / out-of-range) ----
  // visible_date：按 visible_date_fields 顺序找第一个 "非 null 非空字符串" 字段值
  std::unordered_map<std::string, std::vector<yyjson_val *>> by_day;
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(items_arr, i, n, item) {
    std::string vd;
    for (int vd_idx : vd_idxs) {
      yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(vd_idx));
      if (!v || !yyjson_is_str(v)) continue;
      const char *s = yyjson_get_str(v);
      if (!s || s[0] == '\0') continue;
      vd = s;
      break;
    }
    if (vd.empty()) continue;
    if (vd < task.start || vd > task.end) continue;
    by_day[vd].push_back(item);
  }

  // ---- Write every day in [task.start, task.end] ----
  // 有数据 → 写 day file；无数据 → 不写文件
  // 空/非空状态变更先收集，最后按月统一写 _empty.json
  // (yyyy_mm → 该月需要写入的最终 EmptyMonth 状态)
  // 注意：dividend 同一天有两个 task (ann_date / imp_ann_date)，
  // 第二个 task 在同一天可能为空但 day file 已存在 → 用 fs::exists 二次确认状态
  std::unordered_map<std::string, EmptyMonth> dirty_months;
  auto days = misc::iter_days(task.start, task.end);
  for (auto &d : days) {
    auto it = by_day.find(d);
    bool has_data = (it != by_day.end()) && !it->second.empty();
    if (has_data) {
      write_day(spec, d, field_names, pk_idxs, it->second);
    }
    // 二次确认 day file 状态 (上一行 write_day 之后必然 exists)
    bool day_exists = has_data || fs::exists(data_path(d, spec.name));
    std::string yyyy = d.substr(0, 4);
    std::string mm = d.substr(4, 2);
    std::string dd = d.substr(6, 2);
    std::string ym = yyyy + mm;
    auto mit = dirty_months.find(ym);
    if (mit == dirty_months.end()) {
      mit = dirty_months.emplace(ym, read_empty_month(yyyy, mm)).first;
    }
    EmptySet &set = mit->second[spec.name];
    if (day_exists) set.erase(dd);
    else set.insert(dd);
  }

  for (auto &[ym, month] : dirty_months) {
    write_empty_month(ym.substr(0, 4), ym.substr(4, 2), month);
  }
}

} // namespace tushare::store
