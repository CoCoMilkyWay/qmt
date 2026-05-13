"""verify_daily_basic_close: tushare daily_basic.close/turnover_rate vs bigquant cn_stock_real_bar1d.

实测两边 close 均**未复权** (tushare 2024-12-30 平安银行 close=11.95 ↔ bigquant 2024-12-31 pre_close=11.95).

字段映射:
  daily_basic.close          ↔ cn_stock_real_bar1d.close   (元/股, 未复权)
  daily_basic.turnover_rate  ↔ cn_stock_real_bar1d.turn    (% vs ratio? 注意单位!)

注: bigquant turn 实测 2024-12-31 平安银行 = 0.007602 (probe[7]), tushare turnover_rate = 0.6966 (%).
   两者比值 ≈ 91, 不是 100. 需要先看几行确认单位/口径再定 scale.
   保守起见, 默认 turn 是 ratio 而 turnover_rate 是 %, scale = 1/100. 跑出来再调.
"""

import _common as c


def main():
    print(f"=== verify_daily_basic_close  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("daily_basic")
    df_ts = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"})
    df_ts["date_str"] = df_ts["date_str"].astype(str)

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, close, turn FROM cn_stock_real_bar1d",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])

    ts_keys = set(zip(df_ts["date_str"], df_ts["instrument"]))
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))
    c.diff_sets("PK (date, ts_code)", ts_keys, bq_keys)

    merged = df_ts[["date_str", "instrument", "close", "turnover_rate"]].merge(
        df_bq[["date_str", "instrument", "close", "turn"]],
        on=["date_str", "instrument"], how="inner", suffixes=("_ts", "_bq"),
    )

    # close: 元/股, 2 位小数, eps 略宽
    bad_close = c.diff_values("close ↔ close (未复权)", merged, "close_ts", "close_bq", eps=1e-4)

    # turnover_rate: tushare 是 % (e.g. 0.6966), bigquant turn 是 ratio (e.g. 0.007602? 待确认)
    # 比值假设: bq.turn × 100 == ts.turnover_rate? 不确定, 先用 scale=1 跑看分布
    bad_turn = c.diff_values("turnover_rate ↔ turn (口径未定)", merged,
                              "turnover_rate", "turn", eps=1e-3)

    passed = (ts_keys == bq_keys) and bad_close.empty
    # turn 单位未定, 不进 passed 条件 (报告即可)
    print(f"  (turn 字段口径待确认, 不进 PASS 条件)")
    c.finish(passed)


if __name__ == "__main__":
    main()
