// verify_margin_detail: tushare margin_detail vs bigquant cn_stock_margin_trading_detail.
// 字段映射 (元):
//   rzye   ↔ financing_balance
//   rqye   ↔ securities_lending_balance
//   rzmre  ↔ financing_purchase
//   rzche  ↔ financing_repayment
//   rzrqye ↔ margin_trading_balance

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

namespace verify::run {

void margin_detail(const Ctx &ctx) {
    std::vector<diff::FieldSpec> specs = {
        {"financing_balance",          true, 1.0, 1e-4},
        {"securities_lending_balance", true, 1.0, 1e-4},
        {"financing_purchase",         true, 1.0, 1e-4},
        {"financing_repayment",        true, 1.0, 1e-4},
        {"margin_trading_balance",     true, 1.0, 1e-4},
    };
    diff::MultiYearReport rep("margin_detail", ctx.year_from, ctx.year_to, specs);

    auto recs = ts::load_itf(ctx.data_dir, "margin_detail", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        diff::FieldMap fm;
        auto take = [&](const char *src, const char *unified) {
            auto it = r.fields.find(src);
            if (it != r.fields.end()) fm[unified] = it->second;
        };
        take("rzye",   "financing_balance");
        take("rqye",   "securities_lending_balance");
        take("rzmre",  "financing_purchase");
        take("rzche",  "financing_repayment");
        take("rzrqye", "margin_trading_balance");
        rep.add_ts(it_c->second, it_d->second, std::move(fm));
    }

    auto tables = bq::query_yearly(
        "SELECT date, instrument, financing_balance, securities_lending_balance, "
        "financing_purchase, financing_repayment, margin_trading_balance "
        "FROM cn_stock_margin_trading_detail",
        ctx.year_from, ctx.year_to, "cn_stock_margin_trading_detail");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        int idx_fb = t.idx("financing_balance");
        int idx_sl = t.idx("securities_lending_balance");
        int idx_fp = t.idx("financing_purchase");
        int idx_fr = t.idx("financing_repayment");
        int idx_mt = t.idx("margin_trading_balance");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            diff::FieldMap fm;
            fm["financing_balance"]          = row[idx_fb];
            fm["securities_lending_balance"] = row[idx_sl];
            fm["financing_purchase"]         = row[idx_fp];
            fm["financing_repayment"]        = row[idx_fr];
            fm["margin_trading_balance"]     = row[idx_mt];
            rep.add_bq(row[idx_inst], d, std::move(fm));
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
