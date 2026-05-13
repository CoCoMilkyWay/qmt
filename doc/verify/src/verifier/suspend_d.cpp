// verify_suspend_d: tushare suspend_d vs bigquant cn_stock_suspend.
// 字段语义两边不一致 (S/R 边界 vs suspend_period 天数), 仅比 PK 集合.
// 双方均保留 suspend_type / suspend_period 等附加字段在输出供人工排查.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

namespace verify::run {

void suspend_d(const Ctx &ctx) {
    diff::MultiYearReport rep("suspend_d", ctx.year_from, ctx.year_to, /*specs*/ {});

    auto recs = ts::load_itf(ctx.data_dir, "suspend_d", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        diff::FieldMap fm;
        for (auto &kv : r.fields) {
            if (kv.first == "trade_date" || kv.first == "ts_code") continue;
            fm[kv.first] = kv.second;
        }
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, suspend_period, suspend_reason FROM cn_stock_suspend",
        ctx.year_from, ctx.year_to, "cn_stock_suspend");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_per = t.idx("suspend_period");
        int idx_rsn = t.idx("suspend_reason");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["suspend_period"] = row[idx_per];
            fm["suspend_reason"] = row[idx_rsn];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
