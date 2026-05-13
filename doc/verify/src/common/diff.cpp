#include "common/diff.hpp"
#include "common/fs.hpp"

#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <utility>

namespace verify::diff {

namespace fs = std::filesystem;

namespace {

inline std::string make_key(const std::string &inst, const std::string &date) {
    return inst + "|" + date;
}
inline void split_key(const std::string &k, std::string &inst, std::string &date) {
    auto p = k.find('|');
    inst.assign(k, 0, p);
    date.assign(k, p + 1, std::string::npos);
}

// 解析浮点; 空串 / "nan" / "NaN" → NaN. 返回 true 表示 "数值或缺失".
bool parse_num(const std::string &s, double &out) {
    if (s.empty()) {
        out = std::nan("");
        return true;
    }
    // pandas 缺失常打成空串; 偶尔可能 "nan"
    if (s == "nan" || s == "NaN" || s == "NAN") {
        out = std::nan("");
        return true;
    }
    char *end = nullptr;
    out = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    return true;
}

// 单 FieldSpec 比对: 返回 true 表示通过 (相等).
bool field_match(const FieldSpec &spec, const std::string &ts_v, const std::string &bq_v) {
    if (!spec.numeric) {
        return ts_v == bq_v;
    }
    double a, b;
    bool oa = parse_num(ts_v, a);
    bool ob = parse_num(bq_v, b);
    if (!oa || !ob) return false;
    bool na = std::isnan(a), nb = std::isnan(b);
    if (na && nb) return true;
    if (na || nb) return false;
    double lhs = a * spec.scale;
    double d = std::fabs(lhs - b);
    double tol = spec.eps * std::max(1.0, std::fabs(b));
    return d <= tol;
}

// 用 yyjson_mut_strncpy 让 key + val 都 copy 到 doc arena, 避免 fm 局部
// 字符串生命周期问题 (yyjson 默认 _add_str 系列 key 不拷贝).
void add_field_obj(yyjson_mut_doc *doc, yyjson_mut_val *root,
                   const char *key, const FieldMap &fm) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    // 排序 key 让输出稳定
    std::vector<std::string> ks;
    ks.reserve(fm.size());
    for (auto &kv : fm) ks.push_back(kv.first);
    std::sort(ks.begin(), ks.end());
    for (auto &k : ks) {
        auto &v = fm.at(k);
        yyjson_mut_val *jk = yyjson_mut_strncpy(doc, k.c_str(), k.size());
        yyjson_mut_val *jv = yyjson_mut_strncpy(doc, v.c_str(), v.size());
        yyjson_mut_obj_add(obj, jk, jv);
    }
    yyjson_mut_obj_add_val(doc, root, key, obj);
}

} // namespace

YearReport::YearReport(int year, std::vector<FieldSpec> specs)
    : year_(year), specs_(std::move(specs)) {}

void YearReport::add_ts(std::string inst, std::string date, FieldMap f) {
    ts_[make_key(inst, date)] = std::move(f);
}
void YearReport::add_bq(std::string inst, std::string date, FieldMap f) {
    bq_[make_key(inst, date)] = std::move(f);
}

