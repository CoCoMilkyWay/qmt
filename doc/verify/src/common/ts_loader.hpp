#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace verify::ts {

// 一条 tushare 记录 (单 itf 的单行). 所有字段值统一存字符串 (数值再 parse_double).
struct Record {
    std::string visible_date;  // 文件所在日 YYYYMMDD (= 落地 PK 之一)
    std::unordered_map<std::string, std::string> fields;
};

// 把数值转字符串时, NULL/缺失 → 空串.
// 浮点小数 → yyjson 原样字符串 (避免 round-trip 误差).

// 遍历 [year_from..year_to] 全部 data/YYYY/MM/DD/<itf>.json, 输出全部记录.
// sample_dates 若非空, 仅留 visible_date ∈ sample_dates.
std::vector<Record> load_itf(const std::filesystem::path &data_dir,
                             const std::string &itf,
                             int year_from, int year_to,
                             const std::vector<std::string> &sample_dates = {});

// 读 data/_meta/<name>.json 整文件文本 (调用方用 yyjson 自行解析).
std::string read_meta_raw(const std::filesystem::path &data_dir, const std::string &name);

} // namespace verify::ts
