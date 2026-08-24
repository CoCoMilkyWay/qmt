"""复现 cn_stock_valuation: 从 PIT/financial_ttm 自己算 valuation, 对比 BigQuant 给的 ground truth.

路径 B (优先验证): bar1d.close + shares + financial_ttm_shift.shift=0 + balance_general_pit + dividend → valuation

复现字段:
    total_market_cap     = close × total_shares
    float_market_cap     = close × a_float_shares                                     # ← A 股流通 (002594 等 H+A 股发现)
    pe_ttm               = total_market_cap / net_profit_to_parent_shareholders_ttm
    pb                   = total_market_cap / total_owner_equity                      # ← 含少数股东 (300750 等 11% 误差发现)
    ps_ttm               = total_market_cap / total_operating_revenue_ttm             # ← 含利息/保费 (600519 等 2% 误差发现)
    pcf_op_ttm           = total_market_cap / net_cffoa_ttm
    dividend_yield_ratio = Σ(ttm12月 cash_after_tax × total_shares_at_ex_date) / total_market_cap

PIT 语义:
    对一个 target_date T, "最新可见"的财务字段 = MAX{visible_date ≤ T} 那条记录;
    具体到 ttm_shift, 取 shift=0 那条 (该次披露下最新报告期 TTM).
    对一个 (instrument, report_date), 多次 reissue 取 visible_date 最晚的那条 (latest wins).

跑法:
    python3.11 doc/research/verify_valuation.py              # 5 只股 × 8 日期 详细对比 (sanity)
    python3.11 doc/research/verify_valuation.py batch        # 3 个月 × ~100 只股抽样, 统计全市场精确复现率
"""

import os
import sys
import glob
from datetime import date, timedelta

import pandas as pd
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PQ_ROOT = os.path.join(ROOT, "import", "parquet")

# 测试标的: 跨板块/风格
TEST_CODES = [
    "000001.SZ",  # 平安银行  - 银行
    "600519.SH",  # 贵州茅台  - 消费长牛
    "000651.SZ",  # 格力电器  - 家电分红大户
    "300750.SZ",  # 宁德时代  - 创业板大盘
    "002594.SZ",  # 比亚迪    - 周期成长
]

# 测试日期: 覆盖年报披露季前后 + 半年报 + Q3
TEST_DATES = [
    "2023-06-30",
    "2023-12-29",
    "2024-04-30",  # 年报披露季
    "2024-06-28",
    "2024-09-30",
    "2024-12-31",
    "2025-04-30",  # 年报披露季
    "2025-06-30",
]

# 误差阈值 (相对误差 |reproduce - truth| / |truth|)
EPS_OK = 0.005   # 0.5% 以内 = OK
EPS_WARN = 0.05  # 5% 以内 = WARN (业务可接受)
# 否则 FAIL


# ============================================================================
# parquet 月份滚动加载 helper
# ============================================================================

def _months_between(start_yyyymm, end_yyyymm):
    """yield 'YYYY-MM' from start to end inclusive."""
    y0, m0 = map(int, start_yyyymm.split("-"))
    y1, m1 = map(int, end_yyyymm.split("-"))
    y, m = y0, m0
    while (y, m) <= (y1, m1):
        yield f"{y:04d}-{m:02d}"
        m += 1
        if m == 13:
            m, y = 1, y + 1


def load_parquet_range(table, start_yyyymm, end_yyyymm, codes=None, cols=None):
    """加载 [start_yyyymm, end_yyyymm] 区间内 table 的所有月 parquet, 可按 codes 过滤."""
    dfs = []
    for ym in _months_between(start_yyyymm, end_yyyymm):
        p = os.path.join(PQ_ROOT, ym, f"{table}.parquet")
        if not os.path.exists(p):
            continue
        df = pd.read_parquet(p, columns=cols)
        if codes is not None:
            df = df[df.instrument.isin(codes)]
        if len(df):
            dfs.append(df)
    if not dfs:
        return pd.DataFrame()
    return pd.concat(dfs, ignore_index=True)


