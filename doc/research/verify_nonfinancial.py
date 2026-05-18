"""验证 data/ 下非财务类特征所需数据是否符合预期 (stdlib only).

非财务特征 → itf 源 → 验证项, 与 cpp/src/feature/{pit,feature}.cpp 对齐:
  bar1d                          → close_raw / adjust_factor / mcap_raw / fmcap_raw / daily_return (hfq 链式, 内部叠 af)
  shares                         → share_raw / fmcap_raw
  limit_price                    → up_lim / dn_lim / limit_up / limit_dn
  status                         → susp / risk_warn
  margin_trading_detail          → is_margin / mr_bal_raw / ms_bal_raw
  industry_component (MonthFirst)+ industry_change   → industry_l1
  _meta/cn_stock_basic_info      → list_age / delist_age / pool_b (exchange/list_sector)
  _meta/trading_days             → D 轴 / floor_date

输出语义:
  [OK]   = 全部样本符合预期
  [WARN] = 少量异常 (打印前 N 条样例 + 占比)
  [FAIL] = 大量异常 / 致命点

用法:
  python doc/research/verify_nonfinancial.py
  python doc/research/verify_nonfinancial.py --year 2024   # 仅扫某年
"""

import argparse
import json
import os
import sys
from collections import Counter, defaultdict
from datetime import date

# ============================================================================
# 路径 & 常量
# ============================================================================

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, "data")
META = os.path.join(DATA, "_meta")

# 与 cpp/include/feature/industry.hpp::SW2021_L1_NAMES (1..31) 完全对齐
SW2021_L1_NAMES = {
    "交通运输", "传媒", "公用事业", "农林牧渔", "医药生物", "商贸零售",
    "国防军工", "基础化工", "家用电器", "建筑材料", "建筑装饰", "房地产",
    "有色金属", "机械设备", "汽车", "煤炭", "环保", "电力设备", "电子",
    "石油石化", "社会服务", "纺织服饰", "综合", "美容护理", "计算机",
    "轻工制造", "通信", "钢铁", "银行", "非银金融", "食品饮料",
}
assert len(SW2021_L1_NAMES) == 31

EXCHANGE_VALUES = {"上海证券交易所", "深圳证券交易所", "北京证券交易所"}
LIST_SECTOR_VALUES = {1, 2, 3, 4}

# ============================================================================
# helpers
# ============================================================================

ANOMALY_HEAD = 5  # 打印前 N 条异常样例


def section(title):
    print()
    print("=" * 78)
    print(title)
    print("=" * 78)


def status(label, total, bad_count, anomalies_preview, threshold_warn=0.001):
    """统一状态打印; bad_count == 0 -> OK; 占比 > threshold_warn -> FAIL else WARN."""
    if bad_count == 0:
        print(f"  [OK]   total={total}  no anomaly")
        return
    ratio = bad_count / max(total, 1)
    tag = "FAIL" if ratio > threshold_warn else "WARN"
    print(f"  [{tag}] total={total}  bad={bad_count}  ratio={ratio:.4%}  {label}")
    for i, s in enumerate(anomalies_preview[:ANOMALY_HEAD]):
        print(f"    sample[{i}]: {s}")


def iter_day_files(itf_name, year_filter=None):
    """yield (yyyymmdd_str, path) over all data/YYYY/MM/DD/<itf_name>.json (升序)."""
    if not os.path.isdir(DATA):
        return
    for y in sorted(os.listdir(DATA)):
        if not y.isdigit() or len(y) != 4:
            continue
        if year_filter and y != year_filter:
            continue
        yp = os.path.join(DATA, y)
        for m in sorted(os.listdir(yp)):
            if not m.isdigit() or len(m) != 2:
                continue
            mp = os.path.join(yp, m)
            for d in sorted(os.listdir(mp)):
                if not d.isdigit() or len(d) != 2:
                    continue
                fp = os.path.join(mp, d, f"{itf_name}.json")
                if os.path.exists(fp):
                    yield (f"{y}{m}{d}", fp)


def load_json(p):
    with open(p) as f:
        return json.load(f)


def is_yyyymmdd(s):
    """8 字符纯数字 + 合法日期."""
    if not isinstance(s, str) or len(s) != 8 or not s.isdigit():
        return False
    try:
        date(int(s[:4]), int(s[4:6]), int(s[6:8]))
        return True
    except ValueError:
        return False


