"""verify_suspend_d: tushare suspend_d vs bigquant cn_stock_suspend.

字段不直接可比 (tushare suspend_type ∈ {S,R} vs bigquant suspend_period [天数]).
仅校验 (date, ts_code) 集合存在性 + 'S' 持续段 (tushare 通过 S/R 边界) 是否落在 bigquant suspend_period 内.

简化口径: 直接比 (date, ts_code) 集合 (tushare 同日同股可能 S+R 各 1 行, 去重后).
"""

import _common as c


def main():
    print(f"=== verify_suspend_d  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("suspend_d")
    df_ts["trade_date"] = df_ts["trade_date"].astype(str)
    ts_keys = set(zip(df_ts["trade_date"], df_ts["ts_code"]))

    df_bq = c.bq_query(
        "SELECT date, instrument, suspend_period, suspend_reason FROM cn_stock_suspend",
        filters={"date": [c.DATE_FROM, c.DATE_TO]},
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))

    print(f"  tushare 原始记录 {len(df_ts)} (含 S/R 边界), 去重 PK 后 {len(ts_keys)}")
    print(f"  bigquant 记录 {len(df_bq)}")

    # tushare S+R 边界 vs bigquant period: tushare 含 S(停盘日) 和 R(复盘日) 两类边界事件,
    # bigquant 只在停牌日列, 复牌当日不列. 先看集合差异规模, 再决定要不要分 S/R.
    c.diff_sets("PK (date, ts_code) 含 S+R 边界", ts_keys, bq_keys)

    # 仅 tushare 的 S (停盘事件)
    ts_s = set(zip(df_ts[df_ts["suspend_type"] == "S"]["trade_date"],
                   df_ts[df_ts["suspend_type"] == "S"]["ts_code"]))
    ts_r = set(zip(df_ts[df_ts["suspend_type"] == "R"]["trade_date"],
                   df_ts[df_ts["suspend_type"] == "R"]["ts_code"]))
    print(f"  tushare 拆分: S(停)={len(ts_s)}  R(复)={len(ts_r)}")
    c.diff_sets("PK 仅 S (停盘事件)", ts_s, bq_keys)

    # 集合差异允许 (S/R 语义不同), 该脚本仅报告不 assert
    print("  (suspend_d 字段语义两边不一致, 仅报告 + sanity, 不 assert fail)")
    c.finish(True)


if __name__ == "__main__":
    main()
