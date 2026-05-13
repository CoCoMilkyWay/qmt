#include "common/bq.hpp"
#include "verifier.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// 各 api 开关 — 在此 flip true/false. 不再走命令行.
// ============================================================
namespace {

struct ApiEntry {
    const char *name;
    void (*fn)(const verify::Ctx &);
    bool enabled;
};

//true
//false

constexpr ApiEntry kApis[] = {
    {"calendar",           &verify::run::calendar,           false},
    {"stk_limit",          &verify::run::stk_limit,          true},
    {"suspend_d",          &verify::run::suspend_d,          false},
    {"margin_secs",        &verify::run::margin_secs,        false},
    {"margin_detail",      &verify::run::margin_detail,      false},
    {"stock_st",           &verify::run::stock_st,           false},
    {"adj_factor",         &verify::run::adj_factor,         false},
    {"daily_basic_close",  &verify::run::daily_basic_close,  false},
    {"daily_basic_shares", &verify::run::daily_basic_shares, false},
};

std::vector<std::string> split_csv(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void usage(const char *prog) {
    std::cerr <<
        "用法: " << prog << " [选项]\n"
        "选项:\n"
        "  --year-from N        默认 2015\n"
        "  --year-to N          默认 2025\n"
        "  --data-dir PATH      tushare 落地根 (含 _meta + YYYY/MM/DD)\n"
        "  --out-dir PATH       输出根 (out/), 内部按 api/年 分文件\n"
        "  --sample-dates LIST  YYYYMMDD 逗号列表 (调试: 仅比这些日期)\n"
        "  --help\n"
        "api 开关在 main.cpp 顶部的 kApis 表里编辑.\n";
}

} // namespace

int main(int argc, char *argv[]) {
    verify::Ctx ctx;
    std::string sample_csv;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char *name) -> std::string {
            assert(i + 1 < argc && "缺少参数值");
            (void)name;
            return argv[++i];
        };
        if (a == "--year-from")         ctx.year_from = std::atoi(need("--year-from").c_str());
        else if (a == "--year-to")      ctx.year_to = std::atoi(need("--year-to").c_str());
        else if (a == "--data-dir")     ctx.data_dir = need("--data-dir");
        else if (a == "--out-dir")      ctx.out_dir = need("--out-dir");
        else if (a == "--sample-dates") sample_csv = need("--sample-dates");
        else if (a == "--help" || a == "-h") { usage(argv[0]); return 0; }
        else { std::cerr << "未知参数: " << a << "\n"; usage(argv[0]); return 2; }
    }

    assert(!ctx.data_dir.empty() && "必须 --data-dir");
    assert(!ctx.out_dir.empty()  && "必须 --out-dir");
    assert(fs::exists(ctx.data_dir) && "data-dir 不存在");
    assert(ctx.year_from <= ctx.year_to);

    if (!sample_csv.empty()) ctx.sample_dates = split_csv(sample_csv);

    std::cerr << "[main] data-dir=" << ctx.data_dir
              << "\n        out-dir=" << ctx.out_dir
              << "\n        years=[" << ctx.year_from << "," << ctx.year_to << "]"
              << "\n        apis=";
    bool any = false;
    for (const auto &e : kApis) {
        if (e.enabled) { std::cerr << e.name << " "; any = true; }
    }
    std::cerr << "\n";
    if (!any) {
        std::cerr << "[main] kApis 中无 enabled 项, 直接退出.\n";
        return 0;
    }

    auto t0 = std::chrono::steady_clock::now();
    for (const auto &e : kApis) {
        if (!e.enabled) continue;
        fs::path api_dir = ctx.out_dir / e.name;
        fs::create_directories(api_dir);
        verify::bq::set_csv_dir(api_dir);
        auto ts = std::chrono::steady_clock::now();
        std::cerr << "\n========== verifier: " << e.name << " ==========\n";
        e.fn(ctx);
        auto te = std::chrono::steady_clock::now();
        double el = std::chrono::duration<double>(te - ts).count();
        std::cerr << "[" << e.name << "] done in " << el << "s\n";
    }
    auto t1 = std::chrono::steady_clock::now();
    double total = std::chrono::duration<double>(t1 - t0).count();
    std::cerr << "\n[main] all done in " << total << "s\n";
    return 0;
}