# ============================================================================
# 各 itf 校验
# ============================================================================


def check_basic_info():
    section("[meta] cn_stock_basic_info  (axis A 源 + list_age/delist_age/pool_b)")
    p = os.path.join(META, "cn_stock_basic_info.json")
    if not os.path.exists(p):
        print(f"  [FAIL] {p} missing")
        return None
    arr = load_json(p)
    print(f"  rows={len(arr)}  fields={len(arr[0])}")

    # 1) instrument 唯一性
    counter_ins = Counter(r.get("instrument") for r in arr)
    dup = [k for k, c in counter_ins.items() if c > 1]
    status("instrument duplicates",
           len(arr), len(dup), [f"ins={k} count={counter_ins[k]}" for k in dup[:ANOMALY_HEAD]])

    # 2) list_date 格式
    bad_ld = []
    for r in arr:
        ld = r.get("list_date")
        if ld is None:
            continue
        if not is_yyyymmdd(ld):
            bad_ld.append(f"ins={r.get('instrument')} list_date={ld!r}")
    status("list_date 非 YYYYMMDD", len(arr), len(bad_ld), bad_ld)

    # 3) delist_date 格式
    bad_dd = []
    for r in arr:
        dd = r.get("delist_date")
        if dd is None:
            continue
        if not is_yyyymmdd(dd):
            bad_dd.append(f"ins={r.get('instrument')} delist_date={dd!r}")
    status("delist_date 非 YYYYMMDD", len(arr), len(bad_dd), bad_dd)

    # 4) delist_date >= list_date
    bad_order = []
    for r in arr:
        ld, dd = r.get("list_date"), r.get("delist_date")
        if ld and dd and is_yyyymmdd(ld) and is_yyyymmdd(dd) and dd < ld:
            bad_order.append(f"ins={r.get('instrument')} list={ld} delist={dd}")
    status("delist_date < list_date", len(arr), len(bad_order), bad_order)

    # 5) list_sector 枚举
    counter_ls = Counter(r.get("list_sector") for r in arr)
    print(f"  list_sector dist: {dict(counter_ls)}")
    bad_ls = [f"ins={r.get('instrument')} list_sector={r.get('list_sector')!r}"
              for r in arr if r.get("list_sector") not in LIST_SECTOR_VALUES]
    status("list_sector ∉ {1,2,3,4}", len(arr), len(bad_ls), bad_ls)

    # 6) exchange 枚举
    counter_ex = Counter(r.get("exchange") for r in arr)
    print(f"  exchange dist: {dict(counter_ex)}")
    bad_ex = [f"ins={r.get('instrument')} exchange={r.get('exchange')!r}"
              for r in arr if r.get("exchange") not in EXCHANGE_VALUES]
    status("exchange ∉ 3 大所", len(arr), len(bad_ex), bad_ex)

    # 7) 板块/交易所交叉一致性 (主板=1 应只在 SH/SZ; 北交所=4 应只在 BJ)
    bad_cross = []
    for r in arr:
        ls, ex = r.get("list_sector"), r.get("exchange")
        if ls == 4 and ex != "北京证券交易所":
            bad_cross.append(f"ins={r.get('instrument')} ls=4 (北交所) ex={ex}")
        if ex == "北京证券交易所" and ls != 4:
            bad_cross.append(f"ins={r.get('instrument')} ex=BJ ls={ls}")
    status("list_sector × exchange 交叉不一致", len(arr), len(bad_cross), bad_cross)

    return arr


def check_trading_days():
    section("[meta] trading_days  (axis D 源)")
    p = os.path.join(META, "trading_days.json")
    if not os.path.exists(p):
        print(f"  [FAIL] {p} missing")
        return None
    arr = load_json(p)
    print(f"  rows={len(arr)}  fields={sorted(arr[0].keys())}")

    cn_dates = []
    bad_date = []
    for r in arr:
        if r.get("market_code") != "CN":
            continue
        d = r.get("date")
        if not is_yyyymmdd(d):
            bad_date.append(f"date={d!r}")
            continue
        cn_dates.append(d)
    status("market_code='CN' date 非 YYYYMMDD", len(arr), len(bad_date), bad_date)

    uniq = sorted(set(cn_dates))
    dup_n = len(cn_dates) - len(uniq)
    print(f"  market_code='CN' 记录 n={len(cn_dates)}  unique dates n={len(uniq)}  duplicates={dup_n}")
    print(f"  date range: {uniq[0]} → {uniq[-1]}")

    by_year = Counter(d[:4] for d in uniq)
    print(f"  per-year count: {dict(sorted(by_year.items()))}")
    return set(uniq)


