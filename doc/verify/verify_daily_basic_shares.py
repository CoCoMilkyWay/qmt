"""verify_daily_basic_shares: tushare daily_basic.{total/float/free}_share vs bigquant cn_stock_shares.

单位换算: tushare 万股 × 1e4 ↔ bigquant 股.

字段映射:
  total_share × 1e4  ↔ total_shares
  float_share × 1e4  ↔ a_float_shares       (A 股流通股本)
  free_share × 1e4   ↔ free_float_shares
"""

import _common as c


def main():
    print(f"=== verify_daily_basic_shares  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("daily_basic")
    df_ts = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"})
    df_ts["date_str"] = df_ts["date_str"].astype(str)

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, total_shares, a_float_shares, free_float_shares "
        "FROM cn_stock_shares",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])

    ts_keys = set(zip(df_ts["date_str"], df_ts["instrument"]))
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))
    c.diff_sets("PK (date, ts_code)", ts_keys, bq_keys)

    merged = df_ts[["date_str", "instrument", "total_share", "float_share", "free_share"]].merge(
        df_bq[["date_str", "instrument", "total_shares", "a_float_shares", "free_float_shares"]],
        on=["date_str", "instrument"], how="inner",
    )

    # 单位换算 scale=1e4: tushare 万股 → bigquant 股. eps=1e-4 (浮点小数差).
    bad1 = c.diff_values("total_share × 1e4 ↔ total_shares", merged,
                          "total_share", "total_shares", eps=1e-4, scale=1e4)
    bad2 = c.diff_values("float_share × 1e4 ↔ a_float_shares", merged,
                          "float_share", "a_float_shares", eps=1e-4, scale=1e4)
    bad3 = c.diff_values("free_share × 1e4 ↔ free_float_shares", merged,
                          "free_share", "free_float_shares", eps=1e-4, scale=1e4)

    passed = (ts_keys == bq_keys) and bad1.empty and bad2.empty and bad3.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
