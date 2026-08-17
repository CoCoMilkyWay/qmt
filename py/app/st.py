"""统计 cn_stock_status 月度 parquet 的 st_status / is_risk_warning 分布与转移.

目的: 摸清 BigQuant cn_stock_status 的状态字段分布, 验证 cpp/src/feature/feature.cpp
里 risk_warn 4 态派生 (0=正常, 1=ST, 2=*ST, 3=退市整理期) 的数据基础:
  - st_status (int8: 0=正常, 1=ST, 2=*ST) 频次
  - is_risk_warning (int8: 0/1) 频次
  - 派生 risk_warn 4 态频次 (st_status 1/2 → 1/2; st_status==0 ∧ rw!=0 → 3)
  - per-instrument 状态转移频次 (按 date 升序, prev_state -> cur_state)
"""

import glob
import os
from collections import Counter, defaultdict

import pandas as pd

TOP_N = 50
SAMPLES_PER_TRANS = 3


def derive_risk_warn(st_status: int, is_risk_warning: int) -> int:
    if st_status == 1:
        return 1
    if st_status == 2:
        return 2
    if st_status == 0 and is_risk_warning != 0:
        return 3
    return 0


def _print_dist(title, counter, total):
    print(f"=== {title} ===")
    print(f"  总数: {total}  种类: {len(counter)}")
    for k, v in counter.most_common(TOP_N):
        label = "<null>" if k is None or pd.isna(k) else str(k)
        pct = v * 100.0 / total if total else 0.0
        print(f"  {label:<12}  {v:>8}  ({pct:5.2f}%)")
    print()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/../..")
    assert os.path.isdir("data"), "data/ not found at repo root"

    paths = sorted(glob.glob("data/*-*/cn_stock_status.parquet"))
    assert paths, "未发现 cn_stock_status.parquet 月度分片"
    print(f"月度分片数: {len(paths)}\n")

    dfs = []
    for p in paths:
        df = pd.read_parquet(p, columns=["date", "instrument", "st_status", "is_risk_warning"])
        dfs.append(df)
    df = pd.concat(dfs, ignore_index=True)
    total = len(df)
    print(f"总记录数: {total}\n")

    df["risk_warn"] = df.apply(
        lambda r: derive_risk_warn(int(r["st_status"]), int(r["is_risk_warning"])), axis=1
    )

    _print_dist("st_status (0=正常, 1=ST, 2=*ST)", df["st_status"].value_counts(dropna=False), total)
    _print_dist("is_risk_warning (0/1)", df["is_risk_warning"].value_counts(dropna=False), total)
    _print_dist(
        "派生 risk_warn (0=正常, 1=ST, 2=*ST, 3=退市整理期)",
        df["risk_warn"].value_counts(dropna=False),
        total,
    )

    print("=== per-instrument 状态转移频次 (按 date 升序) ===")
    df = df.sort_values(["instrument", "date"])
    trans = Counter()
    trans_samples = defaultdict(list)
    for code, grp in df.groupby("instrument"):
        prev = None
        for _, row in grp.iterrows():
            cur = int(row["risk_warn"])
            if prev is None:
                key = f"INIT->{cur}"
            else:
                key = f"{prev}->{cur}"
            trans[key] += 1
            if len(trans_samples[key]) < SAMPLES_PER_TRANS:
                trans_samples[key].append((code, row["date"]))
            prev = cur
    for key, cnt in trans.most_common(TOP_N):
        print(f"  {key:<12}  {cnt:>8}")
        for code, d in trans_samples[key]:
            print(f"    sample: {code:<12}  {pd.Timestamp(d).date()}")
    print()


if __name__ == "__main__":
    main()
