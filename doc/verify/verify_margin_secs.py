"""verify_margin_secs: tushare margin_secs vs bigquant cn_stock_static_data.crd_buy_flag=1.

tushare margin_secs 是当日全市场两融标的名单 (含 A 股 + ETF).
bigquant cn_stock_static_data 仅 A 股, 用 crd_buy_flag=1 表示可融资买入 (= 两融名单).

口径: 用 _meta/stock_basic 过滤掉 tushare 中的 ETF (非股票 ts_code), 然后比 PK 集合.
"""

import _common as c


def main():
    print(f"=== verify_margin_secs  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    # tushare: 过滤掉 ETF (仅留 stock_basic 中的 ts_code)
    sb = c.load_meta("stock_basic.json")
    stock_codes = set(r["ts_code"] for r in sb)

    df_ts = c.load_itf("margin_secs")
    df_ts["trade_date"] = df_ts["trade_date"].astype(str)
    n_raw = len(df_ts)
    df_ts = df_ts[df_ts["ts_code"].isin(stock_codes)]
    print(f"  tushare raw {n_raw} → 过滤 ETF 后 {len(df_ts)} 行 (仅 A 股)")
    ts_keys = set(zip(df_ts["trade_date"], df_ts["ts_code"]))

    # bigquant: 按年分段 (cn_stock_static_data 表全期超 200MB)
    df_bq = c.bq_query_yearly(
        "SELECT date, instrument FROM cn_stock_static_data WHERE crd_buy_flag=1",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))

    only_ts, only_bq = c.diff_sets("两融名单 PK", ts_keys, bq_keys)

    passed = (not only_ts) and (not only_bq)
    c.finish(passed)


if __name__ == "__main__":
    main()
