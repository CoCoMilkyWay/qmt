// verify_stk_limit: tushare stk_limit (up_limit/down_limit) vs bigquant cn_stock_limit_price.
// PK=(trade_date, ts_code); 字段映射 up_limit↔upper_limit, down_limit↔lower_limit; 均未复权.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

#include <iostream>

namespace verify::run {

void stk_limit(const Ctx &ctx) {
    std::vector<diff::FieldSpec> specs = {
        {"up_limit",   true, 1.0, 1e-6},
        {"down_limit", true, 1.0, 1e-6},
    };
    diff::MultiYearReport rep("stk_limit", ctx.year_from, ctx.year_to, specs);

    auto recs = ts::load_itf(ctx.data_dir, "stk_limit", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        auto it_u = r.fields.find("up_limit");
        auto it_l = r.fields.find("down_limit");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        diff::FieldMap fm;
        if (it_u != r.fields.end()) fm["up_limit"]   = it_u->second;
        if (it_l != r.fields.end()) fm["down_limit"] = it_l->second;
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, upper_limit, lower_limit FROM cn_stock_limit_price",
        ctx.year_from, ctx.year_to, "cn_stock_limit_price");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_up = t.idx("upper_limit");
        int idx_dn = t.idx("lower_limit");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["up_limit"]   = row[idx_up];
            fm["down_limit"] = row[idx_dn];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
