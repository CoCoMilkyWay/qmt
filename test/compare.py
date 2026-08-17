#!/usr/bin/env python3
# 果仁网因子 100% 还原对比: 用 data/YYYY-MM/*.parquet (bigquant/tushare 落地) 逐因子复现 test/1.csv
# 用法: python3 test/compare.py [factor ...]   (默认全部; --list 看清单)
import argparse
import os
import sys
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor

import numpy as np
import pandas as pd
import pyarrow.parquet as pq

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
CACHE = os.path.join(ROOT, "test", "cache")
OUT = os.path.join(ROOT, "test", "out")
CSV = os.path.join(ROOT, "test", "1.csv")

# ============================================================================
# 表加载 (feather 缓存; date 统一 int32 yyyymmdd)
# ============================================================================

TABLES = {
    "bar1d": ("cn_stock_real_bar1d", ["date", "instrument", "close"]),
    "shares": ("cn_stock_shares", ["date", "instrument", "total_shares", "a_float_shares", "total_float_shares"]),
    "limit": ("cn_stock_limit_price", ["date", "instrument", "upper_limit", "lower_limit"]),
    "status": ("cn_stock_status", ["date", "instrument", "st_status", "is_risk_warning", "suspended"]),
    "margin": ("cn_stock_margin_trading_detail", ["date", "instrument", "financing_balance", "securities_lending_balance"]),
    "ttm": ("cn_stock_financial_ttm_shift", ["date", "instrument", "report_date", "shift",
                                             "total_operating_revenue_ttm", "net_profit_to_parent_shareholders_ttm",
                                             "net_profit_ttm", "net_cffoa_ttm"]),
    "balance": ("cn_stock_financial_balance_general_pit", ["date", "instrument", "report_date",
                                                           "total_owner_equity", "total_equity_to_parent_shareholders",
                                                           "total_assets"]),
    "income": ("cn_stock_financial_income_general_pit", ["date", "instrument", "report_date", "fs_quarter_index",
                                                         "net_profit_to_parent_shareholders", "net_profit",
                                                         "total_profit", "total_operating_revenue"]),
    "dividend": ("cn_stock_dividend", ["date", "instrument", "report_date", "publish_date",
                                       "cash_before_tax", "cash_after_tax", "register_date", "ex_date"]),
    "indcomp": ("cn_stock_industry_component", ["date", "instrument", "industry", "industry_level1_name",
                                                "industry_level2_name"]),
    "indchg": ("cn_stock_industry_change", ["date", "instrument", "industry", "industry_level", "industry_name", "change_flag"]),
    "forecast": ("forecast", ["ts_code", "ann_date", "end_date", "type", "last_parent_net",
                              "net_profit_min", "net_profit_max"]),
    "tdays": ("trading_days", None),
    "valuation": ("cn_stock_valuation", ["date", "instrument", "total_market_cap", "pe_ttm", "pb", "ps_ttm",
                                         "pcf_op_ttm", "dividend_yield_ratio"]),
}


def _to_int_date(s: pd.Series) -> np.ndarray:
    if s.dtype == object:  # tushare "YYYYMMDD" string
        return s.astype("int64").astype("int32").values
    dt = pd.to_datetime(s)
    return (dt.dt.year * 10000 + dt.dt.month * 100 + dt.dt.day).astype("int32").values


def months() -> list[str]:
    return sorted(m for m in os.listdir(DATA) if len(m) == 7 and m[4] == "-")


def load(key: str) -> pd.DataFrame:
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, key + ".feather")
    if os.path.exists(path):
        return pd.read_feather(path)
    name, cols = TABLES[key]
    parts = []
    def read1(m):
        f = os.path.join(DATA, m, name + ".parquet")
        if not os.path.exists(f):
            return None
        avail = set(pq.read_schema(f).names)
        use = [c for c in cols if c in avail] if cols else None
        t = pq.read_table(f, columns=use)
        return t.to_pandas() if t.num_rows else None
    with ThreadPoolExecutor(8) as ex:
        for df in ex.map(read1, months()):
            if df is not None:
                parts.append(df)
    df = pd.concat(parts, ignore_index=True)
    for c in df.columns:
        if c in ("date", "report_date", "publish_date", "ex_date", "ann_date", "end_date"):
            df[c] = _to_int_date(df[c])
    tmp = path + ".tmp"
    df.to_feather(tmp)
    os.replace(tmp, path)
    return df


# ============================================================================
# 公共上下文: 果仁 CSV + 交易日轴
# ============================================================================

def to_inst(code: str) -> str:
    return code + (".SH" if code[0] == "6" else ".SZ")


