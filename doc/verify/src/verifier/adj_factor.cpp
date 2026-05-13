// verify_adj_factor: 两边均累计因子但基期不同, 比相邻交易日比值.
// ts: adj_factor[i] / adj_factor[i-1]  vs  bq: adjust_factor[i] / adjust_factor[i-1]
// 每个 ts_code 第一日 ratio 缺失 (NaN, 与对侧 NaN 匹配; 与对侧非 NaN 报 diff).

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace verify::run {

namespace {

struct Row { std::string date; double factor; };

void compute_and_add(diff::MultiYearReport &rep, bool to_ts,
                     std::unordered_map<std::string, std::vector<Row>> &by_stock) {
    for (auto &kv : by_stock) {
        const std::string &inst = kv.first;
        auto &rows = kv.second;
        std::sort(rows.begin(), rows.end(),
                  [](const Row &a, const Row &b) { return a.date < b.date; });
        for (std::size_t i = 0; i < rows.size(); ++i) {
            diff::FieldMap fm;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.17g", rows[i].factor);
            fm["factor"] = buf;
            if (i > 0 && rows[i - 1].factor > 0.0) {
                double r = rows[i].factor / rows[i - 1].factor;
                std::snprintf(buf, sizeof(buf), "%.17g", r);
                fm["ratio"] = buf;
            }
            if (to_ts) rep.add_ts(inst, rows[i].date, std::move(fm));
            else       rep.add_bq(inst, rows[i].date, std::move(fm));
        }
    }
}

} // namespace

void adj_factor(const Ctx &ctx) {
    std::vector<diff::FieldSpec> specs = {
        {"ratio", true, 1.0, 1e-5},
    };
    diff::MultiYearReport rep("adj_factor", ctx.year_from, ctx.year_to, specs);

    // ts
    auto recs = ts::load_itf(ctx.data_dir, "adj_factor", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    std::unordered_map<std::string, std::vector<Row>> ts_by;
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        auto it_a = r.fields.find("adj_factor");
        if (it_d == r.fields.end() || it_c == r.fields.end() || it_a == r.fields.end()) continue;
        if (it_a->second.empty()) continue;
        ts_by[it_c->second].push_back({it_d->second, std::strtod(it_a->second.c_str(), nullptr)});
    }
    compute_and_add(rep, /*to_ts*/ true, ts_by);

    // bq
    auto tables = bq::query_yearly(
        "SELECT date, instrument, adjust_factor FROM cn_stock_real_bar1d",
        ctx.year_from, ctx.year_to, "cn_stock_real_bar1d.adjust_factor");
    std::unordered_map<std::string, std::vector<Row>> bq_by;
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_af   = t.idx("adjust_factor");
        for (auto &row : t.rows) {
            if (row[idx_af].empty()) continue;
            std::string d = util::to_yyyymmdd(row[idx_date]);
            bq_by[row[idx_inst]].push_back({d, std::strtod(row[idx_af].c_str(), nullptr)});
        }
    }
    compute_and_add(rep, /*to_ts*/ false, bq_by);

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
