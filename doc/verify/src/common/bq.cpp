#include "common/bq.hpp"

#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace verify::bq {

namespace fs = std::filesystem;

namespace {

fs::path g_csv_dir;  // 空 = /tmp 临时文件 (读后删); 非空 = 落到此目录且保留

// 单引号包裹 + 内部 ' 转义为 '\'' (Posix sh).
std::string shq(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

fs::path mktemp_csv() {
    // 简单 pid+counter 路径, 单进程内不会撞; 不用 mkstemps 避免 feature-test 麻烦
    static int counter = 0;
    fs::path tmp_root = fs::temp_directory_path();
    pid_t pid = getpid();
    for (;;) {
        fs::path p = tmp_root / ("verify_bq_" + std::to_string(pid)
                                 + "_" + std::to_string(counter++) + ".csv");
        if (!fs::exists(p)) return p;
    }
}

std::string sanitize_name(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '_' || c == '-' || c == '.') out.push_back(c);
        else out.push_back('_');
    }
    if (out.empty()) out = "query";
    return out;
}

} // namespace

void set_csv_dir(const fs::path &dir) {
    g_csv_dir = dir;
}

csv::Table query(const QueryOpts &opts) {
    fs::path csv_path;
    bool keep;
    if (g_csv_dir.empty()) {
        csv_path = mktemp_csv();
        keep = false;
    } else {
        std::string base = sanitize_name(opts.tag.empty() ? opts.sql.substr(0, 40) : opts.tag);
        csv_path = g_csv_dir / (base + ".csv");
        keep = true;
    }

    // 缓存命中: 已有非空 csv 就直接读, 跳过 bq dai query
    if (keep && fs::exists(csv_path) && fs::file_size(csv_path) > 0) {
        auto sz = fs::file_size(csv_path);
        std::cerr << "  [bq] CACHE  " << csv_path.string()
                  << "  " << static_cast<double>(sz) / 1e6 << " MB  reading...\n";
        csv::Table t = csv::read_csv(csv_path);
        std::cerr << "  [bq] parsed " << t.rows.size() << " rows, cols=[";
        for (std::size_t i = 0; i < t.header.size(); ++i) {
            if (i) std::cerr << ",";
            std::cerr << t.header[i];
        }
        std::cerr << "]\n";
        return t;
    }

    std::ostringstream cmd;
    cmd << "bq dai query " << shq(opts.sql)
        << " --limit 0 -o " << shq(csv_path.string());
    if (!opts.date_from.empty()) {
        assert(!opts.date_to.empty());
        // --filters '{"date":["F","T"]}'
        std::string filt = "{\"date\":[\"" + opts.date_from + "\",\"" + opts.date_to + "\"]}";
        cmd << " --filters " << shq(filt);
    } else if (opts.full_scan) {
        cmd << " --full-db-scan";
    }

    std::string label = opts.tag.empty() ? opts.sql.substr(0, 60) : opts.tag;
    for (auto &c : label) if (c == '\n') c = ' ';
    std::cerr << "  [bq] " << label
              << "  date=[" << opts.date_from << "," << opts.date_to << "]"
              << "  → " << csv_path.string() << "\n";

    auto t0 = std::chrono::steady_clock::now();
    std::string full = cmd.str();
    int rc = std::system(full.c_str());
    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    if (rc != 0) {
        std::cerr << "  [bq] FAIL rc=" << rc << " elapsed=" << elapsed << "s\n"
                  << "       cmd: " << full << "\n";
        if (!keep && fs::exists(csv_path)) fs::remove(csv_path);
        assert(false && "bq dai query 返回非零");
    }

    assert(fs::exists(csv_path) && fs::file_size(csv_path) > 0 && "bq 输出空");
    auto sz = fs::file_size(csv_path);
    std::cerr << "  [bq] done " << elapsed << "s  csv="
              << static_cast<double>(sz) / 1e6 << " MB  reading...\n";

    csv::Table t = csv::read_csv(csv_path);
    std::cerr << "  [bq] parsed " << t.rows.size() << " rows, cols=[";
    for (std::size_t i = 0; i < t.header.size(); ++i) {
        if (i) std::cerr << ",";
        std::cerr << t.header[i];
    }
    std::cerr << "]\n";

    if (!keep) fs::remove(csv_path);
    return t;
}

std::vector<csv::Table> query_yearly(const std::string &sql,
                                     int year_from, int year_to,
                                     const std::string &tag) {
    std::cerr << "  [bq yearly] " << (tag.empty() ? sql.substr(0, 50) : tag)
              << "  years=[" << year_from << "," << year_to << "]\n";
    std::vector<csv::Table> out;
    out.reserve(year_to - year_from + 1);
    for (int y = year_from; y <= year_to; ++y) {
        QueryOpts opts;
        opts.sql = sql;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d-01-01", y);
        opts.date_from = buf;
        std::snprintf(buf, sizeof(buf), "%d-12-31", y);
        opts.date_to = buf;
        opts.tag = (tag.empty() ? sql.substr(0, 50) : tag) + " y=" + std::to_string(y);
        out.push_back(query(opts));
    }
    return out;
}

} // namespace verify::bq
