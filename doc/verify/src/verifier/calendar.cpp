// verify_calendar: tushare calendar (SSE/SZSE is_open=1) vs bigquant trading_days (market_code='CN').
// 仅 date 维, 无 ts_code; 用 "_" 占位 instrument.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

#include <iostream>
#include <string>
#include <unordered_set>

namespace verify::run {

void calendar(const Ctx &ctx) {
    diff::MultiYearReport rep("calendar", ctx.year_from, ctx.year_to, /*specs*/ {});

    // tushare: 全期 calendar.json: 取 exchange in {SSE,SZSE} && is_open==1 的 cal_date
    auto recs = ts::load_itf(ctx.data_dir, "calendar", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    std::unordered_set<std::string> ts_dates;
    for (auto &r : recs) {
        auto it_ex = r.fields.find("exchange");
        auto it_op = r.fields.find("is_open");
        auto it_cd = r.fields.find("cal_date");
        if (it_ex == r.fields.end() || it_op == r.fields.end() || it_cd == r.fields.end()) continue;
        if (it_ex->second != "SSE" && it_ex->second != "SZSE") continue;
        if (it_op->second != "1") continue;
        ts_dates.insert(it_cd->second);
    }
    std::cerr << "  [calendar] ts open dates (SSE∪SZSE): " << ts_dates.size() << "\n";
    for (auto &d : ts_dates) {
        rep.add_ts("_", d, {});
    }

    // bigquant: SELECT date FROM trading_days WHERE market_code='CN'  (按年分段)
    auto tables = bq::query_yearly(
        "SELECT date FROM trading_days WHERE market_code='CN'",
        ctx.year_from, ctx.year_to, "trading_days");
    std::size_t bq_total = 0;
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            rep.add_bq("_", d, {});
            ++bq_total;
        }
    }
    std::cerr << "  [calendar] bq dates: " << bq_total << "\n";

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
