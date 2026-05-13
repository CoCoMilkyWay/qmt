#include "common/ts_loader.hpp"
#include "common/fs.hpp"

#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace verify::ts {

namespace fs = std::filesystem;

namespace {

// 把任意 yyjson_val 转字符串:
// - str: 原样
// - int/uint/real: 文本表示 (yyjson 内部已存原 raw, 但 API 只暴露 num; 用 sprintf)
// - bool: "true"/"false"
// - null: ""
// - 对象/数组: 序列化为 mini json (一般 daily_basic 不会出现, 留兜底)
std::string val_to_str(yyjson_val *v) {
    if (!v) return {};
    yyjson_type t = yyjson_get_type(v);
    if (t == YYJSON_TYPE_STR) return yyjson_get_str(v);
    if (t == YYJSON_TYPE_NULL) return {};
    if (t == YYJSON_TYPE_BOOL) return yyjson_get_bool(v) ? "true" : "false";
    if (t == YYJSON_TYPE_NUM) {
        yyjson_subtype st = yyjson_get_subtype(v);
        char buf[64];
        if (st == YYJSON_SUBTYPE_SINT) {
            std::snprintf(buf, sizeof(buf), "%lld",
                          static_cast<long long>(yyjson_get_sint(v)));
        } else if (st == YYJSON_SUBTYPE_UINT) {
            std::snprintf(buf, sizeof(buf), "%llu",
                          static_cast<unsigned long long>(yyjson_get_uint(v)));
        } else {
            // real: 17 位精度足以无损 round-trip
            std::snprintf(buf, sizeof(buf), "%.17g", yyjson_get_real(v));
        }
        return buf;
    }
    // 对象/数组: 序列化
    size_t out_len = 0;
    char *s = yyjson_val_write(v, 0, &out_len);
    if (!s) return {};
    std::string r(s, out_len);
    std::free(s);
    return r;
}

void load_one_day_file(const fs::path &file, const std::string &visible_date,
                       std::vector<Record> &out) {
    std::string buf = fs_util::read_file_all(file);
    if (buf.empty()) return;
    yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
    assert(doc && "yyjson parse 失败");
    yyjson_val *root = yyjson_doc_get_root(doc);
    assert(yyjson_is_arr(root) && "tushare day json 顶层必须是 array");

    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(root, i, n, item) {
        assert(yyjson_is_obj(item));
        Record rec;
        rec.visible_date = visible_date;
        rec.fields.reserve(yyjson_obj_size(item));
        size_t j, m;
        yyjson_val *k, *v;
        yyjson_obj_foreach(item, j, m, k, v) {
            assert(yyjson_is_str(k));
            rec.fields.emplace(yyjson_get_str(k), val_to_str(v));
        }
        out.push_back(std::move(rec));
    }
    yyjson_doc_free(doc);
}

struct Job {
    fs::path file;
    std::string visible_date;
};

unsigned int worker_count() {
    if (const char *env = std::getenv("VERIFY_THREADS")) {
        int v = std::atoi(env);
        if (v > 0) return static_cast<unsigned int>(v);
    }
    unsigned int n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;
    return n;
}

} // namespace

std::vector<Record> load_itf(const fs::path &data_dir, const std::string &itf,
                             int year_from, int year_to,
                             const std::vector<std::string> &sample_dates) {
    std::unordered_set<std::string> sample_set(sample_dates.begin(), sample_dates.end());

    std::vector<Job> jobs;
    for (int y = year_from; y <= year_to; ++y) {
        char ystr[8];
        std::snprintf(ystr, sizeof(ystr), "%04d", y);
        fs::path ydir = data_dir / ystr;
        if (!fs::exists(ydir)) continue;
        for (int mo = 1; mo <= 12; ++mo) {
            char mstr[8];
            std::snprintf(mstr, sizeof(mstr), "%02d", mo);
            fs::path mdir = ydir / mstr;
            if (!fs::exists(mdir)) continue;
            std::vector<std::string> dds;
            for (auto &ent : fs::directory_iterator(mdir)) {
                std::string n = ent.path().filename().string();
                if (n.size() == 2 && std::isdigit(static_cast<unsigned char>(n[0]))
                    && std::isdigit(static_cast<unsigned char>(n[1]))
                    && ent.is_directory()) {
                    dds.push_back(n);
                }
            }
            std::sort(dds.begin(), dds.end());
            for (auto &dd : dds) {
                std::string vd = std::string(ystr) + mstr + dd;
                if (!sample_set.empty() && !sample_set.count(vd)) continue;
                fs::path f = mdir / dd / (itf + ".json");
                if (!fs::exists(f)) continue;
                jobs.push_back({std::move(f), std::move(vd)});
            }
        }
    }

    if (jobs.empty()) {
        std::cerr << "  [ts " << itf << "] total 0 day files, rows=0\n";
        return {};
    }

    unsigned int nthreads = worker_count();
    if (static_cast<std::size_t>(nthreads) > jobs.size()) {
        nthreads = static_cast<unsigned int>(jobs.size());
    }
    std::cerr << "  [ts " << itf << "] parsing " << jobs.size()
              << " day files with " << nthreads << " threads\n";

    std::vector<std::vector<Record>> partials(jobs.size());
    std::atomic<std::size_t> next_idx{0};
    std::atomic<std::size_t> done{0};
    std::mutex log_mu;
    std::size_t last_log = 0;

    auto worker = [&] {
        for (;;) {
            std::size_t i = next_idx.fetch_add(1, std::memory_order_relaxed);
            if (i >= jobs.size()) return;
            load_one_day_file(jobs[i].file, jobs[i].visible_date, partials[i]);
            std::size_t d = done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (d - last_log >= 500 || d == jobs.size()) {
                std::lock_guard<std::mutex> lk(log_mu);
                if (d - last_log >= 500 || d == jobs.size()) {
                    std::cerr << "  [ts " << itf << "] read " << d << "/"
                              << jobs.size() << " day files\n";
                    last_log = d;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(nthreads);
    for (unsigned int t = 0; t < nthreads; ++t) threads.emplace_back(worker);
    for (auto &t : threads) t.join();

    std::size_t total_rows = 0;
    for (auto &p : partials) total_rows += p.size();

    std::vector<Record> out;
    out.reserve(total_rows);
    for (auto &p : partials) {
        for (auto &r : p) out.push_back(std::move(r));
        std::vector<Record>().swap(p);
    }

    std::cerr << "  [ts " << itf << "] total " << jobs.size()
              << " day files, rows=" << out.size() << "\n";
    return out;
}

std::string read_meta_raw(const fs::path &data_dir, const std::string &name) {
    fs::path p = data_dir / "_meta" / name;
    assert(fs::exists(p) && "_meta 文件不存在");
    return fs_util::read_file_all(p);
}

} // namespace verify::ts
