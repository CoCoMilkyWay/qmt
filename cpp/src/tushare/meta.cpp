#include "tushare/meta.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"
#include "tushare/http.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tushare {

namespace fs = std::filesystem;

namespace {

fs::path stock_basic_path() {
  return misc::git_root() / "data" / "_meta" / "stock_basic.json";
}

fs::path index_member_all_path() {
  return misc::git_root() / "data" / "_meta" / "index_member_all.json";
}

fs::path lastupdate_path(std::string_view name) {
  return misc::git_root() / "data" / "_meta" /
         (std::string(name) + ".lastupdate");
}

} // namespace

void refresh_stock_basic(Http &http) {
  // tushare stock_basic 默认 list_status=L；要覆盖退市/暂停/过会未交易需分别拉
  // L 在市 / D 退市 / P 暂停上市 / G 过会未交易
  static constexpr std::array<const char *, 4> STATUSES = {"L", "D", "P", "G"};

  // 显式锁全字段：默认 fields="" 只返回"默认显示=Y"的 10 列，
  // 这里枚举 doc 的 17 列 (Y+N)，含 fullname/enname/exchange/curr_type/
  // list_status/delist_date/is_hs。顺序即返回 fields 顺序。
  static constexpr const char *ALL_FIELDS =
      "ts_code,symbol,name,area,industry,fullname,enname,cnspell,market,"
      "exchange,curr_type,list_status,list_date,delist_date,is_hs,act_name,"
      "act_ent_type";

  std::cout << "\n[stock_basic] refresh meta ..." << std::flush;

  yyjson_mut_doc *out_doc = yyjson_mut_doc_new(nullptr);

  // (ts_code, mut_obj) 列表 + ts_code 去重集合；最后按 ts_code 排序写出，稳定 diff
  std::vector<std::pair<std::string, yyjson_mut_val *>> records;
  std::unordered_map<std::string, size_t> seen;

  // field_names 必须在 yyjson_mut_write 之前保持生命周期 (key 非拷贝)
  std::vector<std::string> field_names;

  for (const char *status : STATUSES) {
    yyjson_doc *doc = http.call(
        "stock_basic", {{"list_status", std::string(status)}}, ALL_FIELDS);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *data = yyjson_obj_get(root, "data");
    assert(data);
    yyjson_val *fields_arr = yyjson_obj_get(data, "fields");
    yyjson_val *items_arr = yyjson_obj_get(data, "items");
    assert(fields_arr && items_arr);

    // 字段顺序在 4 次调用间必须一致；首轮锚定，后续 assert
    std::vector<std::string> cur_fields;
    size_t fn = yyjson_arr_size(fields_arr);
    cur_fields.reserve(fn);
    for (size_t i = 0; i < fn; i++) {
      yyjson_val *v = yyjson_arr_get(fields_arr, i);
      assert(yyjson_is_str(v));
      cur_fields.emplace_back(yyjson_get_str(v));
    }
    if (field_names.empty())
      field_names = std::move(cur_fields);
    else
      assert(field_names == cur_fields);

    int ts_idx = -1;
    for (size_t i = 0; i < field_names.size(); i++) {
      if (field_names[i] == "ts_code") {
        ts_idx = static_cast<int>(i);
        break;
      }
    }
    assert(ts_idx >= 0);

    size_t added = 0, dup = 0;
    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(items_arr, i, n, item) {
      yyjson_val *tv = yyjson_arr_get(item, static_cast<size_t>(ts_idx));
      assert(yyjson_is_str(tv));
      std::string ts_code = yyjson_get_str(tv);
      if (seen.find(ts_code) != seen.end()) {
        dup++;
        continue;
      }

      yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
      for (size_t k = 0; k < field_names.size(); k++) {
        yyjson_val *v = yyjson_arr_get(item, k);
        yyjson_mut_val *mv = yyjson_val_mut_copy(out_doc, v);
        yyjson_mut_obj_add_val(out_doc, obj, field_names[k].c_str(), mv);
      }
      seen.emplace(ts_code, records.size());
      records.emplace_back(std::move(ts_code), obj);
      added++;
    }
    yyjson_doc_free(doc);

    std::cout << " " << status << "=" << added;
    if (dup)
      std::cout << "(+" << dup << "dup)";
    std::cout << std::flush;
  }

  std::sort(records.begin(), records.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  yyjson_mut_val *out_root = yyjson_mut_arr(out_doc);
  yyjson_mut_doc_set_root(out_doc, out_root);
  for (auto &[k, v] : records) {
    yyjson_mut_arr_append(out_root, v);
  }

  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(out_doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  misc::atomic_write(stock_basic_path(), json_str, out_len);
  std::free(json_str);
  yyjson_mut_doc_free(out_doc);

  std::cout << " -> " << records.size() << " total" << std::endl;
}

void refresh_index_member_all(Http &http) {
  std::cout << "\n[index_member_all] refresh meta ..." << std::flush;

  // ---- Step 1: 拉 SW2021 L1 列表 (~31 条) ----
  // 单次 index_member_all 上限 2000 行；按 L1 分批 (每个 L1 通常 100~600 股)
  yyjson_doc *cls_doc = http.call(
      "index_classify", {{"level", "L1"}, {"src", "SW2021"}});
  yyjson_val *cls_root = yyjson_doc_get_root(cls_doc);
  yyjson_val *cls_data = yyjson_obj_get(cls_root, "data");
  assert(cls_data);
  yyjson_val *cls_fields = yyjson_obj_get(cls_data, "fields");
  yyjson_val *cls_items = yyjson_obj_get(cls_data, "items");
  assert(cls_fields && cls_items);

  int code_idx = -1;
  size_t cf_n = yyjson_arr_size(cls_fields);
  for (size_t i = 0; i < cf_n; i++) {
    yyjson_val *v = yyjson_arr_get(cls_fields, i);
    assert(yyjson_is_str(v));
    if (std::string(yyjson_get_str(v)) == "index_code") {
      code_idx = static_cast<int>(i);
      break;
    }
  }
  assert(code_idx >= 0);

  std::vector<std::string> l1_codes;
  {
    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(cls_items, i, n, item) {
      yyjson_val *v = yyjson_arr_get(item, static_cast<size_t>(code_idx));
      assert(yyjson_is_str(v));
      l1_codes.emplace_back(yyjson_get_str(v));
    }
  }
  yyjson_doc_free(cls_doc);
  assert(!l1_codes.empty());

  std::cout << " (L1=" << l1_codes.size() << ")" << std::flush;

  // ---- Step 2: 按 L1 循环拉 index_member_all(is_new=Y) ----
  yyjson_mut_doc *out_doc = yyjson_mut_doc_new(nullptr);

  std::vector<std::pair<std::string, yyjson_mut_val *>> records; // (ts_code, obj)
  std::unordered_set<std::string> seen;
  std::vector<std::string> field_names;

  for (const auto &l1 : l1_codes) {
    yyjson_doc *doc = http.call(
        "index_member_all", {{"l1_code", l1}, {"is_new", "Y"}});
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *data = yyjson_obj_get(root, "data");
    assert(data);
    yyjson_val *fields_arr = yyjson_obj_get(data, "fields");
    yyjson_val *items_arr = yyjson_obj_get(data, "items");
    assert(fields_arr && items_arr);

    // 字段顺序必须跨 L1 一致，首轮锚定，后续 assert
    std::vector<std::string> cur_fields;
    size_t fn = yyjson_arr_size(fields_arr);
    cur_fields.reserve(fn);
    for (size_t i = 0; i < fn; i++) {
      yyjson_val *v = yyjson_arr_get(fields_arr, i);
      assert(yyjson_is_str(v));
      cur_fields.emplace_back(yyjson_get_str(v));
    }
    if (field_names.empty())
      field_names = std::move(cur_fields);
    else
      assert(field_names == cur_fields);

    int ts_idx = -1;
    for (size_t i = 0; i < field_names.size(); i++) {
      if (field_names[i] == "ts_code") {
        ts_idx = static_cast<int>(i);
        break;
      }
    }
    assert(ts_idx >= 0);

    size_t added = 0, dup = 0;
    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(items_arr, i, n, item) {
      yyjson_val *tv = yyjson_arr_get(item, static_cast<size_t>(ts_idx));
      assert(yyjson_is_str(tv));
      std::string ts_code = yyjson_get_str(tv);
      if (seen.count(ts_code)) {
        dup++;
        continue;
      }

      yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
      for (size_t k = 0; k < field_names.size(); k++) {
        yyjson_val *v = yyjson_arr_get(item, k);
        yyjson_mut_val *mv = yyjson_val_mut_copy(out_doc, v);
        yyjson_mut_obj_add_val(out_doc, obj, field_names[k].c_str(), mv);
      }
      seen.insert(ts_code);
      records.emplace_back(std::move(ts_code), obj);
      added++;
    }
    yyjson_doc_free(doc);

    std::cout << " " << l1 << "=" << added;
    if (dup) std::cout << "(+" << dup << "dup)";
    std::cout << std::flush;
  }

  std::sort(records.begin(), records.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  yyjson_mut_val *out_root = yyjson_mut_arr(out_doc);
  yyjson_mut_doc_set_root(out_doc, out_root);
  for (auto &[k, v] : records) {
    yyjson_mut_arr_append(out_root, v);
  }

  size_t out_len = 0;
  char *json_str =
      yyjson_mut_write(out_doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json_str);
  misc::atomic_write(index_member_all_path(), json_str, out_len);
  std::free(json_str);
  yyjson_mut_doc_free(out_doc);

  std::cout << " -> " << records.size() << " total" << std::endl;
}

// ============================================================================
// 单 itf 去重 (data/_meta/<name>.lastupdate, 内容 = unix epoch seconds 文本)
// ============================================================================

bool should_skip_api(std::string_view name, int window_seconds) {
  assert(window_seconds >= 0);
  fs::path p = lastupdate_path(name);
  if (!fs::exists(p)) return false;
  std::string buf = misc::read_file_all(p);
  while (!buf.empty() &&
         (buf.back() == '\n' || buf.back() == '\r' || buf.back() == ' ')) {
    buf.pop_back();
  }
  assert(!buf.empty());
  int64_t last = std::stoll(buf);
  int64_t now = static_cast<int64_t>(std::time(nullptr));
  return (now - last) < window_seconds;
}

void mark_api_updated(std::string_view name) {
  int64_t now = static_cast<int64_t>(std::time(nullptr));
  std::string content = std::to_string(now) + "\n";
  misc::atomic_write(lastupdate_path(name), content.data(), content.size());
}

} // namespace tushare
