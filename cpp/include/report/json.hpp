#pragma once

#include "package/yyjson/yyjson.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// ============================================================================
// report.json 拼装小工具 — 消掉 backtest / analysis / aggregate / main 里重复的
//   add_*_arr lambda. 唯一职责是"把 span 挂成 JSON 数组", 不含任何业务判断.
//
// 表格一律**列式**落盘 (`{"列名": [...], ...}`): 与 npy / pandas / py 侧
//   _html_table(header, cols) 同构, 前端零转置直接渲染.
//
// float NaN 走 misc::atomic_write_json 的 ALLOW_INF_AND_NAN (见 misc/fs.hpp).
//
// 生命周期: yyjson 原生 add_* 既不拷 key 也不拷 value 字符串 (悬垂风险). 本层
//   全部走 *ncpy 变体把字符串拷进 doc 内存池 ⇒ 调用方的临时 std::string /
//   string_view 用完即可释放. 唯一例外: key 为 const char* 的重载走 yyjson 原生
//   不拷贝路径, 只允许传字符串**字面量** (静态存储期); 运行期拼出来的 key 必须用
//   string_view 重载.
// ============================================================================
namespace report {

// 在 parent 下新建一个子对象并返回 (key 挂好).
yyjson_mut_val *add_obj(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        const char *key);
yyjson_mut_val *add_obj(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        std::string_view key);

// 在 parent 下新建一个子数组并返回 (key 挂好).
yyjson_mut_val *add_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        const char *key);
yyjson_mut_val *add_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                        std::string_view key);

void add_str_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                 std::span<const std::string> v);

// 编译期常量名 (特征名 / 策略名) 的数组形态
void add_sv_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const std::string_view> v);

void add_f4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const float> v);
void add_f4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                std::string_view key, std::span<const float> v);

void add_i4_arr(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
                std::span<const std::int32_t> v);

// 标量 (float NaN 允许)
void add_f4(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
            float v);

// 字符串标量 (值走拷贝)
void add_str(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key,
             std::string_view v);

} // namespace report
