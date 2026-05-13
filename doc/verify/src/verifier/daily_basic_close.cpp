// verify_daily_basic_close: tushare daily_basic.close vs bigquant cn_stock_real_bar1d.close.
// 两边均未复权; 直接浮点比, eps=1e-4. turnover_rate vs turn 单位口径未定, 暂不比.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

namespace verify::run {

void daily_basic_close(const Ctx &ctx) {
    std::vector<diff::FieldSpec> specs = {
        {"close", true, 1.0, 1e-4},
    };
    diff::MultiYearReport rep("daily_basic_close", ctx.year_from, ctx.year_to, specs);

    auto recs = ts::load_itf(ctx.data_dir, "daily_basic", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        auto it_close = r.fields.find("close");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        diff::FieldMap fm;
        if (it_close != r.fields.end()) fm["close"] = it_close->second;
        // turnover_rate 单位未确认, 仅附 diagnostics
        auto it_tr = r.fields.find("turnover_rate");
        if (it_tr != r.fields.end()) fm["turnover_rate"] = it_tr->second;
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, close, turn FROM cn_stock_real_bar1d",
        ctx.year_from, ctx.year_to, "cn_stock_real_bar1d.close");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_cl   = t.idx("close");
        int idx_tn   = t.idx("turn");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["close"] = row[idx_cl];
            fm["turn"]  = row[idx_tn];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