void YearReport::write(const fs::path &out_dir) {
    // 1. union keys
    std::set<std::string> all_keys;
    for (auto &kv : ts_) all_keys.insert(kv.first);
    for (auto &kv : bq_) all_keys.insert(kv.first);

    // 2. 分类
    std::size_t ts_only_n = 0, bq_only_n = 0, diff_n = 0, match_n = 0;
    // instrument 维度: 每个 instrument 三态布尔
    struct InstFlags { bool on_ts=false, on_bq=false, has_diff=false; };
    std::unordered_map<std::string, InstFlags> insts;

    // diffs: instrument -> date -> kind/ts/bq
    // 用 std::map 保持 (instrument, date) 输出顺序稳定
    struct DiffEntry { std::string kind; const FieldMap *ts=nullptr, *bq=nullptr; };
    std::map<std::string, std::map<std::string, DiffEntry>> diffs;

    for (auto &k : all_keys) {
        std::string inst, date;
        split_key(k, inst, date);
        auto it_ts = ts_.find(k);
        auto it_bq = bq_.find(k);
        InstFlags &fl = insts[inst];
        if (it_ts != ts_.end()) fl.on_ts = true;
        if (it_bq != bq_.end()) fl.on_bq = true;
        if (it_ts != ts_.end() && it_bq == bq_.end()) {
            ++ts_only_n;
            fl.has_diff = true;
            diffs[inst][date] = {"ts_only", &it_ts->second, nullptr};
        } else if (it_bq != bq_.end() && it_ts == ts_.end()) {
            ++bq_only_n;
            fl.has_diff = true;
            diffs[inst][date] = {"bq_only", nullptr, &it_bq->second};
        } else {
            // both
            bool ok = true;
            for (auto &spec : specs_) {
                auto fa = it_ts->second.find(spec.name);
                auto fb = it_bq->second.find(spec.name);
                std::string va = (fa == it_ts->second.end()) ? std::string{} : fa->second;
                std::string vb = (fb == it_bq->second.end()) ? std::string{} : fb->second;
                if (!field_match(spec, va, vb)) { ok = false; break; }
            }
            if (ok) {
                ++match_n;
            } else {
                ++diff_n;
                fl.has_diff = true;
                diffs[inst][date] = {"diff", &it_ts->second, &it_bq->second};
            }
        }
    }

    // 3. instrument 三态统计
    std::size_t ts_only_inst = 0, bq_only_inst = 0, diff_inst = 0;
    for (auto &kv : insts) {
        auto &f = kv.second;
        if (f.on_ts && !f.on_bq) ++ts_only_inst;
        else if (f.on_bq && !f.on_ts) ++bq_only_inst;
        else if (f.has_diff) ++diff_inst;
    }

    std::size_t union_n = all_keys.size();
    auto pct = [&](std::size_t x) {
        return union_n == 0 ? 0.0 : static_cast<double>(x) / static_cast<double>(union_n);
    };

    // 4. 写 JSON
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "year", year_);

    yyjson_mut_val *summ = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_uint(doc, summ, "union_entries", union_n);
    yyjson_mut_obj_add_uint(doc, summ, "match_entries", match_n);
    yyjson_mut_obj_add_uint(doc, summ, "ts_only_entries", ts_only_n);
    yyjson_mut_obj_add_real(doc, summ, "ts_only_entries_pct", pct(ts_only_n));
    yyjson_mut_obj_add_uint(doc, summ, "bq_only_entries", bq_only_n);
    yyjson_mut_obj_add_real(doc, summ, "bq_only_entries_pct", pct(bq_only_n));
    yyjson_mut_obj_add_uint(doc, summ, "diff_entries", diff_n);
    yyjson_mut_obj_add_real(doc, summ, "diff_entries_pct", pct(diff_n));
    yyjson_mut_obj_add_uint(doc, summ, "total_instruments", insts.size());
    yyjson_mut_obj_add_uint(doc, summ, "ts_only_instruments", ts_only_inst);
    yyjson_mut_obj_add_uint(doc, summ, "bq_only_instruments", bq_only_inst);
    yyjson_mut_obj_add_uint(doc, summ, "diff_instruments", diff_inst);
    yyjson_mut_obj_add_val(doc, root, "summary", summ);

    yyjson_mut_val *diffs_obj = yyjson_mut_obj(doc);
    for (auto &kv : diffs) {
        const std::string &inst = kv.first;
        yyjson_mut_val *inst_obj = yyjson_mut_obj(doc);
        for (auto &kv2 : kv.second) {
            const std::string &date = kv2.first;
            const DiffEntry &e = kv2.second;
            yyjson_mut_val *e_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_strncpy(doc, e_obj, "kind", e.kind.c_str(), e.kind.size());
            if (e.ts) add_field_obj(doc, e_obj, "ts", *e.ts);
            if (e.bq) add_field_obj(doc, e_obj, "bq", *e.bq);
            yyjson_mut_obj_add_val(doc, inst_obj, date.c_str(), e_obj);
        }
        yyjson_mut_obj_add_val(doc, diffs_obj, inst.c_str(), inst_obj);
    }
    yyjson_mut_obj_add_val(doc, root, "diffs", diffs_obj);

    char yrbuf[16];
    std::snprintf(yrbuf, sizeof(yrbuf), "%d.json", year_);
    fs::path out_path = out_dir / yrbuf;

    size_t out_len = 0;
    char *jstr = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
    assert(jstr);
    fs_util::atomic_write(out_path, jstr, out_len);
    std::free(jstr);
    yyjson_mut_doc_free(doc);

    std::cerr << "  [year " << year_ << "] union=" << union_n
              << "  match=" << match_n
              << "  ts_only=" << ts_only_n
              << "  bq_only=" << bq_only_n
              << "  diff=" << diff_n
              << "  insts(ts_only/bq_only/diff)="
              << ts_only_inst << "/" << bq_only_inst << "/" << diff_inst
              << "  → " << out_path.string() << "\n";
}

// ---------------- MultiYearReport ----------------

MultiYearReport::MultiYearReport(std::string api, int yf, int yt,
                                 std::vector<FieldSpec> specs)
    : api_(std::move(api)), year_from_(yf), year_to_(yt), specs_(std::move(specs)) {
    for (int y = yf; y <= yt; ++y) {
        years_.emplace(y, YearReport(y, specs_));
    }
}

YearReport &MultiYearReport::get_year(const std::string &date) {
    assert(date.size() >= 4);
    int y = std::atoi(date.substr(0, 4).c_str());
    auto it = years_.find(y);
    assert(it != years_.end() && "date 年份越出 [year_from, year_to] 范围");
    return it->second;
}

void MultiYearReport::add_ts(const std::string &inst, const std::string &date, FieldMap f) {
    get_year(date).add_ts(inst, date, std::move(f));
}
void MultiYearReport::add_bq(const std::string &inst, const std::string &date, FieldMap f) {
    get_year(date).add_bq(inst, date, std::move(f));
}

void MultiYearReport::write_all(const fs::path &out_root) {
    fs::path dir = out_root / api_;
    fs::create_directories(dir);
    std::cerr << "[" << api_ << "] writing reports → " << dir.string() << "\n";
    for (int y = year_from_; y <= year_to_; ++y) {
        auto it = years_.find(y);
        assert(it != years_.end());
        it->second.write(dir);
    }
}

} // namespace verify::diff
