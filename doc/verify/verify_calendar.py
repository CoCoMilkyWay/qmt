"""verify_calendar: tushare calendar (SSE/SZSE is_open=1) vs bigquant trading_days (market_code='CN').

集合一致性: A 股交易日 date 集合.
"""

import _common as c


def main():
    print(f"=== verify_calendar  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    # tushare: 全期落地, 取 exchange ∈ {SSE, SZSE} ∩ is_open=1, 合 SSE ∪ SZSE
    df_ts = c.load_itf("calendar")
    ts_open = df_ts[(df_ts["exchange"].isin(["SSE", "SZSE"])) & (df_ts["is_open"] == 1)]
    ts_dates = set(ts_open["cal_date"].astype(str))

    # bigquant: trading_days where market_code='CN' (= A 股市场)
    df_bq = c.bq_query(
        "SELECT date FROM trading_days WHERE market_code='CN'",
        filters={"date": [c.DATE_FROM, c.DATE_TO]},
    )
    bq_dates = set(c.bq_dates_to_str(df_bq["date"]))

    only_ts, only_bq = c.diff_sets("A 股交易日", ts_dates, bq_dates)

    # 同时校验 SSE 和 SZSE 各自集合相等 (sanity: 项目假设 D=SSE∪SZSE)
    sse = set(df_ts[(df_ts["exchange"] == "SSE") & (df_ts["is_open"] == 1)]["cal_date"].astype(str))
    szse = set(df_ts[(df_ts["exchange"] == "SZSE") & (df_ts["is_open"] == 1)]["cal_date"].astype(str))
    print(f"  [SSE vs SZSE] SSE={len(sse)}  SZSE={len(szse)}  对称差={len(sse ^ szse)}")
    if sse ^ szse:
        print(f"    样例: {sorted(sse ^ szse)[:10]}")

    passed = (not only_ts) and (not only_bq) and (not (sse ^ szse))
    c.finish(passed)


if __name__ == "__main__":
    main()
