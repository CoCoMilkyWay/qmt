"""verify_namechange: tushare _meta/namechange.json vs bigquant cn_stock_name_change.

tushare 嵌套: {ts_code: [{name, start_date, change_reason}, ...]} (内层升序, 无 end_date 防未来信息).
bigquant 长表: (instrument, start_date, end_date, name).

校验: 按 (instrument, start_date) merge 比 name; end_date 不验 (tushare 没有).
"""

import _common as c
import pandas as pd


def main():
    print(f"=== verify_namechange ===")

    nc = c.load_meta("namechange.json")
    rows = []
    for ts_code, segs in nc.items():
        for s in segs:
            rows.append({
                "instrument": ts_code,
                "start_date": s["start_date"],
                "name": s["name"],
            })
    df_ts = pd.DataFrame(rows)
    print(f"  tushare 长表 {len(df_ts)} 段, ts_code {df_ts['instrument'].nunique()} 个")

    df_bq = c.bq_query(
        "SELECT instrument, start_date, name FROM cn_stock_name_change",
        full_scan=True,
    )
    df_bq["start_date"] = c.bq_dates_to_str(df_bq["start_date"])
    print(f"  bigquant 长表 {len(df_bq)} 段, instrument {df_bq['instrument'].nunique()} 个")

    ts_keys = set(zip(df_ts["instrument"], df_ts["start_date"]))
    bq_keys = set(zip(df_bq["instrument"], df_bq["start_date"]))
    c.diff_sets("PK (instrument, start_date)", ts_keys, bq_keys)

    merged = df_ts.merge(df_bq, on=["instrument", "start_date"], how="inner",
                          suffixes=("_ts", "_bq"))
    bad = merged[merged["name_ts"] != merged["name_bq"]]
    print(f"  [name 字段] 比对 {len(merged)}  一致 {len(merged) - len(bad)}  不一致 {len(bad)}")
    if len(bad):
        print(bad[["instrument", "start_date", "name_ts", "name_bq"]].head(10).to_string(index=False))

    passed = (ts_keys == bq_keys) and bad.empty
    c.finish(passed)


if __name__ == "__main__":
    main()
