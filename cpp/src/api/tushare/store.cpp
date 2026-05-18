#include "api/tushare/store.hpp"

#include "misc/store.hpp"

#include <cassert>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tushare::store {

namespace {

// 同次响应 PK key: 按 pk_idxs 顺序取字符串 + '|' 分隔 (与 bigquant 同语义)
std::string make_pk_key(yyjson_val *item, const std::vector<int> &pk_idxs) {
  std::string key;
  for (int idx : pk_idxs) {
    yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(idx));
    if (v && yyjson_is_str(v))
      key += yyjson_get_str(v);
    key += '|';
  }
  return key;
}

// items_today → 行式 JSON doc (按 field_names 还原为 obj; drop_idxs 内字段不写盘).
// 同次响应内 PK upsert: 同 PK 多条 → 末条胜 (服务端 PIT 通常已合并, 容错处理).
yyjson_mut_doc *build_day_doc(const std::vector<std::string> &field_names,
                              const std::vector<int> &pk_idxs,
                              const std::unordered_set<int> &drop_idxs,
                              const std::vector<yyjson_val *> &items_today) {
  assert(!items_today.empty());

  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  std::vector<yyjson_mut_val *> records;
  std::unordered_map<std::string, size_t> pk_to_idx;

  for (yyjson_val *item : items_today) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    for (size_t k = 0; k < field_names.size(); ++k) {
      if (drop_idxs.count(static_cast<int>(k)))
        continue;
      yyjson_val *v = yyjson_arr_get(item, k);
      yyjson_mut_val *mv = yyjson_val_mut_copy(doc, v);
      yyjson_mut_obj_add_val(doc, obj, field_names[k].c_str(), mv);
    }
    std::string pk = make_pk_key(item, pk_idxs);
    auto it = pk_to_idx.find(pk);
    if (it == pk_to_idx.end()) {
      pk_to_idx.emplace(std::move(pk), records.size());
      records.push_back(obj);
    } else {
      records[it->second] = obj;
    }
  }

  yyjson_mut_val *root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);
  for (yyjson_mut_val *r : records)
    yyjson_mut_arr_append(root, r);
  return doc;
}

// 解 envelope 后 fields → index 表; 找不到必填字段直接 assert.
int find_idx(const std::vector<std::string> &field_names,
             const std::string &name) {
  for (size_t i = 0; i < field_names.size(); ++i)
    if (field_names[i] == name)
      return static_cast<int>(i);
  return -1;
}

} // namespace

void write_by_visible_date(yyjson_doc *doc, const InterfaceSpec &spec,
                           std::string_view start, std::string_view end) {
  // ---- 解 envelope: root.data.fields / root.data.items ----
  yyjson_val *root = yyjson_doc_get_root(doc);
  yyjson_val *data = yyjson_obj_get(root, "data");
  assert(data && "tushare response missing 'data'");
  yyjson_val *fields_arr = yyjson_obj_get(data, "fields");
  yyjson_val *items_arr = yyjson_obj_get(data, "items");
  assert(fields_arr && items_arr && yyjson_is_arr(fields_arr) &&
         yyjson_is_arr(items_arr));

  // ---- 解析 fields → vd_idxs / pk_idxs / drop_idxs ----
  std::vector<std::string> field_names;
  size_t fn = yyjson_arr_size(fields_arr);
  field_names.reserve(fn);
  for (size_t i = 0; i < fn; ++i) {
    yyjson_val *v = yyjson_arr_get(fields_arr, i);
    assert(yyjson_is_str(v));
    field_names.emplace_back(yyjson_get_str(v));
  }

  std::vector<int> vd_idxs;
  vd_idxs.reserve(spec.visible_date_fields.size());
  for (auto &f : spec.visible_date_fields) {
    int idx = find_idx(field_names, f);
    assert(idx >= 0);
    vd_idxs.push_back(idx);
  }
  assert(!vd_idxs.empty());

  std::vector<int> pk_idxs;
  pk_idxs.reserve(spec.pk.size());
  for (auto &p : spec.pk) {
    int idx = find_idx(field_names, p);
    assert(idx >= 0);
    pk_idxs.push_back(idx);
  }

  std::unordered_set<int> drop_idxs;
  for (auto &f : spec.drop_fields) {
    int idx = find_idx(field_names, f);
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
    if (vd < start || vd > end)
      continue;
    by_day[vd].push_back(item);
  }

  // ---- 构 docs (vd → mut_doc); 尾段 (写 day file + _empty) 共用 misc::store ----
  std::map<std::string, yyjson_mut_doc *> docs;
  for (auto &[d, items_today] : by_day) {
    if (items_today.empty())
      continue;
    docs.emplace(d, build_day_doc(field_names, pk_idxs, drop_idxs, items_today));
  }
  misc::store::write_day_docs(spec.name, start, end, std::move(docs));
}

} // namespace tushare::store