def check_grid_itf(itf_name, year_filter, required_fields, value_checks):
    """通用网格 itf 验证:
      required_fields: list[str] - 期望字段, 缺则计 bad
      value_checks: list[(field, predicate, label)] - 每条记录字段值校验
    """
    files_n = 0
    rows_total = 0
    files_empty = 0
    schema_seen = set()
    field_missing_cnt = defaultdict(int)
    bad_value = defaultdict(list)  # label -> sample list
    bad_value_cnt = defaultdict(int)
    rows_per_day = []  # (date, n)
    for ymd, fp in iter_day_files(itf_name, year_filter):
        files_n += 1
        arr = load_json(fp)
        if not arr:
            files_empty += 1
            rows_per_day.append((ymd, 0))
            continue
        rows_per_day.append((ymd, len(arr)))
        rows_total += len(arr)
        schema_seen.add(tuple(sorted(arr[0].keys())))
        for rec in arr:
            for f in required_fields:
                if f not in rec or rec[f] is None:
                    field_missing_cnt[f] += 1
            for f, pred, lbl in value_checks:
                v = rec.get(f)
                if v is None:
                    continue
                if not pred(v):
                    bad_value_cnt[lbl] += 1
                    if len(bad_value[lbl]) < ANOMALY_HEAD:
                        bad_value[lbl].append(f"{ymd} ins={rec.get('instrument')} {f}={v!r}")

    print(f"  files={files_n}  empty={files_empty}  total_rows={rows_total}")
    print(f"  distinct schemas: {len(schema_seen)}")
    if rows_per_day:
        ns = [n for _, n in rows_per_day if n > 0]
        if ns:
            ns_sorted = sorted(ns)
            print(f"  rows/day  min={min(ns)} p05={ns_sorted[len(ns)//20]} "
                  f"med={ns_sorted[len(ns)//2]} max={max(ns)}")
            # 异常少数 (< 0.5 × median) 的日子
            med = ns_sorted[len(ns) // 2]
            low_days = [(d, n) for d, n in rows_per_day if 0 < n < med * 0.5]
            if low_days:
                print(f"  rows/day << median (<0.5×{med}): {len(low_days)} days; "
                      f"samples={low_days[:ANOMALY_HEAD]}")

    for f in required_fields:
        if field_missing_cnt[f]:
            status(f"字段 {f} 缺失", rows_total, field_missing_cnt[f], [])
    for f, _, lbl in value_checks:
        if bad_value_cnt[lbl]:
            status(lbl, rows_total, bad_value_cnt[lbl], bad_value[lbl])
    if not any(field_missing_cnt.values()) and not any(bad_value_cnt.values()):
        print("  [OK]   all required fields present + value predicates hold")


def check_bar1d(year):
    section("[grid] cn_stock_real_bar1d  →  close_raw + adjust_factor")
    files_n = 0
    rows_total = 0
    bad_close = 0; bad_close_samples = []   # close 异常 (≤0 / 非数 / null 且非停牌)
    bad_af = 0; bad_af_samples = []         # adjust_factor 异常 (≤0 / 非数 / null 且非停牌)
    susp_null_skipped = 0                   # close/af=null ∧ suspended=1: 停牌正常, ffill 兜
    rows_per_day = []
    for ymd, fp in iter_day_files("cn_stock_real_bar1d", year):
        files_n += 1
        arr = load_json(fp)
        rows_per_day.append((ymd, len(arr)))
        rows_total += len(arr)
        # 同日 status cross-check: close/af=null 落在停牌票上是正常 (pit.cpp ffill 兜前值)
        status_fp = fp[: -len("cn_stock_real_bar1d.json")] + "cn_stock_status.json"
        susp_map = {}
        if os.path.exists(status_fp):
            for r in load_json(status_fp):
                susp_map[r.get("instrument")] = r.get("suspended")
        for rec in arr:
            ins = rec.get("instrument")
            is_susp = (susp_map.get(ins) == 1)
            v = rec.get("close")
            if v is None:
                if is_susp:
                    susp_null_skipped += 1
                else:
                    bad_close += 1
                    if len(bad_close_samples) < ANOMALY_HEAD:
                        bad_close_samples.append(
                            f"{ymd} ins={ins} close=null (未停牌)")
            elif not (isinstance(v, (int, float)) and v > 0):
                bad_close += 1
                if len(bad_close_samples) < ANOMALY_HEAD:
                    bad_close_samples.append(
                        f"{ymd} ins={ins} close={v!r}")
            af = rec.get("adjust_factor")
            if af is None:
                if not is_susp:
                    bad_af += 1
                    if len(bad_af_samples) < ANOMALY_HEAD:
                        bad_af_samples.append(
                            f"{ymd} ins={ins} adjust_factor=null (未停牌)")
            elif not (isinstance(af, (int, float)) and af > 0):
                bad_af += 1
                if len(bad_af_samples) < ANOMALY_HEAD:
                    bad_af_samples.append(
                        f"{ymd} ins={ins} adjust_factor={af!r}")
    print(f"  files={files_n}  total_rows={rows_total}")
    if rows_per_day:
        ns = [n for _, n in rows_per_day if n > 0]
        if ns:
            ns_sorted = sorted(ns)
            print(f"  rows/day  min={min(ns)} p05={ns_sorted[len(ns)//20]} "
                  f"med={ns_sorted[len(ns)//2]} max={max(ns)}")
    if susp_null_skipped:
        print(f"  close/af=null 但停牌 (业务正常, ffill 兜, 不计 anomaly): {susp_null_skipped} 行")
    status("close ≤ 0 / 非数 / null 且未停牌", rows_total, bad_close, bad_close_samples)
    status("adjust_factor ≤ 0 / 非数 / null 且未停牌", rows_total, bad_af, bad_af_samples)


def check_shares(year):
    section("[grid] cn_stock_shares  →  share_raw / fmcap_raw")
    files_n = 0
    rows_total = 0
    bad_pos = 0; bad_pos_samples = []
    bad_order = 0; bad_order_samples = []
    rows_per_day = []
    for ymd, fp in iter_day_files("cn_stock_shares", year):
        files_n += 1
        arr = load_json(fp)
        rows_per_day.append((ymd, len(arr)))
        rows_total += len(arr)
        for rec in arr:
            ts = rec.get("total_shares")
            tf = rec.get("total_float_shares")
            if ts is None or not isinstance(ts, (int, float)) or ts <= 0:
                bad_pos += 1
                if len(bad_pos_samples) < ANOMALY_HEAD:
                    bad_pos_samples.append(f"{ymd} ins={rec.get('instrument')} total_shares={ts!r}")
                continue
            if tf is None or not isinstance(tf, (int, float)) or tf <= 0:
                bad_pos += 1
                if len(bad_pos_samples) < ANOMALY_HEAD:
                    bad_pos_samples.append(f"{ymd} ins={rec.get('instrument')} total_float_shares={tf!r}")
                continue
            if tf > ts + 1e-6:
                bad_order += 1
                if len(bad_order_samples) < ANOMALY_HEAD:
                    bad_order_samples.append(
                        f"{ymd} ins={rec.get('instrument')} float={tf} > total={ts}")
    print(f"  files={files_n}  total_rows={rows_total}")
    status("total_shares/total_float_shares 非正", rows_total, bad_pos, bad_pos_samples)
    status("total_float_shares > total_shares", rows_total, bad_order, bad_order_samples)


def check_limit_price(year):
    section("[grid] cn_stock_limit_price  →  up_lim / dn_lim")
    files_n = 0
    rows_total = 0
    bad_neg = 0; bad_neg_samples = []
    bad_order = 0; bad_order_samples = []
    # 停牌票 BigQuant 给"理论涨跌停 = 同值" (up==dn), 业务无影响 (下游 close NaN/+inf
    # 被 cs_limit_up/dn 的 is_finite 过滤). 不计 anomaly, 单独计数显示.
    susp_order_skipped = 0
    zero_up = 0; zero_dn = 0
    for ymd, fp in iter_day_files("cn_stock_limit_price", year):
        files_n += 1
        arr = load_json(fp)
        rows_total += len(arr)
        # 同日 status cross-check: {instrument: suspended}. 文件缺则空 dict (无法判定 → 保守报)
        status_fp = fp[: -len("cn_stock_limit_price.json")] + "cn_stock_status.json"
        susp_map = {}
        if os.path.exists(status_fp):
            for r in load_json(status_fp):
                susp_map[r.get("instrument")] = r.get("suspended")
        for rec in arr:
            up = rec.get("upper_limit")
            dn = rec.get("lower_limit")
            if up is None or dn is None:
                bad_neg += 1
                if len(bad_neg_samples) < ANOMALY_HEAD:
                    bad_neg_samples.append(f"{ymd} ins={rec.get('instrument')} up={up!r} dn={dn!r}")
                continue
            if up == 0:
                zero_up += 1
            if dn == 0:
                zero_dn += 1
            if up < 0 or dn < 0:
                bad_neg += 1
                if len(bad_neg_samples) < ANOMALY_HEAD:
                    bad_neg_samples.append(f"{ymd} ins={rec.get('instrument')} up={up} dn={dn}")
                continue
            # 排除 0 哨兵后再检查序
            if up > 0 and dn > 0 and not (up > dn):
                if susp_map.get(rec.get("instrument")) == 1:
                    susp_order_skipped += 1
                    continue
                bad_order += 1
                if len(bad_order_samples) < ANOMALY_HEAD:
                    bad_order_samples.append(
                        f"{ymd} ins={rec.get('instrument')} up={up} dn={dn}")
    print(f"  files={files_n}  total_rows={rows_total}")
    print(f"  零哨兵触发: upper_limit==0 → {zero_up} 行 (parse 转 1e6); "
          f"lower_limit==0 → {zero_dn} 行 (parse 转 0.01)")
    if susp_order_skipped:
        print(f"  up==dn 但停牌 (业务正常, 不计 anomaly): {susp_order_skipped} 行")
    status("upper/lower < 0 或缺失", rows_total, bad_neg, bad_neg_samples)
    status("upper_limit ≤ lower_limit (剔 0 哨兵 + 停牌)", rows_total, bad_order, bad_order_samples)


def check_status(year):
    section("[grid] cn_stock_status  →  susp / risk_warn")
    files_n = 0
    rows_total = 0
    bad_st = 0; bad_st_samples = []
    bad_sp = 0; bad_sp_samples = []
    st_counter = Counter()
    sp_counter = Counter()
    for ymd, fp in iter_day_files("cn_stock_status", year):
        files_n += 1
        arr = load_json(fp)
        rows_total += len(arr)
        for rec in arr:
            st = rec.get("st_status")
            sp = rec.get("suspended")
            st_counter[st] += 1
            sp_counter[sp] += 1
            if st not in (0, 1, 2):
                bad_st += 1
                if len(bad_st_samples) < ANOMALY_HEAD:
                    bad_st_samples.append(f"{ymd} ins={rec.get('instrument')} st_status={st!r}")
            if sp not in (0, 1):
                bad_sp += 1
                if len(bad_sp_samples) < ANOMALY_HEAD:
                    bad_sp_samples.append(f"{ymd} ins={rec.get('instrument')} suspended={sp!r}")
    print(f"  files={files_n}  total_rows={rows_total}")
    print(f"  st_status dist: {dict(st_counter)}")
    print(f"  suspended dist: {dict(sp_counter)}")
    status("st_status ∉ {0,1,2}", rows_total, bad_st, bad_st_samples)
    status("suspended ∉ {0,1}", rows_total, bad_sp, bad_sp_samples)


def check_margin(year):
    section("[grid] cn_stock_margin_trading_detail  →  is_margin / mr_bal_raw / ms_bal_raw")
    files_n = 0
    rows_total = 0
    bad_fb = 0; bad_fb_samples = []
    bad_sb = 0; bad_sb_samples = []
    rows_per_day = []
    for ymd, fp in iter_day_files("cn_stock_margin_trading_detail", year):
        files_n += 1
        arr = load_json(fp)
        rows_per_day.append((ymd, len(arr)))
        rows_total += len(arr)
        for rec in arr:
            fb = rec.get("financing_balance")
            sb = rec.get("securities_lending_balance")
            if fb is not None and (not isinstance(fb, (int, float)) or fb < 0):
                bad_fb += 1
                if len(bad_fb_samples) < ANOMALY_HEAD:
                    bad_fb_samples.append(f"{ymd} ins={rec.get('instrument')} financing_balance={fb!r}")
            if sb is not None and (not isinstance(sb, (int, float)) or sb < 0):
                bad_sb += 1
                if len(bad_sb_samples) < ANOMALY_HEAD:
                    bad_sb_samples.append(f"{ymd} ins={rec.get('instrument')} securities_lending_balance={sb!r}")
    print(f"  files={files_n}  total_rows={rows_total}")
    if rows_per_day:
        ns = [n for _, n in rows_per_day if n > 0]
        if ns:
            ns_sorted = sorted(ns)
            print(f"  rows/day min={min(ns)} med={ns_sorted[len(ns)//2]} max={max(ns)}")
    status("financing_balance < 0", rows_total, bad_fb, bad_fb_samples)
    status("securities_lending_balance < 0", rows_total, bad_sb, bad_sb_samples)


def check_industry_component(year):
    section("[event-MonthFirst] cn_stock_industry_component  →  industry_l1 base")
    by_month = defaultdict(list)  # (yyyy, mm) -> list[dd]
    files_n = 0
    rows_total = 0
    sw_rows = 0
    bad_l1_name = 0; bad_l1_name_samples = []
    l1_name_counter = Counter()
    for ymd, fp in iter_day_files("cn_stock_industry_component", year):
        files_n += 1
        y, m, d = ymd[:4], ymd[4:6], ymd[6:8]
        by_month[(y, m)].append(d)
        arr = load_json(fp)
        rows_total += len(arr)
        for rec in arr:
            if rec.get("industry") != "sw2021":
                continue
            sw_rows += 1
            l1n = rec.get("industry_level1_name")
            l1_name_counter[l1n] += 1
            if l1n not in SW2021_L1_NAMES:
                bad_l1_name += 1
                if len(bad_l1_name_samples) < ANOMALY_HEAD:
                    bad_l1_name_samples.append(
                        f"{ymd} ins={rec.get('instrument')} l1_name={l1n!r}")
    print(f"  files={files_n}  total_rows={rows_total}  sw2021_rows={sw_rows}")

    # MonthFirst: 每月应仅 1 个 DD 子目录有文件
    bad_month_days = []
    for (y, m), dds in by_month.items():
        if len(set(dds)) > 1:
            bad_month_days.append(f"{y}-{m}: DDs={sorted(set(dds))}")
    status("MonthFirst: 单月多个 DD 文件", len(by_month), len(bad_month_days),
           bad_month_days[:ANOMALY_HEAD])

    # sw2021 L1 名称白名单
    status("industry_level1_name ∉ SW2021_L1_NAMES (31)",
           sw_rows, bad_l1_name, bad_l1_name_samples)
    print(f"  sw2021 L1 name unique count: {len(l1_name_counter)}")
    if len(l1_name_counter) != 31:
        missing = SW2021_L1_NAMES - set(l1_name_counter.keys())
        extra = set(l1_name_counter.keys()) - SW2021_L1_NAMES
        if missing:
            print(f"  数据中未出现的 SW2021 L1 名称 ({len(missing)}): {sorted(missing)}")
        if extra:
            print(f"  数据中多出的 L1 名称 ({len(extra)}): {sorted(extra)}")


def check_industry_change(year):
    section("[event] cn_stock_industry_change  →  industry_l1 incr (sw2021 L1 change_flag=1)")
    files_n = 0
    rows_total = 0
    sw_l1_total = 0  # industry='sw2021' AND industry_level=1
    sw_l1_in_events = 0  # 上面 ∧ change_flag=1 (parse 真正消费的)
    industry_dist = Counter()
    level_dist = Counter()
    change_flag_dist = Counter()
    bad_l1_name = 0; bad_l1_name_samples = []
    l1_name_counter = Counter()
    for ymd, fp in iter_day_files("cn_stock_industry_change", year):
        files_n += 1
        arr = load_json(fp)
        rows_total += len(arr)
        for rec in arr:
            industry_dist[rec.get("industry")] += 1
            level_dist[(rec.get("industry"), rec.get("industry_level"))] += 1
            change_flag_dist[rec.get("change_flag")] += 1
            if rec.get("industry") != "sw2021":
                continue
            if rec.get("industry_level") != 1:
                continue
            sw_l1_total += 1
            if rec.get("change_flag") != 1:
                continue
            sw_l1_in_events += 1
            name = rec.get("industry_name")
            l1_name_counter[name] += 1
            if name not in SW2021_L1_NAMES:
                bad_l1_name += 1
                if len(bad_l1_name_samples) < ANOMALY_HEAD:
                    bad_l1_name_samples.append(
                        f"{ymd} ins={rec.get('instrument')} industry_name={name!r}")
    print(f"  files={files_n}  total_rows={rows_total}")
    print(f"  industry dist: {dict(industry_dist)}")
    print(f"  change_flag dist: {dict(change_flag_dist)}")
    print(f"  sw2021 × level=1 行数: {sw_l1_total}  其中 change_flag=1 (parse 消费): {sw_l1_in_events}")
    status("sw2021 L1 industry_name ∉ SW2021_L1_NAMES",
           sw_l1_in_events, bad_l1_name, bad_l1_name_samples)


def check_file_dates_in_axis(d_axis):
    """每个网格 itf 的 day file 路径日是否都在 trading_days 中."""
    section("[cross] day files YYYYMMDD ∈ trading_days?")
    grid_itfs = [
        "cn_stock_real_bar1d",
        "cn_stock_shares",
        "cn_stock_limit_price",
        "cn_stock_status",
        "cn_stock_margin_trading_detail",
    ]
    for itf in grid_itfs:
        files_n = 0
        bad = []
        for ymd, _ in iter_day_files(itf, None):
            files_n += 1
            if ymd not in d_axis:
                bad.append(ymd)
        print(f"  {itf}: files={files_n}  off-axis={len(bad)}")
        if bad:
            for s in bad[:ANOMALY_HEAD]:
                print(f"    sample: {s}")


def check_instruments_against_axis(arr_basic):
    """各 itf 的 instrument 是否都在 basic_info instrument 集合中
    (否则 lookup_a == -1, parse skip)."""
    section("[cross] 各 itf instrument ∈ axis (cn_stock_basic_info)?")
    code_set = {r.get("instrument") for r in arr_basic if r.get("instrument")}
    print(f"  axis A size = {len(code_set)}")
    # 仅扫每个 itf 最近一天文件 (足够发现 schema 级问题)
    grid_itfs = [
        "cn_stock_real_bar1d",
        "cn_stock_shares",
        "cn_stock_limit_price",
        "cn_stock_status",
        "cn_stock_margin_trading_detail",
        "cn_stock_industry_component",
        "cn_stock_industry_change",
    ]
    for itf in grid_itfs:
        latest = None
        for ymd, fp in iter_day_files(itf, None):
            latest = (ymd, fp)
        if not latest:
            print(f"  {itf}: no file")
            continue
        ymd, fp = latest
        arr = load_json(fp)
        not_in = [r.get("instrument") for r in arr if r.get("instrument") not in code_set]
        print(f"  {itf} latest={ymd}: rows={len(arr)}  off-axis_instrument={len(not_in)}")
        for s in not_in[:ANOMALY_HEAD]:
            print(f"    sample: {s!r}")


# ============================================================================
# 主入口
# ============================================================================


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--year", default=None, help="仅扫某年 (e.g. 2024); 默认全量")
    args = ap.parse_args()

    arr_basic = check_basic_info()
    d_axis = check_trading_days()

    check_bar1d(args.year)
    check_shares(args.year)
    check_limit_price(args.year)
    check_status(args.year)
    check_margin(args.year)
    check_industry_component(args.year)
    check_industry_change(args.year)

    if d_axis is not None:
        check_file_dates_in_axis(d_axis)
    if arr_basic is not None:
        check_instruments_against_axis(arr_basic)


if __name__ == "__main__":
    main()
