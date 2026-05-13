// verify_daily_basic_shares: tushare daily_basic.{total/float/free}_share (万股) vs bigquant cn_stock_shares (股).
// 单位换算 scale=1e4 (tushare 万股 × 1e4 → bigquant 股). eps=1e-4.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

namespace verify::run {

void daily_basic_shares(const Ctx &ctx) {
    std::vector<diff::FieldSpec> specs = {
        {"total_shares",      true, 1e4, 1e-4},
        {"a_float_shares",    true, 1e4, 1e-4},
        {"free_float_shares", true, 1e4, 1e-4},
    };
    diff::MultiYearReport rep("daily_basic_shares", ctx.year_from, ctx.year_to, specs);

    auto recs = ts::load_itf(ctx.data_dir, "daily_basic", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        auto take = [&](const char *src, const char *unified, diff::FieldMap &fm) {
            auto it = r.fields.find(src);
            if (it != r.fields.end()) fm[unified] = it->second;
        };
        diff::FieldMap fm;
        take("total_share", "total_shares",      fm);
        take("float_share", "a_float_shares",    fm);
        take("free_share",  "free_float_shares", fm);
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, total_shares, a_float_shares, free_float_shares "
        "FROM cn_stock_shares",
        ctx.year_from, ctx.year_to, "cn_stock_shares");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_ts   = t.idx("total_shares");
        int idx_af   = t.idx("a_float_shares");
        int idx_ff   = t.idx("free_float_shares");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["total_shares"]      = row[idx_ts];
            fm["a_float_shares"]    = row[idx_af];
            fm["free_float_shares"] = row[idx_ff];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