# ============================================================================
# 各源数据获取 + PIT cutoff
# ============================================================================

def fetch_close_and_shares(target_dates, codes):
    """返回 dict[(date, code)] -> dict(close, total_shares, total_float_shares).
    取 target_date 当日精确匹配 (close = 当日不复权收盘; shares = 当日股本)."""
    months_needed = sorted({d[:7] for d in target_dates})
    start, end = months_needed[0], months_needed[-1]
    bar1d = load_parquet_range("cn_stock_real_bar1d", start, end, codes,
                               cols=["date", "instrument", "close"])
    shares = load_parquet_range("cn_stock_shares", start, end, codes,
                                cols=["date", "instrument", "total_shares", "total_float_shares", "a_float_shares", "free_float_shares"])
    # 取 target_date 当日的 close/shares
    bar1d["d"] = pd.to_datetime(bar1d["date"]).dt.strftime("%Y-%m-%d")
    shares["d"] = pd.to_datetime(shares["date"]).dt.strftime("%Y-%m-%d")
    out = {}
    for td in target_dates:
        b = bar1d[bar1d.d == td].set_index("instrument")
        s = shares[shares.d == td].set_index("instrument")
        for c in codes:
            if c in b.index and c in s.index:
                out[(td, c)] = {
                    "close": float(b.loc[c, "close"]),
                    "total_shares": float(s.loc[c, "total_shares"]),
                    "total_float_shares": float(s.loc[c, "total_float_shares"]),
                    "a_float_shares": float(s.loc[c, "a_float_shares"]),
                    "free_float_shares": float(s.loc[c, "free_float_shares"]),
                }
    return out


def fetch_latest_ttm(target_dates, codes, lookback_months=15):
    """PIT: 对每个 (target_date, code), 取 visible_date<=target_date 的最新 shift=0 那条.
    回溯 lookback_months 个月已经足够 (公司每季披露一次, 12 个月够多 reissue)."""
    out = {}
    fields = [
        "date", "instrument", "report_date", "shift",
        "operating_revenue_ttm",
        "total_operating_revenue_ttm",
        "net_profit_ttm",
        "net_profit_to_parent_shareholders_ttm",
        "net_cffoa_ttm",
    ]
    # 缓存按 (target_date) 加载一次
    months_cache = {}
    for td in target_dates:
        td_dt = pd.Timestamp(td)
        end_ym = f"{td_dt.year:04d}-{td_dt.month:02d}"
        start_dt = td_dt - pd.DateOffset(months=lookback_months)
        start_ym = f"{start_dt.year:04d}-{start_dt.month:02d}"
        key = (start_ym, end_ym)
        if key not in months_cache:
            df = load_parquet_range("cn_stock_financial_ttm_shift", start_ym, end_ym, codes, cols=fields)
            df = df[df["shift"] == 0]
            months_cache[key] = df
        df = months_cache[key]
        # 取 visible_date <= target_date 的最新一条 per code; 同 (code, report_date) 取最新 visible_date (修订 latest wins)
        sub = df[pd.to_datetime(df.date) <= td_dt]
        for c in codes:
            cs = sub[sub.instrument == c].sort_values(["date"])
            if len(cs) == 0:
                continue
            # 按 visible_date 升序遍历, latest by report_date 维护一个 dict
            latest = {}  # report_date -> row
            for _, r in cs.iterrows():
                latest[r.report_date] = r
            # 该 code 当前的"最新报告期" = max(report_date in latest)
            best_rd = max(latest.keys())
            r = latest[best_rd]
            out[(td, c)] = {
                "report_date": str(pd.Timestamp(r.report_date).date()),
                "operating_revenue_ttm": float(r.operating_revenue_ttm) if pd.notna(r.operating_revenue_ttm) else None,
                "total_operating_revenue_ttm": float(r.total_operating_revenue_ttm) if pd.notna(r.total_operating_revenue_ttm) else None,
                "net_profit_ttm": float(r.net_profit_ttm) if pd.notna(r.net_profit_ttm) else None,
                "net_profit_to_parent_shareholders_ttm": float(r.net_profit_to_parent_shareholders_ttm) if pd.notna(r.net_profit_to_parent_shareholders_ttm) else None,
                "net_cffoa_ttm": float(r.net_cffoa_ttm) if pd.notna(r.net_cffoa_ttm) else None,
                "visible_date": str(pd.Timestamp(r.date).date()),
            }
    return out


