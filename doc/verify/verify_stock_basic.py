"""verify_stock_basic: tushare _meta/stock_basic.json vs bigquant cn_stock_basic_info.

无 date 维, 单次静态查询.

字段映射:
  ts_code      ↔ instrument
  list_date    ↔ list_date
  delist_date  ↔ delist_date
  exchange     ↔ exchange      (tushare 'SSE'/'SZSE'/'BSE'; bigquant 'SSE'/'SZSE'/'BSE')
  market       ↔ list_sector (TINYINT 编码, 映射需确认)
"""

import _common as c
import pandas as pd


def main():
    print(f"=== verify_stock_basic ===")

    sb = c.load_meta("stock_basic.json")
    df_ts = pd.DataFrame(sb)
    df_ts = df_ts.rename(columns={"ts_code": "instrument"})
    df_ts["list_date_str"] = df_ts["list_date"].astype(str).where(df_ts["list_date"].notna(), None)
    df_ts["delist_date_str"] = df_ts["delist_date"].astype(str).where(df_ts["delist_date"].notna(), None)

    df_bq = c.bq_query(
        "SELECT instrument, list_date, delist_date, exchange, list_sector FROM cn_stock_basic_info",
        full_scan=True,
    )
    df_bq["list_date_str"] = c.bq_dates_to_str(df_bq["list_date"])
    df_bq["delist_date_str"] = c.bq_dates_to_str(df_bq["delist_date"])
    # bigquant NaT → 'NaT' 字符串, 转 None
    df_bq.loc[df_bq["list_date_str"] == "NaT", "list_date_str"] = None
    df_bq.loc[df_bq["delist_date_str"] == "NaT", "delist_date_str"] = None

    ts_set = set(df_ts["instrument"])
    bq_set = set(df_bq["instrument"])
    c.diff_sets("ts_code (含已退市)", ts_set, bq_set)

    merged = df_ts.merge(df_bq, on="instrument", how="inner", suffixes=("_ts", "_bq"))
    print(f"  inner merge: {len(merged)} 行")

    # list_date
    bad_ld = merged[merged["list_date_str_ts"] != merged["list_date_str_bq"]]
    print(f"  [list_date] 不一致 {len(bad_ld)}")
    if len(bad_ld):
        print(bad_ld[["instrument", "list_date_str_ts", "list_date_str_bq"]].head(10).to_string(index=False))

    # delist_date
    bad_dd = merged[merged["delist_date_str_ts"] != merged["delist_date_str_bq"]]
    print(f"  [delist_date] 不一致 {len(bad_dd)}")
    if len(bad_dd):
        print(bad_dd[["instrument", "delist_date_str_ts", "delist_date_str_bq"]].head(10).to_string(index=False))

    # exchange
    bad_ex = merged[merged["exchange_ts"] != merged["exchange_bq"]]
    print(f"  [exchange] 不一致 {len(bad_ex)}")
    if len(bad_ex):
        print(bad_ex[["instrument", "exchange_ts", "exchange_bq"]].head(10).to_string(index=False))

    # market vs list_sector: 映射不确定, 仅打印分布对照
    print(f"  [market 分布 tushare]  {df_ts['market'].value_counts().to_dict()}")
    print(f"  [list_sector 分布 bigquant]  {df_bq['list_sector'].value_counts(dropna=False).to_dict()}")

    passed = (ts_set == bq_set) and bad_ld.empty and bad_dd.empty and bad_ex.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
