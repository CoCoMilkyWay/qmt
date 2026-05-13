#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace verify {

// 单个 verifier 上下文 + 入口签名.
struct Ctx {
    std::filesystem::path data_dir;  // tushare 落地根 (data/)
    std::filesystem::path out_dir;   // 输出根 (out/) — 内部再建 <api>/<year>.json
    int year_from = 2015;
    int year_to = 2025;
    std::vector<std::string> sample_dates;  // 空 = 全期; 非空 = 仅这些 YYYYMMDD
};

} // namespace verify

// 各 verifier 暴露的 run 函数: 在自己 .cpp 里实现, 由 main.cpp 的 constexpr 表分发.
namespace verify::run {
void calendar(const Ctx &);
void stk_limit(const Ctx &);
void suspend_d(const Ctx &);
void margin_secs(const Ctx &);
void margin_detail(const Ctx &);
void stock_st(const Ctx &);
void adj_factor(const Ctx &);
void daily_basic_close(const Ctx &);
void daily_basic_shares(const Ctx &);
} // namespace verify::run