def fetch_latest_equity(target_dates, codes, lookback_months=15):
    """PIT: 对每个 (target_date, code), 取最新一份 balance 的 total_equity_to_parent_shareholders."""
    out = {}
    fields = ["date", "instrument", "report_date", "total_owner_equity", "total_equity_to_parent_shareholders", "total_assets"]
    months_cache = {}
    for td in target_dates:
        td_dt = pd.Timestamp(td)
        end_ym = f"{td_dt.year:04d}-{td_dt.month:02d}"
        start_dt = td_dt - pd.DateOffset(months=lookback_months)
        start_ym = f"{start_dt.year:04d}-{start_dt.month:02d}"
        key = (start_ym, end_ym)
        if key not in months_cache:
            months_cache[key] = load_parquet_range("cn_stock_financial_balance_general_pit", start_ym, end_ym, codes, cols=fields)
        df = months_cache[key]
        sub = df[pd.to_datetime(df.date) <= td_dt]
        for c in codes:
            cs = sub[sub.instrument == c].sort_values(["date"])
            if len(cs) == 0:
                continue
            latest = {}
            for _, r in cs.iterrows():
                latest[r.report_date] = r
            best_rd = max(latest.keys())
            r = latest[best_rd]
            out[(td, c)] = {
                "report_date": str(pd.Timestamp(r.report_date).date()),
                "total_equity_to_parent_shareholders": float(r.total_equity_to_parent_shareholders) if pd.notna(r.total_equity_to_parent_shareholders) else None,
                "total_owner_equity": float(r.total_owner_equity) if pd.notna(r.total_owner_equity) else None,
                "total_assets": float(r.total_assets) if pd.notna(r.total_assets) else None,
                "visible_date": str(pd.Timestamp(r.date).date()),
            }
    return out


def fetch_dividend_ttm12(target_dates, codes, current_shares, lookback_months=18):
    """ttm12 月分红 Σ(cash_after_tax) × current_shares_at_target_date / total_market_cap.

    BigQuant 口径 (实测):
      - 窗口 = ex_date ∈ (target_date - 365 天, target_date]
      - 每股分红额 × **当前股本** (target_date 当日 total_shares), 不是 ex_date 当日股本
        (300750: 0.6528 + 2.52 共 3.17 元/股 × 当前 4.396e9 = 13.95e9 ✓ = truth)

    传入 current_shares: dict[(td, c)] -> {total_shares, ...}; 用其 total_shares 字段.
    """
    out = {}
    div_months = {}
    for td in target_dates:
        td_dt = pd.Timestamp(td)
        end_ym = f"{td_dt.year:04d}-{td_dt.month:02d}"
        start_dt = td_dt - pd.DateOffset(months=lookback_months)
        start_ym = f"{start_dt.year:04d}-{start_dt.month:02d}"
        key = (start_ym, end_ym)
        if key not in div_months:
            div_months[key] = load_parquet_range("cn_stock_dividend", start_ym, end_ym, codes,
                                                  cols=["date", "instrument", "publish_date", "ex_date", "cash_after_tax", "report_date"])
        d = div_months[key]
        win_start = td_dt - pd.Timedelta(days=365)
        d2 = d[(pd.to_datetime(d.ex_date) > win_start) & (pd.to_datetime(d.ex_date) <= td_dt)]
        for c in codes:
            cs = d2[d2.instrument == c]
            if len(cs) == 0:
                out[(td, c)] = {"total_dividend": 0.0, "events": 0, "cash_sum": 0.0}
                continue
            cash_sum = 0.0
            for _, r in cs.iterrows():
                cat = r.cash_after_tax
                if pd.isna(cat):
                    continue
                cash_sum += float(cat)
            cs_key = (td, c)
            ts = current_shares[cs_key]["total_shares"] if cs_key in current_shares else None
            total = cash_sum * ts if ts is not None else None
            out[(td, c)] = {"total_dividend": total, "events": len(cs), "cash_sum": cash_sum}
    return out


