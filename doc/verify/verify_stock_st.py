"""verify_stock_st: tushare stock_st vs bigquant cn_stock_status.

tushare stock_st 是每日 ST 全量快照 (始 2016-01-01), name 含 '*' 区分 ST/*ST.
bigquant cn_stock_status.st_status: TINYINT 0=正常 / 1=ST / 2=*ST.

校验:
  - PK (date, ts_code) 集合: tushare 全集 vs bigquant st_status > 0 子集
  - 状态: tushare name 含 '*' ↔ bq st_status=2, tushare name 不含 '*' ↔ bq st_status=1
"""

import _common as c

# tushare stock_st 始 2016-01-01, 早于此跳过
ST_DATE_FROM = "2016-01-01"


def main():
    print(f"=== verify_stock_st  [{ST_DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("stock_st", date_from=ST_DATE_FROM)
    df_ts["trade_date"] = df_ts["trade_date"].astype(str)
    df_ts["ts_st"] = df_ts["name"].str.contains(r"\*", regex=True).astype(int) + 1   # ST=1, *ST=2
    ts_keys = set(zip(df_ts["trade_date"], df_ts["ts_code"]))

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, st_status FROM cn_stock_status WHERE st_status > 0",
        date_from=ST_DATE_FROM,
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))

    c.diff_sets("ST 名单 PK", ts_keys, bq_keys)

    # 状态细分 (ST/*ST) 一致性
    merged = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"}).merge(
        df_bq[["date_str", "instrument", "st_status"]],
        on=["date_str", "instrument"], how="inner",
    )
    bad = c.diff_values("ST 三态 (1=ST,2=*ST)", merged, "ts_st", "st_status", eps=1e-9)

    passed = (ts_keys == bq_keys) and bad.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