def load_basic_info():
    df = pq.read_table(os.path.join(DATA, "_meta", "cn_stock_basic_info.parquet"),
                       columns=["instrument", "list_date", "delist_date", "list_sector"]).to_pandas()
    for c in ("list_date", "delist_date"):
        dt = pd.to_datetime(df[c])
        df[c] = (dt.dt.year * 10000 + dt.dt.month * 100 + dt.dt.day).fillna(0).astype("int32")
    return df.set_index("instrument")


def load_ctx():
    csv = pd.read_csv(CSV, dtype={"股票代码": str})
    csv["T"] = csv["开始日期"].str.replace("-", "").astype("int32")
    csv["inst"] = csv["股票代码"].map(to_inst)
    csv["stuck"] = csv["备注"].notna()  # 因停牌/封板无法调仓的存量持仓

    td = load("tdays")
    tdays = np.sort(td.loc[td["market_code"] == "CN", "date"].unique())
    # P = T 的上一交易日, P2 = 上上交易日
    uts = np.sort(csv["T"].unique())
    idx = np.searchsorted(tdays, uts, side="left")
    pmap = {int(t): (int(tdays[i - 1]) if i >= 1 else 0) for t, i in zip(uts, idx)}
    p2map = {int(t): (int(tdays[i - 2]) if i >= 2 else 0) for t, i in zip(uts, idx)}
    csv["P"] = csv["T"].map(pmap).astype("int32")
    csv["P2"] = csv["T"].map(p2map).astype("int32")
    return csv, tdays


# ============================================================================
# 对比工具
# ============================================================================

