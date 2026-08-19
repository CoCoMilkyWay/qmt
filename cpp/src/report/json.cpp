#include "report/json.hpp"

#include <cassert>
#include <cstdint>

namespace report {

namespace {

// 运行期 key → 拷进 doc 内存池的 str val (yyjson 原生 add_* 不拷 key).
yyjson_mut_val *key_val(yyjson_mut_doc *doc, std::string_view k) {
  yyjson_mut_val *kv = yyjson_mut_strncpy(doc, k.data(), k.size());
  assert(kv && "report::json: key 分配失败");
  return kv;
}

void attach(yyjson_mut_doc *doc, yyjson_mut_val *parent, std::string_view key,
            yyjson_mut_val *val) {
  bool ok = yyjson_mut_obj_add(parent, key_val(doc, key), val);
  assert(ok && "report::json: 挂载失败 (parent 非 object?)");
  (void)ok;
}

} // namespace

yyjson_mut_val *add_obj(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        const char *key) {
  assert(doc && parent && key);
  yyjson_mut_val *o = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_val(doc, parent, key, o);
  return o;
}

yyjson_mut_val *add_obj(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        std::string_view key) {
  assert(doc && parent);
  yyjson_mut_val *o = yyjson_mut_obj(doc);
  attach(doc, parent, key, o);
  return o;
}

yyjson_mut_val *add_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        const char *key) {
  assert(doc && parent && key);
  yyjson_mut_val *a = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, parent, key, a);
  return a;
}

yyjson_mut_val *add_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        std::string_view key) {
  assert(doc && parent);
  yyjson_mut_val *a = yyjson_mut_arr(doc);
  attach(doc, parent, key, a);
  return a;
}

void add_str_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                 std::span<const std::string> v) {
  yyjson_mut_val *arr = add_arr(doc, parent, key);
  for (const std::string &s : v)
    yyjson_mut_arr_add_strncpy(doc, arr, s.data(), s.size());
}

void add_sv_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const std::string_view> v) {
  yyjson_mut_val *arr = add_arr(doc, parent, key);
  for (std::string_view s : v)
    yyjson_mut_arr_add_strncpy(doc, arr, s.data(), s.size());
}

void add_f4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const float> v) {
  yyjson_mut_val *arr = add_arr(doc, parent, key);
  for (float x : v)
    yyjson_mut_arr_add_real(doc, arr, static_cast<double>(x));
}

void add_f4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                std::string_view key, std::span<const float> v) {
  yyjson_mut_val *arr = add_arr(doc, parent, key);
  for (float x : v)
    yyjson_mut_arr_add_real(doc, arr, static_cast<double>(x));
}

void add_i4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const std::int32_t> v) {
  yyjson_mut_val *arr = add_arr(doc, parent, key);
  for (std::int32_t x : v)
    yyjson_mut_arr_add_int(doc, arr, x);
}

void add_f4(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
            float v) {
  assert(doc && parent && key);
  yyjson_mut_obj_add_real(doc, parent, key, static_cast<double>(v));
}

void add_str(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
             std::string_view v) {
  assert(doc && parent && key);
  yyjson_mut_obj_add_val(doc, parent, key,
                         yyjson_mut_strncpy(doc, v.data(), v.size()));
}

} // namespace report
