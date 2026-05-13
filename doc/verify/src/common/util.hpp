#pragma once

#include <cassert>
#include <cctype>
#include <string>

namespace verify::util {

// 把 "2024-12-31" / "2024-12-31 00:00:00" / "20241231" / "2024/12/31"
// 等任意含日期前缀的字符串规整成 "YYYYMMDD". 不够 8 位数字 → assert.
inline std::string to_yyyymmdd(const std::string &s) {
    std::string out;
    out.reserve(8);
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            out.push_back(c);
            if (out.size() == 8) break;
        }
    }
    assert(out.size() == 8 && "date 字符串解析为 YYYYMMDD 失败");
    return out;
}

} // namespace verify::util
