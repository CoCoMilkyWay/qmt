"""统计 data/_meta/stock_basic.json 的各类别字段分布"""

import json
import os
from collections import Counter

PATH = "data/_meta/stock_basic.json"

FIELDS = [
    "area",
    "industry",
    "market",
    "exchange",
    "curr_type",
    "list_status",
    "is_hs",
    "act_ent_type",
]

TOP_N = 30


def _print_dist(title, counter, total_records):
    total = sum(counter.values())
    n_kinds = len(counter)
    n_null = counter.get(None, 0)
    n_valued = n_kinds - (1 if None in counter else 0)
    print(f"=== {title} ===")
    print(f"  总数: {total}  种类: {n_kinds} (有值 {n_valued}, null {n_null})")
    items = counter.most_common()
    shown = items[:TOP_N]
    for k, v in shown:
        label = "<null>" if k is None else str(k)
        pct = v * 100.0 / total_records if total_records else 0.0
        print(f"  {label:<12}  {v:>5}  ({pct:5.2f}%)")
    if len(items) > TOP_N:
        rest = sum(v for _, v in items[TOP_N:])
        print(f"  ... 其余 {len(items) - TOP_N} 项合计 {rest}")
    print()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/../..")
    assert os.path.exists(PATH), f"meta not found: {PATH}"

    with open(PATH, "r", encoding="utf-8") as f:
        data = json.load(f)
    assert isinstance(data, list) and len(data) > 0

    total_records = len(data)
    print(f"总记录数: {total_records}\n")

    for field in FIELDS:
        assert field in data[0], f"字段 {field} 不在记录中"
        counter = Counter(item.get(field) for item in data)
        _print_dist(field, counter, total_records)


if __name__ == "__main__":
    main()
