"""verify_stk_limit: tushare stk_limit (up_limit/down_limit) vs bigquant cn_stock_limit_price.

PK = (trade_date, ts_code). 字段直接一一对应, 均未复权.
"""

import _common as c


def main():
    print(f"=== verify_stk_limit  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("stk_limit")
    df_ts = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"})
    df_ts["date_str"] = df_ts["date_str"].astype(str)

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, upper_limit, lower_limit FROM cn_stock_limit_price",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])

    # 集合一致性 (PK)
    ts_keys = set(zip(df_ts["date_str"], df_ts["instrument"]))
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))
    c.diff_sets("PK (date, ts_code)", ts_keys, bq_keys)

    # 字段值比对 (inner join)
    merged = df_ts.merge(
        df_bq[["date_str", "instrument", "upper_limit", "lower_limit"]],
        on=["date_str", "instrument"], how="inner",
    )
    bad_up = c.diff_values("up_limit ↔ upper_limit", merged, "up_limit", "upper_limit")
    bad_dn = c.diff_values("down_limit ↔ lower_limit", merged, "down_limit", "lower_limit")

    passed = (ts_keys == bq_keys) and bad_up.empty and bad_dn.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
