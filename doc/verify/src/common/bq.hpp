#pragma once

#include "common/csv.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace verify::bq {

// 由 main 在每个 verifier 调用前设置: 之后 bq::query 写出的 CSV 落到此目录且保留.
// 空 (默认) = 用 /tmp 临时文件, 读后删 (旧行为).
void set_csv_dir(const std::filesystem::path &dir);

// 调用 `bq dai query <sql> --filters {date:[F,T]} --limit 0 -o tmp.csv`,
// 等返回, 然后读 CSV. 失败直接 assert.
// 注: 现有 Python verify_*.py 走同样路径; 在目标环境必须 bq CLI 可用.
struct QueryOpts {
    std::string sql;
    std::string date_from;   // "YYYY-MM-DD" 或空
    std::string date_to;     // "YYYY-MM-DD" 或空
    bool full_scan = false;  // 无 date 限制时需要
    std::string tag;         // 日志 label (可选)
};

csv::Table query(const QueryOpts &opts);

// 按年分段拉 (绕 bigquant 单次输出 200MB 上限). sql 不带 WHERE date.
// 返回 vector<Table>, 每年一个; 调用方按需合并.
// 任意一年 0 行也保留 (header 仍在).
std::vector<csv::Table> query_yearly(const std::string &sql,
                                     int year_from, int year_to,
                                     const std::string &tag = "");

} // namespace verify::bq
