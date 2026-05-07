"""统计 data/_meta/stock_basic.json 的 area / industry 分布"""

import json
import os
from collections import Counter

PATH = "data/_meta/stock_basic.json"


def _print_dist(title, counter):
    total = sum(counter.values())
    n_kinds = len(counter)
    n_null = counter.get(None, 0)
    n_valued = n_kinds - (1 if None in counter else 0)
    print(f"=== {title} ===")
    print(f"  总数: {total}  种类: {n_kinds} (有值 {n_valued}, null {n_null})")
    for k, v in counter.most_common():
        label = "<null>" if k is None else k
        print(f"  {label:<10}  {v:>5}")
    print()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/../..")
    assert os.path.exists(PATH), f"meta not found: {PATH}"

    with open(PATH, "r", encoding="utf-8") as f:
        data = json.load(f)
    assert isinstance(data, list) and len(data) > 0

    print(f"总记录数: {len(data)}\n")

    areas = Counter(item.get("area") for item in data)
    industries = Counter(item.get("industry") for item in data)

    _print_dist("area", areas)
    _print_dist("industry", industries)


if __name__ == "__main__":
    main()
