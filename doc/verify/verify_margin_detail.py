"""verify_margin_detail: tushare margin_detail vs bigquant cn_stock_margin_trading_detail.

PK = (trade_date, ts_code).
字段映射:
  rzye  ↔ financing_balance               (融资余额, 元)
  rqye  ↔ securities_lending_balance      (融券余额, 元)
  rzmre ↔ financing_purchase              (融资买入额, 元)
  rzche ↔ financing_repayment             (融资偿还额, 元)
  rzrqye ↔ margin_trading_balance         (融资融券总余额, 元)
"""

import _common as c


def main():
    print(f"=== verify_margin_detail  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("margin_detail")
    df_ts = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"})
    df_ts["date_str"] = df_ts["date_str"].astype(str)

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, financing_balance, securities_lending_balance, "
        "financing_purchase, financing_repayment, margin_trading_balance "
        "FROM cn_stock_margin_trading_detail",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])

    ts_keys = set(zip(df_ts["date_str"], df_ts["instrument"]))
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))
    c.diff_sets("PK (date, ts_code)", ts_keys, bq_keys)

    merged = df_ts.merge(
        df_bq[["date_str", "instrument", "financing_balance",
               "securities_lending_balance", "financing_purchase",
               "financing_repayment", "margin_trading_balance"]],
        on=["date_str", "instrument"], how="inner",
    )

    bad1 = c.diff_values("rzye ↔ financing_balance", merged, "rzye", "financing_balance", eps=1e-4)
    bad2 = c.diff_values("rqye ↔ securities_lending_balance", merged, "rqye", "securities_lending_balance", eps=1e-4)
    bad3 = c.diff_values("rzmre ↔ financing_purchase", merged, "rzmre", "financing_purchase", eps=1e-4)
    bad4 = c.diff_values("rzche ↔ financing_repayment", merged, "rzche", "financing_repayment", eps=1e-4)
    bad5 = c.diff_values("rzrqye ↔ margin_trading_balance", merged, "rzrqye", "margin_trading_balance", eps=1e-4)

    passed = (ts_keys == bq_keys) and all(b.empty for b in [bad1, bad2, bad3, bad4, bad5])
    c.finish(passed)


if __name__ == "__main__":
    main()
