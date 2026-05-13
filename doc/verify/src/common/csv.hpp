#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace verify::csv {

// 极简 CSV 读取器.
// - 第一行是 header
// - 支持双引号包裹 (含字段内逗号和换行); 双引号内 "" 转义为 "
// - 不做类型推断, 所有 cell 是 std::string (空串表示空/NULL)
// 对 bq dai query 的 CSV 输出足够 (pyarrow/pandas 风格).
struct Table {
    std::vector<std::string> header;
    std::unordered_map<std::string, int> col_idx;  // name → idx
    std::vector<std::vector<std::string>> rows;    // 每行 cells.size() == header.size()

    int idx(const std::string &name) const;
    bool has(const std::string &name) const;
    const std::string &at(std::size_t row, const std::string &col) const;
};

Table read_csv(const std::filesystem::path &path);

} // namespace verify::csv
