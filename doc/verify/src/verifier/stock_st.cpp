// verify_stock_st: tushare stock_st (name 含 '*' → *ST) vs bigquant cn_stock_status (st_status TINYINT).
// 派生字段 ts_st: name 含 '*' → 2, 否则 → 1.  与 bq st_status 直接整数比.
// tushare stock_st 始 2016-01-01.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

#include <algorithm>
#include <iostream>

namespace verify::run {

void stock_st(const Ctx &ctx) {
    int yf = std::max(ctx.year_from, 2016);
    if (yf > ctx.year_to) {
        std::cerr << "  [stock_st] year_from clamp 2016 > year_to, skip\n";
        return;
    }
    std::vector<diff::FieldSpec> specs = {
        {"st", true, 1.0, 1e-9},
    };
    diff::MultiYearReport rep("stock_st", yf, ctx.year_to, specs);

    auto recs = ts::load_itf(ctx.data_dir, "stock_st", yf, ctx.year_to, ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        auto it_n = r.fields.find("name");
        if (it_d == r.fields.end() || it_c == r.fields.end() || it_n == r.fields.end()) continue;
        bool star = it_n->second.find('*') != std::string::npos;
        diff::FieldMap fm;
        fm["st"]   = star ? "2" : "1";
        fm["name"] = it_n->second;
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, st_status FROM cn_stock_status WHERE st_status > 0",
        yf, ctx.year_to, "cn_stock_status");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_st   = t.idx("st_status");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["st"] = row[idx_st];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
