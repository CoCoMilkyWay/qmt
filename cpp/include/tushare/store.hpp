#pragma once

#include "package/yyjson/yyjson.h"
#include "tushare/spec.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tushare::store {

// data/YYYY/MM/DD/{name}.json 路径 (基于 misc::git_root() / "data")
std::filesystem::path data_path(std::string_view yyyymmdd,
                                std::string_view name);

// 列出 [start, end] 闭区间内需要拉取的日期 (按字典序升序)
// - 文件不存在 → 必拉
// - 最近 lookback_days 个日历日内 → 必拉 (PK upsert 吃增量/订正)
//   日历 7 天约等于 5 个交易日，避免当日数据未结算导致累积缺失
std::vector<std::string> scan_missing(const InterfaceSpec &spec,
                                      std::string_view start,
                                      std::string_view end,
                                      int lookback_days);

// 把 fetch 回来的 (fields, items) 按 visible_date 分桶
// task 范围内每天都写文件 (无数据 → []); PK upsert + tmp+rename
void write_by_visible_date(yyjson_val *fields_arr, yyjson_val *items_arr,
                           const InterfaceSpec &spec, const FetchTask &task);

} // namespace tushare::store
