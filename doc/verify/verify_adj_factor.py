"""verify_adj_factor: tushare adj_factor vs bigquant cn_stock_real_bar1d.adjust_factor.

两边均累计因子, 但**基期不同**:
  - tushare: 上市日起 (1.0 基)
  - bigquant: 不同基期 (probe 显示平安银行 2024-12-31 为 127.78, tushare 同日约 138 左右)

不能直接比绝对值. 比**每股相邻交易日的相对变化** adj[d] / adj[d-1]:
  - 大部分日 ratio = 1.0 (无除权)
  - 除权日 ratio != 1.0, 两边应严格相等 (除权事件位置 + 幅度对齐)
"""

import _common as c
import pandas as pd


def _per_stock_ratios(df, code_col, date_col, factor_col):
    """按 ts_code 分组, 排序后计算 factor[d]/factor[d-1]; 首日 NaN."""
    df = df.sort_values([code_col, date_col]).reset_index(drop=True)
    df["ratio"] = df.groupby(code_col)[factor_col].transform(lambda s: s / s.shift(1))
    return df


def main():
    print(f"=== verify_adj_factor  [{c.DATE_FROM}, {c.DATE_TO}] ===")

    df_ts = c.load_itf("adj_factor")
    df_ts = df_ts.rename(columns={"trade_date": "date_str", "ts_code": "instrument"})
    df_ts["date_str"] = df_ts["date_str"].astype(str)
    df_ts = _per_stock_ratios(df_ts, "instrument", "date_str", "adj_factor")
    df_ts = df_ts.rename(columns={"ratio": "ratio_ts"})

    df_bq = c.bq_query_yearly(
        "SELECT date, instrument, adjust_factor FROM cn_stock_real_bar1d",
    )
    df_bq["date_str"] = c.bq_dates_to_str(df_bq["date"])
    df_bq = _per_stock_ratios(df_bq, "instrument", "date_str", "adjust_factor")
    df_bq = df_bq.rename(columns={"ratio": "ratio_bq"})

    ts_keys = set(zip(df_ts["date_str"], df_ts["instrument"]))
    bq_keys = set(zip(df_bq["date_str"], df_bq["instrument"]))
    c.diff_sets("PK (date, ts_code)", ts_keys, bq_keys)

    merged = df_ts[["date_str", "instrument", "ratio_ts"]].merge(
        df_bq[["date_str", "instrument", "ratio_bq"]],
        on=["date_str", "instrument"], how="inner",
    )

    # 重点看 ratio != 1.0 的行 (除权事件)
    ts_ev = merged[(merged["ratio_ts"].notna()) & ((merged["ratio_ts"] - 1.0).abs() > 1e-6)]
    bq_ev = merged[(merged["ratio_bq"].notna()) & ((merged["ratio_bq"] - 1.0).abs() > 1e-6)]
    print(f"  除权事件: tushare={len(ts_ev)}, bigquant={len(bq_ev)}")

    bad = c.diff_values("adj 比值 (除权对齐)", merged, "ratio_ts", "ratio_bq", eps=1e-5)

    passed = (ts_keys == bq_keys) and bad.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
