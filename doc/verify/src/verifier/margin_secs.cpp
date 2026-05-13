// verify_margin_secs: tushare margin_secs vs bigquant cn_stock_static_data.crd_buy_flag=1.
// tushare margin_secs 含 ETF, 需用 _meta/stock_basic 过滤为仅 A 股, 再比 PK 集合.

#include "common/bq.hpp"
#include "common/diff.hpp"
#include "common/ts_loader.hpp"
#include "common/util.hpp"
#include "verifier.hpp"

#include "package/yyjson/yyjson.h"

#include <cassert>
#include <iostream>
#include <unordered_set>

namespace verify::run {

void margin_secs(const Ctx &ctx) {
    diff::MultiYearReport rep("margin_secs", ctx.year_from, ctx.year_to, /*specs*/ {});

    // 用 _meta/stock_basic.json 提取 A 股 ts_code 集合, 过滤 ETF
    std::string sb_buf = ts::read_meta_raw(ctx.data_dir, "stock_basic.json");
    yyjson_doc *doc = yyjson_read(sb_buf.data(), sb_buf.size(), 0);
    assert(doc && "stock_basic.json 解析失败");
    yyjson_val *root = yyjson_doc_get_root(doc);
    assert(yyjson_is_arr(root));
    std::unordered_set<std::string> stock_codes;
    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(root, i, n, item) {
        yyjson_val *v = yyjson_obj_get(item, "ts_code");
        if (v && yyjson_is_str(v)) stock_codes.insert(yyjson_get_str(v));
    }
    yyjson_doc_free(doc);
    std::cerr << "  [margin_secs] stock_basic A 股: " << stock_codes.size() << "\n";

    auto recs = ts::load_itf(ctx.data_dir, "margin_secs", ctx.year_from, ctx.year_to,
                             ctx.sample_dates);
    std::size_t kept = 0;
    for (auto &r : recs) {
        auto it_d = r.fields.find("trade_date");
        auto it_c = r.fields.find("ts_code");
        if (it_d == r.fields.end() || it_c == r.fields.end()) continue;
        if (!stock_codes.count(it_c->second)) continue;  // 滤掉 ETF
        rep.add_ts(it_c->second, it_d->second, {});
        ++kept;
    }
    std::cerr << "  [margin_secs] ts kept (filter ETF): " << kept << "/" << recs.size() << "\n";

    auto tables = bq::query_yearly(
        "SELECT date, instrument FROM cn_stock_static_data WHERE crd_buy_flag=1",
        ctx.year_from, ctx.year_to, "cn_stock_static_data");
    for (auto &t : tables) {
        int idx_date = t.idx("date");
        int idx_inst = t.idx("instrument");
        for (auto &row : t.rows) {
            std::string d = util::to_yyyymmdd(row[idx_date]);
            rep.add_bq(row[idx_inst], d, {});
        }
    }

    rep.write_all(ctx.out_dir);
}

} // namespace verify::run