def fetch_ground_truth(target_dates, codes):
    """从 cn_stock_valuation parquet 读 ground truth."""
    out = {}
    months_needed = sorted({d[:7] for d in target_dates})
    df_all = load_parquet_range("cn_stock_valuation", months_needed[0], months_needed[-1], codes)
    df_all["d"] = pd.to_datetime(df_all["date"]).dt.strftime("%Y-%m-%d")
    for td in target_dates:
        sub = df_all[df_all.d == td].set_index("instrument")
        for c in codes:
            if c in sub.index:
                r = sub.loc[c]
                out[(td, c)] = {k: float(r[k]) if pd.notna(r[k]) else None for k in [
                    "total_market_cap", "float_market_cap", "dividend_yield_ratio",
                    "pe_ttm", "pe_leading", "pe_trailing", "pb",
                    "ps_ttm", "ps_leading", "ps_trailing",
                    "pcf_net_ttm", "pcf_net_leading", "pcf_op_ttm", "pcf_op_leading",
                ]}
    return out


# ============================================================================
# 复现 + 对比
# ============================================================================

def _safe_div(a, b):
    if a is None or b is None:
        return None
    if not np.isfinite(a) or not np.isfinite(b) or b == 0.0:
        return None
    return a / b


def _diff_tag(repro, truth):
    if repro is None and truth is None:
        return "BOTH_NaN", 0.0
    if repro is None:
        return "REPRO_MISSING", float("inf")
    if truth is None or truth == 0 or not np.isfinite(truth):
        return "TRUTH_MISSING_OR_ZERO", float("inf")
    rel = abs(repro - truth) / abs(truth)
    if rel <= EPS_OK:
        return "OK", rel
    if rel <= EPS_WARN:
        return "WARN", rel
    return "FAIL", rel