def numeric_report(name, csv, ours, theirs, atol, note=""):
    m = pd.DataFrame({"T": csv["T"], "inst": csv["inst"], "ours": ours, "theirs": theirs})
    nodata = m["ours"].isna() & m["theirs"].notna()
    both = m["ours"].notna() & m["theirs"].notna()
    d = (m["ours"] - m["theirs"]).abs()
    rel = d / m["theirs"].abs().clip(lower=1e-12)
    strict = both & ((d <= atol) | (rel <= 5e-4))
    loose = both & ((d <= atol) | (rel <= 1e-2))
    lines = [f"n={len(m)} nodata={nodata.sum()} ({nodata.mean():.2%})",
             f"strict(±{atol}或0.05%)={strict.sum()/max(both.sum(),1):.4%}  loose(1%)={loose.sum()/max(both.sum(),1):.4%}  "
             f"median_rel={rel[both].median():.2e}"]
    bad = m[both & ~loose].copy()
    if len(bad):
        byy = (m[both].assign(y=m["T"] // 10000, ok=loose[both])
               .groupby("y")["ok"].agg(["mean", "count"]))
        worst = byy[byy["mean"] < 0.999]
        if len(worst):
            lines.append("按年 loose 匹配率(<99.9%的年): " +
                         "  ".join(f"{y}:{r['mean']:.2%}" for y, r in worst.iterrows()))
        top = bad.assign(rel=rel[bad.index]).nlargest(5, "rel")
        for _, r in top.iterrows():
            lines.append(f"  例 {r['T']} {r['inst']} ours={r['ours']:.6g} theirs={r['theirs']:.6g}")
        os.makedirs(OUT, exist_ok=True)
        bad.head(5000).to_csv(os.path.join(OUT, f"{name}_mismatch.csv"), index=False)
    if note:
        lines.append(note)
    return {"factor": name, "rate": strict.sum() / max(both.sum(), 1),
            "loose": loose.sum() / max(both.sum(), 1), "lines": lines}


def binary_report(name, csv, ours, theirs, note=""):
    keep = theirs.notna()
    m = pd.DataFrame({"T": csv["T"][keep], "inst": csv["inst"][keep],
                      "ours": ours[keep].fillna(0).astype(int), "theirs": theirs[keep].astype(int)})
    agree = (m["ours"] == m["theirs"])
    tp = ((m.ours == 1) & (m.theirs == 1)).sum()
    fp = ((m.ours == 1) & (m.theirs == 0)).sum()
    fn = ((m.ours == 0) & (m.theirs == 1)).sum()
    lines = [f"n={len(m)}  agree={agree.mean():.4%}  csv_1s={int(m.theirs.sum())} ours_1s={int(m.ours.sum())} "
             f"tp={tp} fp={fp} fn={fn}"]
    bad = m[~agree]
    if len(bad):
        for _, r in bad.head(5).iterrows():
            lines.append(f"  例 {r['T']} {r['inst']} ours={r['ours']} theirs={r['theirs']}")
        os.makedirs(OUT, exist_ok=True)
        bad.head(5000).to_csv(os.path.join(OUT, f"{name}_mismatch.csv"), index=False)
    if note:
        lines.append(note)
    return {"factor": name, "rate": agree.mean(), "loose": agree.mean(), "lines": lines}


def grid_at(csv, table, col, on="P", ffill=False):
    """网格表取值: (date==csv[on], inst) 精确 join; ffill=True 时取 date≤csv[on] 最新行 (停牌沿用)."""
    t = load(table)
    if not ffill:
        m = csv[[on, "inst"]].merge(t.rename(columns={"date": on, "instrument": "inst"})[[on, "inst", col]],
                                    on=[on, "inst"], how="left")
        return m[col]
    left = csv[[on, "inst"]].copy()
    left["k"] = left[on].astype("int64")
    left = left.sort_values("k", kind="stable").reset_index()
    right = t.rename(columns={"instrument": "inst"})[["date", "inst", col]].copy()
    right["k"] = right["date"].astype("int64")
    right = right.sort_values("k", kind="stable")
    out = pd.merge_asof(left, right[["k", "inst", col]], on="k", by="inst")
    return out.set_index("index").sort_index()[col]


def asof_event(csv, ev, valcols, vis="date"):
    """事件表 PIT: 每 (inst, T) 取 visible < T 的最新一行 (merge_asof, 不含 T 当日)."""
    left = csv[["T", "inst"]].copy()
    left["k"] = left["T"].astype("int64")
    left = left.sort_values("k", kind="stable").reset_index()
    right = ev.rename(columns={vis: "k", "instrument": "inst"}).copy()
    right["k"] = right["k"].astype("int64")
    right = right.sort_values("k", kind="stable")
    out = pd.merge_asof(left, right[["k", "inst"] + valcols], on="k", by="inst",
                        allow_exact_matches=False)
    return out.set_index("index").sort_index()[valcols]


def mcap_ours(csv):
    cl = grid_at(csv, "bar1d", "close", ffill=True)
    sh = grid_at(csv, "shares", "total_shares", ffill=True)
    return cl * sh


# ============================================================================
# 因子 worker
# ============================================================================

def f_close(csv, tdays):
    ours = grid_at(csv, "bar1d", "close", ffill=True)
    return numeric_report("close", csv, ours, csv["收盘价"], atol=0.006)


def f_mcap(csv, tdays):
    return numeric_report("mcap", csv, mcap_ours(csv) / 1e8, csv["总市值(亿)"], atol=0.006)


def f_mr_bal(csv, tdays):
    # 果仁 "昨日" 实际是 T-2 日余额 (T-1 余额 T 日盘前 10 点才入库, 果仁未用)
    ours = (grid_at(csv, "margin", "financing_balance", on="P2") / 1e4).fillna(0.0)
    return numeric_report("mr_bal", csv, ours, csv["昨日融资余额(万)"], atol=0.006)


def f_ms_bal(csv, tdays):
    ours = (grid_at(csv, "margin", "securities_lending_balance", on="P2") / 1e4).fillna(0.0)
    return numeric_report("ms_bal", csv, ours, csv["昨日融券余额(万)"], atol=0.006)


def f_st(csv, tdays):
    # 果仁 ST标记 = ST ∪ *ST (超集); 星ST标记 = 仅 *ST
    st = grid_at(csv, "status", "st_status", on="T")
    r1 = binary_report("st", csv, (st >= 1).astype(float), csv["ST标记"],
                       note="ST标记=st_status∈{1,2}")
    r2 = binary_report("star_st", csv, (st == 2).astype(float), csv["星ST标记"])
    return [r1, r2]


def f_susp(csv, tdays):
    # 非卡单行 (备注为空 = 当日新调入) 应满足 susp==0 filter; 卡单行是存量持仓, 不受 filter 约束
    fresh = csv[~csv["stuck"]].reset_index(drop=True)
    susp = grid_at(fresh, "status", "suspended", on="T")
    ours = (susp.fillna(0) > 0).astype(float)
    zeros = pd.Series(0.0, index=fresh.index)
    return binary_report("susp(非卡单应全0)", fresh, ours, zeros)


def f_limit(csv, tdays):
    fresh = csv[~csv["stuck"]].reset_index(drop=True)
    cl = grid_at(fresh, "bar1d", "close")
    up = grid_at(fresh, "limit", "upper_limit")
    dn = grid_at(fresh, "limit", "lower_limit")
    zeros = pd.Series(0.0, index=fresh.index)
    lu = ((cl >= up - 1e-4) & up.notna() & (up > 0)).astype(float)
    ld = ((cl <= dn + 1e-4) & dn.notna() & (dn > 0)).astype(float)
    r1 = binary_report("limit_up(非卡单应全0)", fresh, lu, zeros)
    r2 = binary_report("limit_dn(非卡单应全0)", fresh, ld, zeros)
    return [r1, r2]


def _ttm_latest(csv, col):
    ttm = load("ttm")
    ev = ttm[ttm["shift"] == 0][["date", "instrument", col]]
    return asof_event(csv, ev, [col])[col]


def f_rev_ttm(csv, tdays):
    ours = _ttm_latest(csv, "total_operating_revenue_ttm") / 1e4
    return numeric_report("rev_ttm", csv, ours, csv["营业总收入TTM(万)"], atol=0.006)


def f_ni_ttm(csv, tdays):
    ours = _ttm_latest(csv, "net_profit_to_parent_shareholders_ttm") / 1e4
    return numeric_report("ni_ttm", csv, ours, csv["归属于母公司所有者的净利润TTM(万)"], atol=0.006)


def f_pe(csv, tdays):
    ni = _ttm_latest(csv, "net_profit_to_parent_shareholders_ttm")
    ours = mcap_ours(csv) / ni.where(ni != 0)
    return numeric_report("pe", csv, ours, csv["市盈率"], atol=0.006)


def f_ps(csv, tdays):
    rev = _ttm_latest(csv, "total_operating_revenue_ttm")
    ours = mcap_ours(csv) / rev.where(rev != 0)
    return numeric_report("ps", csv, ours, csv["市销率"], atol=0.006)


def f_pcf(csv, tdays):
    cf = _ttm_latest(csv, "net_cffoa_ttm")
    ours = mcap_ours(csv) / cf.where(cf != 0)
    return numeric_report("pcf", csv, ours, csv["市现率"], atol=0.006)


def _balance_latest(csv, col):
    """balance PIT: visible < T 内 max(report_date) 的最新可见行."""
    bal = load("balance").sort_values(["instrument", "date", "report_date"], kind="stable")
    # 按 (inst) 沿 visible 扫: 维护 max(report_date) 对应值
    out_rows = []
    for inst, g in bal.groupby("instrument", sort=False):
        best_rd = -1
        vals = {}
        for date, rd, v in zip(g["date"].values, g["report_date"].values, g[col].values):
            vals[rd] = v
            if rd > best_rd:
                best_rd = rd
            out_rows.append((inst, date, vals[best_rd]))
    ev = pd.DataFrame(out_rows, columns=["instrument", "date", col])
    ev = ev.groupby(["instrument", "date"], as_index=False).last()
    return asof_event(csv, ev, [col])[col]


def f_pb(csv, tdays):
    mc = mcap_ours(csv)
    eq_all = _balance_latest(csv, "total_owner_equity")
    eq_par = _balance_latest(csv, "total_equity_to_parent_shareholders")
    r1 = numeric_report("pb(含少数)", csv, mc / eq_all.where(eq_all != 0), csv["市净率"], atol=0.006)
    r2 = numeric_report("pb(归母)", csv, mc / eq_par.where(eq_par != 0), csv["市净率"], atol=0.006)
    return [r1, r2]


def _prev_q(rd):
    y, md = rd // 10000, rd % 10000
    m2 = {1231: 930, 930: 630, 630: 331, 331: None}[md]
    return (y * 10000 + m2) if m2 else (y - 1) * 10000 + 1231


def _roe_roa_ours(csv):
    """果仁口径 (实测): ROE = net_profit_ttm(含少数) / TTM窗口5季度点平均 total_owner_equity;
       ROA 同分子 / 平均 total_assets. 合并 ttm+balance 事件流, per-inst 状态机后 asof."""
    ttm = load("ttm")
    ttm0 = ttm[ttm["shift"] == 0][["date", "instrument", "report_date", "net_profit_ttm"]]
    bal = load("balance")[["date", "instrument", "report_date", "total_owner_equity", "total_assets"]]
    ev = pd.concat([ttm0.assign(src=1), bal.assign(src=0)], ignore_index=True)
    ev = ev.sort_values(["instrument", "date", "src"], kind="stable")

    rows = []
    for inst, g in ev.groupby("instrument", sort=False):
        vals = {}
        anchor, npt = None, np.nan
        for date, rd, src, np_ttm, eq, ta in zip(g["date"].values, g["report_date"].values, g["src"].values,
                                                 g["net_profit_ttm"].values, g["total_owner_equity"].values,
                                                 g["total_assets"].values):
            if src == 0:
                vals[rd] = (eq, ta)
            else:
                anchor, npt = int(rd), np_ttm
            if anchor is None:
                continue
            ch = [anchor]
            for _ in range(4):
                ch.append(_prev_q(ch[-1]))
            es = [vals[r][0] for r in ch if r in vals]
            ts_ = [vals[r][1] for r in ch if r in vals]
            roe = npt / np.mean(es) if len(es) == 5 else np.nan
            roa = npt / np.mean(ts_) if len(ts_) == 5 else np.nan
            rows.append((inst, date, roe, roa))
    st = pd.DataFrame(rows, columns=["instrument", "date", "roe", "roa"]).groupby(
        ["instrument", "date"], as_index=False).last()
    out = asof_event(csv, st, ["roe", "roa"])
    return out["roe"], out["roa"]


def f_roe_roa(csv, tdays):
    roe, roa = _roe_roa_ours(csv)
    note = "口径=net_profit_ttm(含少数)/5季度点平均; 残差疑似果仁平均法细节"
    r1 = numeric_report("roe", csv, roe, csv["净资产收益率"], atol=0.00011, note=note)
    r2 = numeric_report("roa", csv, roa, csv["资产回报率"], atol=0.00011, note=note)
    return [r1, r2]


def f_dy(csv, tdays):
    # 果仁口径 (实测): 窗口 = 预案公告日 ∈ (T-365, T]; 分红总额 = 每股税前 × 公告时股本; / 当前总市值
    div = load("dividend")
    div = div[div["cash_before_tax"] > 0].copy()
    sh = load("shares")[["date", "instrument", "total_shares"]].copy()
    div["k"] = div["publish_date"].astype("int64")
    div = div[div["k"] > 0].sort_values("k", kind="stable")
    sh = sh.rename(columns={"date": "k"})
    sh["k"] = sh["k"].astype("int64")
    sh = sh.sort_values("k", kind="stable")
    div = pd.merge_asof(div, sh, on="k", by="instrument")

    mc = mcap_ours(csv)
    tsd = pd.to_datetime(csv["T"].astype(str), format="%Y%m%d").values.astype("datetime64[D]")
    by_inst = {k: g for k, g in div.groupby("instrument")}
    res = np.zeros(len(csv))
    for i, (inst, td) in enumerate(zip(csv["inst"].values, tsd)):
        g = by_inst.get(inst)
        if g is None:
            continue
        ad = pd.to_datetime(g["publish_date"].astype(str), format="%Y%m%d", errors="coerce").values.astype("datetime64[D]")
        w = (ad <= td) & (ad > td - np.timedelta64(365, "D"))
        res[i] = (g["cash_before_tax"].values[w] * g["total_shares"].values[w]).sum()
    ours = pd.Series(res, index=csv.index) / mc
    note = "口径=Σ(税前每股×公告日股本, 预案公告∈(T-365,T])/当前总市值; 残差疑似果仁用实施公告日(缺此字段)"
    return numeric_report("dy", csv, ours, csv["股息率TTM"], atol=0.00011, note=note)


def _industry_got(csv, version, level=1):
    col = f"industry_level{level}_name"
    comp = load("indcomp")
    comp = comp[comp["industry"] == version][["date", "instrument", col]]
    chg = load("indchg")
    chg = chg[(chg["industry"] == version) & (chg["industry_level"] == level) & (chg["change_flag"] == 1)]
    chg = chg.rename(columns={"industry_name": col})[["date", "instrument", col]]
    ev = pd.concat([comp, chg], ignore_index=True).sort_values(["instrument", "date"], kind="stable")
    return asof_event(csv, ev, [col])[col]


def f_industry(csv, tdays):
    # 果仁 2022 起用申万2021 (100% 对上); 之前 = sw2014 结构但显示新版名 → 用 sw2014 L2→果仁名 学习映射
    CUTOVER = 20220101
    got21 = _industry_got(csv, "sw2021")
    l1_14 = _industry_got(csv, "sw2014", 1)
    l2_14 = _industry_got(csv, "sw2014", 2)
    pre = csv["T"] < CUTOVER
    key = l2_14.where(l2_14.notna(), l1_14)
    learn = pd.DataFrame({"key": key[pre], "gr": csv["行业分类"][pre]}).dropna()
    mode = learn.groupby("key")["gr"].agg(lambda s: s.mode().iat[0])
    consist = (learn["gr"] == learn["key"].map(mode)).mean()
    got = key.map(mode).where(pre, got21)
    ok = (got == csv["行业分类"])
    both = got.notna()
    lines = [f"n={len(csv)} nodata={(~both).sum()}  agree={ok[both].mean():.4%}  (切换日 {CUTOVER})",
             f"pre-2022 映射: sw2014-L2→果仁名 共 {len(mode)} 键, 键内一致率 {consist:.4%} (由数据学得)"]
    m = csv[both].assign(y=csv["T"][both] // 10000, ok=ok[both])
    byy = m.groupby("y")["ok"].mean()
    worst = byy[byy < 0.999]
    if len(worst):
        lines.append("按年(<99.9%): " + "  ".join(f"{y}:{v:.2%}" for y, v in worst.items()))
    bad = csv[both & ~ok].assign(ours=got[both & ~ok])
    if len(bad):
        pairs = bad.groupby(["ours", "行业分类"]).size().nlargest(8)
        lines.append("常见错对 (ours→果仁): " + "  ".join(f"{a}→{b}:{n}" for (a, b), n in pairs.items()))
        os.makedirs(OUT, exist_ok=True)
        bad[["T", "inst", "ours", "行业分类"]].head(5000).to_csv(os.path.join(OUT, "industry_mismatch.csv"), index=False)
    return {"factor": "industry", "rate": ok[both].mean(), "loose": ok[both].mean(), "lines": lines}


def f_loss2y(csv, tdays):
    fc = load("forecast")
    fc = fc[(fc["end_date"] % 10000 // 100 == 12)
            & fc["type"].isin(["首亏", "续亏"])
            & (fc["last_parent_net"] < 0)]
    inc = load("income")
    ann = inc[inc["fs_quarter_index"] == 4]
    fin_first = ann.groupby(["instrument", "report_date"])["date"].min()  # 正式年报首次可见日
    ours = np.zeros(len(csv))
    csv_i = csv.reset_index(drop=True)
    by_inst = {k: g for k, g in fc.groupby("ts_code")}
    tarr = csv_i["T"].values
    iarr = csv_i["inst"].values
    for i in range(len(csv_i)):
        g = by_inst.get(iarr[i])
        if g is None:
            continue
        T = tarr[i]
        for ann_d, end_d in zip(g["ann_date"].values, g["end_date"].values):
            if not (ann_d < T):
                continue
            ddl = (end_d // 10000 + 1) * 10000 + 430
            if T >= ddl:
                continue
            fin_v = fin_first.get((iarr[i], end_d), None)
            if fin_v is not None and T > fin_v:
                continue
            ours[i] = 1.0
            break
    return binary_report("loss2y", csv, pd.Series(ours, index=csv.index), csv["预期连续两年亏损"])


def f_g9_fin(csv, tdays):
    # 国九条财务 = "预期"口径: 年度预告首亏/续亏 ∧ rev_ttm<阈 (主板3亿/创科1亿, 现行规则全历史回填);
    # 窗口 = 预告公告后 → 正式年报披露/4.30 止
    fc = load("forecast")
    fc = fc[(fc["end_date"] % 10000 // 100 == 12) & fc["type"].isin(["首亏", "续亏"])]
    inc = load("income")
    ann = inc[inc["fs_quarter_index"] == 4]
    fin_first = ann.groupby(["instrument", "report_date"])["date"].min()
    rev = _ttm_latest(csv, "total_operating_revenue_ttm").values
    csv_i = csv.reset_index(drop=True)
    thr_arr = np.where(csv_i["inst"].str.startswith(("30", "68")), 1e8, 3e8)
    ours = np.zeros(len(csv))
    by_inst = {k: g for k, g in fc.groupby("ts_code")}
    tarr = csv_i["T"].values
    iarr = csv_i["inst"].values
    for i in range(len(csv_i)):
        g = by_inst.get(iarr[i])
        if g is None:
            continue
        T = tarr[i]
        for ann_d, end_d in zip(g["ann_date"].values, g["end_date"].values):
            if not (ann_d < T):
                continue
            ddl = (end_d // 10000 + 1) * 10000 + 430
            if T >= ddl:
                continue
            fin_v = fin_first.get((iarr[i], end_d), None)
            if fin_v is not None and T > fin_v:
                continue
            if np.isfinite(rev[i]) and rev[i] < thr_arr[i]:
                ours[i] = 1.0
                break
    return binary_report("g9_fin", csv, pd.Series(ours, index=csv.index), csv["国九条财务退市预警"],
                         note="预告首亏/续亏 ∧ rev_ttm<3亿(主板)/1亿(创科), 年报披露即撤")


def _roll20_all(csv, tdays, flag_grid):
    """flag_grid: DataFrame index=int日期(交易日) cols=inst, bool. 返回 csv 行的 rolling20(all)@P."""
    r = flag_grid.rolling(20, min_periods=20).sum() >= 20
    r = r.reindex(tdays).fillna(False)
    key = pd.MultiIndex.from_arrays([csv["P"], csv["inst"]])
    stacked = r.stack()
    return pd.Series(stacked.reindex(key).values, index=csv.index).fillna(False)


def _pivot_csv_insts(csv, tdays, table, col):
    t = load(table)
    insts = csv["inst"].unique()
    t = t[t["instrument"].isin(set(insts))]
    return t.pivot_table(index="date", columns="instrument", values=col, aggfunc="last").reindex(tdays).ffill()


def f_trade_warn(csv, tdays):
    cl = _pivot_csv_insts(csv, tdays, "bar1d", "close")
    sh = _pivot_csv_insts(csv, tdays, "shares", "total_shares")
    mc = cl * sh
    mainb = pd.Series({c: c[0] in ("6",) and not c.startswith("68") or c.startswith(("000", "001", "002", "003"))
                       for c in cl.columns})
    thr = mainb.map({True: 5e8, False: 3e8})
    low_p = (cl < 1.0) & cl.notna()
    low_mc = mc.lt(thr, axis=1) & mc.notna()
    r_trade = binary_report("trade_warn(=20D面值<1)", csv, _roll20_all(csv, tdays, low_p).astype(float),
                            csv["交易类退市预警"])
    r_g9t = binary_report("g9_trade(=20D市值<阈)", csv, _roll20_all(csv, tdays, low_mc).astype(float),
                          csv["国九条交易退市预警"])
    return [r_trade, r_g9t]


def f_pool(csv, tdays, min_list_days=0):
    """截面攻坚: 全市场 mcap 升序 + 默认 filter → 每日 bottom-N vs 果仁当日新调入 (非卡单).
       卡单持仓从两边剔除 (不受 filter/rank 约束)."""
    bar = load("bar1d")
    sh = load("shares")[["date", "instrument", "total_shares"]]
    lim = load("limit")
    st = load("status")[["date", "instrument", "suspended"]]
    bar = bar[~bar["instrument"].str.endswith(".BJ")]
    bi = load_basic_info()
    list_d = bi["list_date"].to_dict()
    delist_d = bi["delist_date"].to_dict()

    days = np.sort(csv["T"].unique())
    pmap = dict(csv[["T", "P"]].drop_duplicates().values)
    fresh = csv[~csv["stuck"]]
    by_T = {t: set(g) for t, g in fresh.groupby("T")["inst"]}
    stuck_T = {t: set(g) for t, g in csv[csv["stuck"]].groupby("T")["inst"]}

    bar_g = {d: g for d, g in bar.groupby("date")}
    sh_g = {d: g for d, g in sh.groupby("date")}
    lim_g = {d: g for d, g in lim.groupby("date")}
    st_g = {d: g for d, g in st.groupby("date")}

    def days_since(inst, T, anchor):
        a = anchor.get(inst, 0)
        if not a:
            return np.nan
        return (pd.Timestamp(str(T)) - pd.Timestamp(str(a))).days

    stats = []
    diffs = []
    for T in days:
        P = pmap[T]
        want = by_T.get(T, set())
        stuck = stuck_T.get(T, set())
        if P == 0 or P not in bar_g or not want:
            continue
        b = bar_g[P].set_index("instrument")["close"]
        s = sh_g[P].set_index("instrument")["total_shares"] if P in sh_g else pd.Series(dtype=float)
        u = lim_g[P].set_index("instrument") if P in lim_g else None
        stt = st_g[T].set_index("instrument")["suspended"] if T in st_g else pd.Series(dtype=float)
        df = pd.DataFrame({"close": b}).join(s.rename("shares"))
        df["mcap"] = df["close"] * df["shares"]
        df = df[df["mcap"].notna() & (df["mcap"] > 0)]
        if u is not None:
            df = df.join(u)
            df = df[~((df["close"] >= df["upper_limit"] - 1e-4) & (df["upper_limit"] > 0))]
            df = df[~((df["close"] <= df["lower_limit"] + 1e-4) & (df["lower_limit"] > 0))]
        df = df.join(stt.rename("susp"))
        df = df[df["susp"].fillna(0) == 0]
        df = df.drop(index=[i for i in stuck if i in df.index], errors="ignore")
        if min_list_days > 0:
            la = np.array([days_since(i, T, list_d) for i in df.index])
            df = df[np.isfinite(la) & (la >= min_list_days)]
        pick = set(df["mcap"].nsmallest(len(want)).index)
        inter = len(pick & want)
        stats.append((T, len(want), inter))
        if inter < len(want):
            rank = df["mcap"].rank()
            for m in want - pick:
                diffs.append((T, "果仁有", m, float(rank.get(m, np.nan)),
                              days_since(m, T, list_d), days_since(m, T, delist_d)))
            for e in pick - want:
                diffs.append((T, "我们有", e, float(rank.get(e, np.nan)),
                              days_since(e, T, list_d), days_since(e, T, delist_d)))
    sdf = pd.DataFrame(stats, columns=["T", "n", "inter"])
    ov = sdf["inter"].sum() / sdf["n"].sum()
    full = (sdf["inter"] == sdf["n"]).mean()
    lines = [f"days={len(sdf)}  股票·日重合率={ov:.4%}  全对天数占比={full:.2%}  min_list_days={min_list_days}",
             f"平均每天差异股数={(sdf['n'] - sdf['inter']).mean():.2f}"]
    byy = sdf.assign(y=sdf["T"] // 10000).groupby("y").apply(
        lambda g: g["inter"].sum() / g["n"].sum(), include_groups=False)
    lines.append("按年重合率: " + "  ".join(f"{y}:{v:.2%}" for y, v in byy.items()))
    ddf = pd.DataFrame(diffs, columns=["T", "which", "inst", "rank_in_ours", "list_age", "delist_age"])
    if len(ddf):
        os.makedirs(OUT, exist_ok=True)
        ddf.to_csv(os.path.join(OUT, "pool_diff.csv"), index=False)
        for w in ("果仁有", "我们有"):
            g = ddf[ddf["which"] == w]
            if not len(g):
                continue
            la = g["list_age"]
            lines.append(f"{w}: {len(g)} 条  list_age 分位[10,50,90]={la.quantile([.1, .5, .9]).round(0).tolist()}"
                         f"  list_age<365 占 {(la < 365).mean():.2%}  距退市<60d 占 {(g['delist_age'] > -60).mean():.2%}")
        miss = ddf[ddf["which"] == "果仁有"]
        lines.append(f"果仁有我们没有: 查无此股占 {miss['rank_in_ours'].isna().mean():.2%}, "
                     f"rank>N 占 {(miss['rank_in_ours'] > 0).mean():.2%}")
    return {"factor": "pool", "rate": ov, "loose": full, "lines": lines}


FACTORS = {
    "close": f_close, "mcap": f_mcap, "mr_bal": f_mr_bal, "ms_bal": f_ms_bal,
    "st": f_st, "susp": f_susp, "limit": f_limit,
    "rev_ttm": f_rev_ttm, "ni_ttm": f_ni_ttm,
    "pe": f_pe, "ps": f_ps, "pcf": f_pcf, "pb": f_pb, "roe_roa": f_roe_roa,
    "dy": f_dy, "industry": f_industry,
    "loss2y": f_loss2y, "g9_fin": f_g9_fin, "trade_warn": f_trade_warn,
    "pool": f_pool,
}


def run_one(name):
    csv, tdays = load_ctx()
    try:
        r = FACTORS[name](csv, tdays)
    except Exception as e:
        import traceback
        return [{"factor": name, "rate": 0.0, "loose": 0.0,
                 "lines": [f"EXCEPTION: {e}", traceback.format_exc()[-1500:]]}]
    return r if isinstance(r, list) else [r]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("factors", nargs="*", default=list(FACTORS))
    ap.add_argument("--list", action="store_true")
    ap.add_argument("-j", type=int, default=min(len(FACTORS), os.cpu_count()))
    args = ap.parse_args()
    if args.list:
        print(" ".join(FACTORS))
        return
    for f in args.factors:
        assert f in FACTORS, f
    # 预热缓存 (并行建表, 避免 worker 重复建)
    need = set(TABLES) - {"valuation"}
    with ThreadPoolExecutor(8) as ex:
        list(ex.map(load, need))
    results = []
    with ProcessPoolExecutor(args.j) as ex:
        for rs in ex.map(run_one, args.factors):
            results.extend(rs)
            for r in rs:
                print(f"\n=== {r['factor']}  strict={r['rate']:.4%}  loose={r['loose']:.4%}")
                for ln in r["lines"]:
                    print("  " + ln)
                sys.stdout.flush()
    print("\n" + "=" * 60)
    for r in sorted(results, key=lambda x: -x["rate"]):
        print(f"{r['factor']:<24} strict={r['rate']:>9.4%}  loose={r['loose']:>9.4%}")


if __name__ == "__main__":
    main()
