"""统计 data/**/DD/st.json 的 st_tpye 分布与状态语义.

目的: 当前 risk_warn 状态机仅看 name 含 'ST' 子串, 粒度过粗.
     先把数据摸清楚 (有哪些 st_tpye / 与 name 的关系 / 相邻事件转移), 再决定 parse.
"""

import glob
import json
import os
from collections import Counter, defaultdict

TOP_N = 50
SAMPLES_PER_TYPE = 3


def _name_class(name: str) -> str:
    if not name:
        return "<empty>"
    has_star = "*ST" in name
    has_st = "ST" in name
    if has_star:
        return "*ST"
    if has_st:
        return "ST"
    return "无ST"


def _print_dist(title, counter, total):
    print(f"=== {title} ===")
    print(f"  总数: {total}  种类: {len(counter)}")
    for k, v in counter.most_common(TOP_N):
        label = "<null>" if k is None else str(k)
        pct = v * 100.0 / total if total else 0.0
        print(f"  {label:<24}  {v:>6}  ({pct:5.2f}%)")
    print()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/../..")
    assert os.path.isdir("data"), "data/ not found at repo root"

    paths = sorted(glob.glob("data/*/*/*/st.json"))
    assert paths, "未发现 st.json"
    print(f"st.json 文件数: {len(paths)}\n")

    records = []
    for p in paths:
        with open(p, "r", encoding="utf-8") as f:
            arr = json.load(f)
        assert isinstance(arr, list)
        records.extend(arr)
    assert records, "无记录"
    total = len(records)
    print(f"总记录数: {total}\n")

    type_counter = Counter(r.get("st_tpye") for r in records)
    _print_dist("st_tpye 全局频次", type_counter, total)

    type_to_namecls = defaultdict(Counter)
    type_to_samples = defaultdict(list)
    for r in records:
        t = r.get("st_tpye")
        nc = _name_class(r.get("name", ""))
        type_to_namecls[t][nc] += 1
        if len(type_to_samples[t]) < SAMPLES_PER_TYPE:
            type_to_samples[t].append(r)

    print("=== 每种 st_tpye 下 name 的 ST/*ST 子串分布 + 样例 ===")
    for t, _ in type_counter.most_common():
        nc = type_to_namecls[t]
        nc_str = "  ".join(f"{k}={v}" for k, v in nc.most_common())
        print(f"[{t}]  name分布: {nc_str}")
        for s in type_to_samples[t]:
            reason = (s.get("st_reason") or "")[:40].replace("\n", " ")
            print(
                f"    {s.get('ts_code'):>10}  pub={s.get('pub_date')}  "
                f"imp={s.get('imp_date')}  name={s.get('name'):<12}  "
                f"reason={reason}"
            )
        print()

    print("=== 相邻事件转移 (按 ts_code+imp_date 排序; prev_name_cls -> name_cls 频次 by st_tpye) ===")
    by_code = defaultdict(list)
    for r in records:
        by_code[r.get("ts_code")].append(r)
    trans_by_type = defaultdict(Counter)
    init_by_type = defaultdict(Counter)
    for code, rs in by_code.items():
        rs.sort(key=lambda x: (x.get("imp_date") or "", x.get("pub_date") or ""))
        prev_nc = None
        for r in rs:
            t = r.get("st_tpye")
            cur_nc = _name_class(r.get("name", ""))
            if prev_nc is None:
                init_by_type[t][f"INIT->{cur_nc}"] += 1
            else:
                trans_by_type[t][f"{prev_nc}->{cur_nc}"] += 1
            prev_nc = cur_nc

    for t, _ in type_counter.most_common():
        merged = Counter()
        merged.update(init_by_type.get(t, {}))
        merged.update(trans_by_type.get(t, {}))
        merged_str = "  ".join(f"{k}={v}" for k, v in merged.most_common())
        print(f"[{t}]  {merged_str}")
    print()

    print("=== reason 关键词 (按 st_tpye 抽 top-5 reason 原文) ===")
    type_to_reasons = defaultdict(Counter)
    for r in records:
        type_to_reasons[r.get("st_tpye")][r.get("st_reason") or ""] += 1
    for t, _ in type_counter.most_common():
        c = type_to_reasons[t]
        print(f"[{t}]")
        for reason, cnt in c.most_common(5):
            print(f"  ({cnt:>4}) {reason[:80]}")
        print()


if __name__ == "__main__":
    main()
