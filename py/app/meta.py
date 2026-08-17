"""统计 data/_meta/cn_stock_basic_info.parquet 的各类别字段分布.

BigQuant cn_stock_basic_info 字段 (Static, 无 date 维, 全量快照):
  exchange / list_sector (int8: 1=主板/2=创业板/3=科创板/4=北交所) /
  security_type / industry / corp_nature / corp_scale
"""

import os

import pandas as pd

PATH = "data/_meta/cn_stock_basic_info.parquet"

FIELDS = [
    "exchange",
    "list_sector",
    "security_type",
    "industry",
    "corp_nature",
    "corp_scale",
]

TOP_N = 30


def _print_dist(title, counter, total_records):
    total = sum(counter.values())
    n_kinds = len(counter)
    n_null = counter.get(None, 0) + counter.get(float("nan"), 0)
    n_valued = n_kinds - (1 if None in counter else 0)
    print(f"=== {title} ===")
    print(f"  总数: {total}  种类: {n_kinds} (有值 {n_valued}, null {n_null})")
    items = counter.most_common()
    for k, v in items[:TOP_N]:
        label = "<null>" if k is None or pd.isna(k) else str(k)
        pct = v * 100.0 / total_records if total_records else 0.0
        print(f"  {label:<24}  {v:>5}  ({pct:5.2f}%)")
    if len(items) > TOP_N:
        rest = sum(v for _, v in items[TOP_N:])
        print(f"  ... 其余 {len(items) - TOP_N} 项合计 {rest}")
    print()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/../..")
    assert os.path.exists(PATH), f"meta not found: {PATH}"

    df = pd.read_parquet(PATH)
    total_records = len(df)
    print(f"总记录数: {total_records}\n")

    for field in FIELDS:
        assert field in df.columns, f"字段 {field} 不在记录中"
        counter = df[field].value_counts(dropna=False)
        _print_dist(field, counter, total_records)


if __name__ == "__main__":
    main()
