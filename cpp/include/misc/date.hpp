#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

// 通用日期工具：YYYYMMDD 字符串与 std::chrono::sys_days 互转 + 日历日运算。
// 业务无关，可在任意子系统复用。
namespace misc {

// 当天 YYYYMMDD (本地时间)
std::string today_yyyymmdd();

// "YYYYMMDD" -> sys_days; size != 8 直接 assert
std::chrono::sys_days parse_yyyymmdd(std::string_view s);

// sys_days -> "YYYYMMDD"
std::string fmt_yyyymmdd(std::chrono::sys_days d);

// [start, end] 闭区间所有日历日，按字典序升序
std::vector<std::string> iter_days(std::string_view start, std::string_view end);

// "YYYYMMDD" + n 个日历日 (n 可为负)
std::string add_days(std::string_view yyyymmdd, int n);

// "YYYYMM" -> 该月最后一天 "DD" ("28" / "29" / "30" / "31")
std::string month_last_dd(std::string_view yyyymm);

} // namespace misc