def main():
    print(f"复现 cn_stock_valuation  (路径 B: ttm_shift.shift=0 + balance.latest + bar1d + shares + dividend)")
    print(f"标的: {TEST_CODES}")
    print(f"日期: {TEST_DATES}\n")

    print("加载数据...")
    closes = fetch_close_and_shares(TEST_DATES, TEST_CODES)
    print(f"  bar1d+shares: {len(closes)} 个 (target_date, code)")
    ttms = fetch_latest_ttm(TEST_DATES, TEST_CODES)
    print(f"  ttm_shift latest shift=0: {len(ttms)}")
    eqs = fetch_latest_equity(TEST_DATES, TEST_CODES)
    print(f"  balance latest: {len(eqs)}")
    divs = fetch_dividend_ttm12(TEST_DATES, TEST_CODES, closes)
    print(f"  dividend ttm12M: {len(divs)}")
    truths = fetch_ground_truth(TEST_DATES, TEST_CODES)
    print(f"  cn_stock_valuation (truth): {len(truths)}")
    print()

    summary = {"OK": 0, "WARN": 0, "FAIL": 0, "REPRO_MISSING": 0, "TRUTH_MISSING_OR_ZERO": 0, "BOTH_NaN": 0}

    fields = ["total_market_cap", "float_market_cap", "pe_ttm", "pb", "ps_ttm", "pcf_op_ttm", "dividend_yield_ratio"]

    print(f"{'date':<12} {'code':<11} {'field':<22} {'repro':>16} {'truth':>16} {'rel_err':>10}  tag")
    print("-" * 110)
    fail_samples = []
    for td in TEST_DATES:
        for c in TEST_CODES:
            key = (td, c)
            if key not in closes or key not in ttms or key not in eqs or key not in truths:
                # 缺数据 — 跳过 (停牌日 / 财报未披露 / 退市 etc.)
                missing = [n for n, dct in [("close", closes), ("ttm", ttms), ("eq", eqs), ("truth", truths)] if key not in dct]
                print(f"{td:<12} {c:<11} SKIP (missing: {','.join(missing)})")
                continue
            cl = closes[key]
            tt = ttms[key]
            eq = eqs[key]
            dv = divs.get(key, {"total_dividend": 0.0, "cash_sum": 0.0})
            gt = truths[key]

            # 复现 (路径 B 修正后):
            #   float_market_cap → close × a_float_shares (BigQuant 用 A 股流通, 不含 H 股)
            #   pb               → tmc / total_owner_equity (含少数股东, 不用 equity_to_parent)
            #   ps_ttm           → tmc / total_operating_revenue_ttm (含税营业总收入)
            tmc = cl["close"] * cl["total_shares"]
            fmc = cl["close"] * cl["a_float_shares"]
            pe = _safe_div(tmc, tt["net_profit_to_parent_shareholders_ttm"])
            pb = _safe_div(tmc, eq["total_owner_equity"])
            ps = _safe_div(tmc, tt["total_operating_revenue_ttm"])
            pcf = _safe_div(tmc, tt["net_cffoa_ttm"])
            dy = _safe_div(dv["total_dividend"], tmc) if (dv["total_dividend"] or 0) > 0 else 0.0

            rep = {
                "total_market_cap": tmc,
                "float_market_cap": fmc,
                "pe_ttm": pe,
                "pb": pb,
                "ps_ttm": ps,
                "pcf_op_ttm": pcf,
                "dividend_yield_ratio": dy,
            }

            for f in fields:
                tag, rel = _diff_tag(rep[f], gt.get(f))
                summary[tag] += 1
                rs = f"{rep[f]:.4e}" if rep[f] is not None else "None"
                gs = f"{gt.get(f):.4e}" if gt.get(f) is not None else "None"
                rels = f"{rel:.4%}" if np.isfinite(rel) else "inf"
                marker = "" if tag == "OK" else "  ← " + tag
                print(f"{td:<12} {c:<11} {f:<22} {rs:>16} {gs:>16} {rels:>10}{marker}")
                if tag == "FAIL":
                    fail_samples.append((td, c, f, rep[f], gt.get(f), rel, tt, eq, cl))

    print("\n" + "=" * 80)
    print(f"汇总: {summary}")
    if fail_samples:
        print(f"\n[FAIL 详细分析 前 5]")
        for td, c, f, rp, gt, rel, tt, eq, cl in fail_samples[:5]:
            print(f"\n  {td} {c} {f}: repro={rp} truth={gt} rel={rel:.2%}")
            print(f"    close={cl['close']}  total_shares={cl['total_shares']}  total_float_shares={cl['total_float_shares']}")
            print(f"    ttm: report_date={tt['report_date']} visible={tt['visible_date']}")
            print(f"      op_rev_ttm={tt['operating_revenue_ttm']}")
            print(f"      np_parent_ttm={tt['net_profit_to_parent_shareholders_ttm']}")
            print(f"      net_cffoa_ttm={tt['net_cffoa_ttm']}")
            print(f"    balance: report_date={eq['report_date']} visible={eq['visible_date']}")
            print(f"      equity_to_parent={eq['total_equity_to_parent_shareholders']}")


# ============================================================================
# Batch 模式: 多个月 × 全市场抽样, 统计全市场精确复现率
# ============================================================================

# (target_date, sample_size_per_date) — 选业绩披露季前后 + 平时期
BATCH_DATES = [
    "2024-04-30",  # 年报披露季
    "2024-08-30",  # 半年报披露季 (8/30 周五最后交易日)
    "2024-12-31",  # 年末平时期
    "2025-04-30",  # 年报披露季
]
BATCH_SAMPLE_PER_DATE = 200  # 每日抽 200 只 (默认 ground truth >=4000 只, 抽 200 ≈ 5%)


