#include "api/tushare/store.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tushare::store {

namespace fs = std::filesystem;

namespace {

std::string make_pk_key_from_obj(yyjson_val *obj,
                                 const std::vector<std::string> &pk_fields) {
  std::string key;
  for (auto &f : pk_fields) {
    yyjson_val *v = yyjson_obj_get(obj, f.c_str());
    if (v && yyjson_is_str(v))
      key += yyjson_get_str(v);
    key += '|';
  }
  return key;
}

std::string make_pk_key_from_arr(yyjson_val *item,
                                 const std::vector<int> &pk_idxs) {
  std::string key;
  for (int idx : pk_idxs) {
    yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(idx));
    if (v && yyjson_is_str(v))
      key += yyjson_get_str(v);
    key += '|';
  }
  return key;
}

// 写单日 day file (PK upsert + drop_fields 剥离)
//   - 老记录: 加载后按 drop_fields 剥离 (兜底 lookback 外 PK 残留未来字段)
//   - 新记录: lookback 内同 PK 覆盖老记录
//   - 行式输出: [{k:v,...},...] PRETTY_TWO_SPACES
void write_day(const InterfaceSpec &spec, const std::string &day,
               const std::vector<std::string> &field_names,
               const std::vector<int> &pk_idxs,
               const std::unordered_set<int> &drop_idxs,
               const std::vector<yyjson_val *> &items_today) {
  assert(!items_today.empty());
  fs::path path = misc::store::day_data_path(day, spec.name);

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

  // 老文件
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
      for (auto &f : spec.drop_fields) {
        yyjson_mut_obj_remove_str(mut_obj, f.c_str());
      }
      upsert(make_pk_key_from_obj(obj, spec.pk), mut_obj);
    }
  }

  // 新记录 (array → object by field_names; drop_idxs 内字段不写盘)
  for (yyjson_val *item : items_today) {
    yyjson_mut_val *obj = yyjson_mut_obj(mut_doc);
    for (size_t k = 0; k < field_names.size(); k++) {
      if (drop_idxs.count(static_cast<int>(k)))
        continue;
      yyjson_val *v = yyjson_arr_get(item, k);
      yyjson_mut_val *mv = yyjson_val_mut_copy(mut_doc, v);
      yyjson_mut_obj_add_val(mut_doc, obj, field_names[k].c_str(), mv);
    }
    upsert(make_pk_key_from_arr(item, pk_idxs), obj);
  }

  yyjson_mut_val *mut_root = yyjson_mut_arr(mut_doc);
  yyjson_mut_doc_set_root(mut_doc, mut_root);
  for (auto &[k, v] : records) {
    yyjson_mut_arr_append(mut_root, v);
  }

  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(mut_doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  misc::atomic_write(path, json_str, out_len);
  std::free(json_str);

  yyjson_mut_doc_free(mut_doc);
  if (old_doc)
    yyjson_doc_free(old_doc);
}

} // namespace

std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days) {
  return misc::store::scan_missing_days(spec.name, start, end, lookback_days);
}

void write_by_visible_date(yyjson_val *fields_arr, yyjson_val *items_arr,
                           const InterfaceSpec &spec, const FetchTask &task) {
  // ---- 解析 fields → vd_idxs / pk_idxs / drop_idxs ----
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
      if (field_names[i] == f)
        return static_cast<int>(i);
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

  std::unordered_set<int> drop_idxs;
  for (auto &f : spec.drop_fields) {
    int idx = find_idx(f);
    if (idx >= 0)
      drop_idxs.insert(idx);
  }

  // ---- 按 visible_date 分桶 (drop null vd / out-of-range) ----
  std::unordered_map<std::string, std::vector<yyjson_val *>> by_day;
  size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(items_arr, i, n, item) {
    std::string vd;
    for (int vd_idx : vd_idxs) {
      yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(vd_idx));
      if (!v || !yyjson_is_str(v))
        continue;
      const char *s = yyjson_get_str(v);
      if (!s || s[0] == '\0')
        continue;
      vd = s;
      break;
    }
    if (vd.empty())
      continue;
    if (vd < task.start || vd > task.end)
      continue;
    by_day[vd].push_back(item);
  }

  // ---- 写 day file + 维护 _empty.json ----
  // (yyyy_mm → 最终 EmptyMonth) — 末端统一 flush
  std::unordered_map<std::string, misc::store::EmptyMonth> dirty_months;
  auto days = misc::iter_days(task.start, task.end);
  for (auto &d : days) {
    auto it = by_day.find(d);
    bool has_data = (it != by_day.end()) && !it->second.empty();
    if (has_data) {
      write_day(spec, d, field_names, pk_idxs, drop_idxs, it->second);
    }
    bool day_exists =
        has_data || fs::exists(misc::store::day_data_path(d, spec.name));
    std::string yyyy = d.substr(0, 4);
    std::string mm = d.substr(4, 2);
    std::string dd = d.substr(6, 2);
    std::string ym = yyyy + mm;
    auto mit = dirty_months.find(ym);
    if (mit == dirty_months.end()) {
      mit = dirty_months
                .emplace(ym, misc::store::read_empty_month(yyyy, mm))
                .first;
    }
    misc::store::EmptySet &set = mit->second[spec.name];
    if (day_exists)
      set.erase(dd);
    else
      set.insert(dd);
  }

  for (auto &[ym, month] : dirty_months) {
    misc::store::write_empty_month(ym.substr(0, 4), ym.substr(4, 2), month);
  }
}

} // namespace tushare::store
