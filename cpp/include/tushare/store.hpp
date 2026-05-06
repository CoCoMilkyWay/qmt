#pragma once

#include "package/yyjson/yyjson.h"
#include "tushare/spec.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tushare::store {

namespace fs = std::filesystem;

// 从 cwd 向上爬找含 .git 与 data/ 的目录
fs::path qmt_root();
fs::path data_path(std::string_view yyyymmdd, std::string_view name);

// 列出 [start, end] 闭区间内 day 文件不存在的日期 (按字典序升序)
std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end);

// 把 fetch 回来的 (fields, items) 按 visible_date 分桶
// task 范围内每天都写文件 (无数据 → []); PK upsert + tmp+rename
void write_by_visible_date(yyjson_val *fields_arr, yyjson_val *items_arr,
                           const InterfaceSpec &spec, const FetchTask &task);

} // namespace tushare::store