def main_batch():
    import random
    random.seed(42)
    print(f"Batch 模式: 全市场抽样验证")
    print(f"  日期: {BATCH_DATES}")
    print(f"  每日抽: {BATCH_SAMPLE_PER_DATE} 只\n")

    # 字段对应公式 (与 main() 路径 B 完全一致)
    fields = ["total_market_cap", "float_market_cap", "pe_ttm", "pb", "ps_ttm", "pcf_op_ttm"]
    grand = {f: {"OK": 0, "WARN": 0, "FAIL": 0, "MISSING_REPRO": 0, "MISSING_TRUTH": 0} for f in fields}
    worst = {f: [] for f in fields}  # (rel, td, c, repro, truth)

    for td in BATCH_DATES:
        td_dt = pd.Timestamp(td)
        ym = f"{td_dt.year:04d}-{td_dt.month:02d}"
        print(f"== {td} ==")

        # 1) ground truth: 从 valuation 月文件取该日所有股
        gt_df = pd.read_parquet(os.path.join(PQ_ROOT, ym, "cn_stock_valuation.parquet"))
        gt_df = gt_df[pd.to_datetime(gt_df["date"]) == td_dt]
        all_codes = sorted(gt_df.instrument.unique().tolist())
        sample_codes = random.sample(all_codes, min(BATCH_SAMPLE_PER_DATE, len(all_codes)))
        gt_df = gt_df[gt_df.instrument.isin(sample_codes)].set_index("instrument")
        print(f"  全市场 {len(all_codes)} 只, 抽 {len(sample_codes)}")

        # 2) close + shares: 当日精确匹配
        bar1d = pd.read_parquet(os.path.join(PQ_ROOT, ym, "cn_stock_real_bar1d.parquet"),
                                columns=["date", "instrument", "close"])
        bar1d = bar1d[(pd.to_datetime(bar1d["date"]) == td_dt) & (bar1d.instrument.isin(sample_codes))].set_index("instrument")
        shares = pd.read_parquet(os.path.join(PQ_ROOT, ym, "cn_stock_shares.parquet"),
                                 columns=["date", "instrument", "total_shares", "total_float_shares", "a_float_shares", "free_float_shares"])
        shares = shares[(pd.to_datetime(shares["date"]) == td_dt) & (shares.instrument.isin(sample_codes))].set_index("instrument")

        # 3) PIT financial: 回溯 15 个月找 latest shift=0
        ttm_fields = ["date", "instrument", "report_date", "shift",
                      "total_operating_revenue_ttm",
                      "net_profit_to_parent_shareholders_ttm",
                      "net_cffoa_ttm"]
        bal_fields = ["date", "instrument", "report_date", "total_owner_equity"]
        ttm_dfs, bal_dfs = [], []
        for mo in range(15, -1, -1):
            mo_dt = td_dt - pd.DateOffset(months=mo)
            ym2 = f"{mo_dt.year:04d}-{mo_dt.month:02d}"
            pt = os.path.join(PQ_ROOT, ym2, "cn_stock_financial_ttm_shift.parquet")
            pb_ = os.path.join(PQ_ROOT, ym2, "cn_stock_financial_balance_general_pit.parquet")
            if os.path.exists(pt):
                df = pd.read_parquet(pt, columns=ttm_fields)
                df = df[(df["shift"] == 0) & (df.instrument.isin(sample_codes)) & (pd.to_datetime(df["date"]) <= td_dt)]
                if len(df): ttm_dfs.append(df)
            if os.path.exists(pb_):
                df = pd.read_parquet(pb_, columns=bal_fields)
                df = df[(df.instrument.isin(sample_codes)) & (pd.to_datetime(df["date"]) <= td_dt)]
                if len(df): bal_dfs.append(df)
        ttm_all = pd.concat(ttm_dfs, ignore_index=True) if ttm_dfs else pd.DataFrame(columns=ttm_fields)
        bal_all = pd.concat(bal_dfs, ignore_index=True) if bal_dfs else pd.DataFrame(columns=bal_fields)

        # per (instrument, report_date) latest by visible_date; then per instrument latest by report_date
        def _latest_by_rd(df):
            if len(df) == 0: return df.set_index("instrument") if len(df) else pd.DataFrame()
            df = df.sort_values(["instrument", "report_date", "date"])
            df = df.drop_duplicates(["instrument", "report_date"], keep="last")  # latest visible_date per (instr, rd)
            df = df.sort_values(["instrument", "report_date"])
            df = df.drop_duplicates(["instrument"], keep="last")  # latest report_date per instr
            return df.set_index("instrument")
        ttm_pit = _latest_by_rd(ttm_all)
        bal_pit = _latest_by_rd(bal_all)

        # 4) per-sample 复现 + 对比
        for c in sample_codes:
            if c not in gt_df.index or c not in bar1d.index or c not in shares.index:
                for f in fields: grand[f]["MISSING_REPRO"] += 1
                continue
            close = float(bar1d.loc[c, "close"])
            ts = float(shares.loc[c, "total_shares"])
            af = float(shares.loc[c, "a_float_shares"])

            tmc = close * ts
            fmc = close * af
            np_ttm = float(ttm_pit.loc[c, "net_profit_to_parent_shareholders_ttm"]) if c in ttm_pit.index and pd.notna(ttm_pit.loc[c, "net_profit_to_parent_shareholders_ttm"]) else None
            or_ttm = float(ttm_pit.loc[c, "total_operating_revenue_ttm"]) if c in ttm_pit.index and pd.notna(ttm_pit.loc[c, "total_operating_revenue_ttm"]) else None
            cf_ttm = float(ttm_pit.loc[c, "net_cffoa_ttm"]) if c in ttm_pit.index and pd.notna(ttm_pit.loc[c, "net_cffoa_ttm"]) else None
            toe = float(bal_pit.loc[c, "total_owner_equity"]) if c in bal_pit.index and pd.notna(bal_pit.loc[c, "total_owner_equity"]) else None
            rep = {
                "total_market_cap": tmc,
                "float_market_cap": fmc,
                "pe_ttm": _safe_div(tmc, np_ttm),
                "pb": _safe_div(tmc, toe),
                "ps_ttm": _safe_div(tmc, or_ttm),
                "pcf_op_ttm": _safe_div(tmc, cf_ttm),
            }
            gt = gt_df.loc[c]
            for f in fields:
                truth = float(gt[f]) if pd.notna(gt[f]) else None
                if rep[f] is None:
                    grand[f]["MISSING_REPRO"] += 1
                    continue
                if truth is None or truth == 0 or not np.isfinite(truth):
                    grand[f]["MISSING_TRUTH"] += 1
                    continue
                rel = abs(rep[f] - truth) / abs(truth)
                if rel <= EPS_OK:
                    grand[f]["OK"] += 1
                elif rel <= EPS_WARN:
                    grand[f]["WARN"] += 1
                    worst[f].append((rel, td, c, rep[f], truth))
                else:
                    grand[f]["FAIL"] += 1
                    worst[f].append((rel, td, c, rep[f], truth))

    # ===== 汇总 =====
    print(f"\n{'='*80}\n[Batch 全市场抽样汇总]")
    print(f"{'field':<22} {'OK':>8} {'WARN':>8} {'FAIL':>8} {'MISS_REP':>10} {'MISS_TR':>9}  {'OK%':>7}")
    for f in fields:
        s = grand[f]
        tot = s["OK"] + s["WARN"] + s["FAIL"]
        ok_pct = s["OK"] / tot if tot else 0.0
        print(f"{f:<22} {s['OK']:>8} {s['WARN']:>8} {s['FAIL']:>8} {s['MISSING_REPRO']:>10} {s['MISSING_TRUTH']:>9}  {ok_pct:>7.2%}")

    # ===== worst 10 per field =====
    print(f"\n[各字段最差 10 个样本]")
    for f in fields:
        w = sorted(worst[f], reverse=True)[:10]
        if not w: continue
        print(f"\n--- {f} worst 10 ---")
        for rel, td, c, rp, tr in w:
            print(f"  rel={rel:>7.2%}  {td}  {c:<11}  repro={rp:.4e}  truth={tr:.4e}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "batch":
        main_batch()
    else:
        main()
