#!/usr/bin/env python3
# 果仁 CSV ↔ 特征张量 统一对账.
#
# 两个来源, 一个因子一行 (对仗):
#   Python 列 = 用 data/YYYY-MM/*.parquet 在 pandas 里重算 (参考实现)
#   C++ 列    = 直读 output/tensor/<feature>.npy (生产张量, 需 config::TENSOR_DUMP_ENABLE=true 跑过 python run.py)
# 没有对应 C++ 张量的因子 C++ 列写 —.
#
# 用法: python3 test/compare.py [factor ...]   (默认全部; --list 看清单; -j N 进程数)
import argparse
import json
import multiprocessing as mp
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
TENSOR = os.path.join(ROOT, "output", "tensor")
META = os.path.join(ROOT, "output", "meta.json")

# ============================================================================
# 表加载 (feather 缓存; date 统一 int32 yyyymmdd)
# ============================================================================

TABLES = {
    "bar1d": (
        "cn_stock_real_bar1d",
        ["date", "instrument", "open", "high", "low", "close"],
    ),
    "shares": (
        "cn_stock_shares",
        ["date", "instrument", "total_shares",
            "a_float_shares", "total_float_shares"],
    ),
    "limit": (
        "cn_stock_limit_price",
        ["date", "instrument", "upper_limit", "lower_limit"],
    ),
    "status": (
        "cn_stock_status",
        ["date", "instrument", "st_status", "is_risk_warning", "suspended"],
    ),
    "margin": (
        "cn_stock_margin_trading_detail",
        ["date", "instrument", "financing_balance", "securities_lending_balance"],
    ),
    "ttm": (
        "cn_stock_financial_ttm_shift",
        [
            "date",
            "instrument",
            "report_date",
            "shift",
            "total_operating_revenue_ttm",
            "net_profit_to_parent_shareholders_ttm",
            "net_profit_ttm",
            "net_cffoa_ttm",
        ],
    ),
    "balance": (
        "cn_stock_financial_balance_general_pit",
        [
            "date",
            "instrument",
            "report_date",
            "total_owner_equity",
            "total_equity_to_parent_shareholders",
            "total_assets",
        ],
    ),
    "income": (
        "cn_stock_financial_income_general_pit",
        [
            "date",
            "instrument",
            "report_date",
            "fs_quarter_index",
            "net_profit_to_parent_shareholders",
            "net_profit",
            "total_profit",
            "total_operating_revenue",
        ],
    ),
    "dividend": (
        "cn_stock_dividend",
        [
            "date",
            "instrument",
            "report_date",
            "publish_date",
            "cash_before_tax",
            "cash_after_tax",
            "register_date",
            "ex_date",
        ],
    ),
    "indcomp": (
        "cn_stock_industry_component",
        [
            "date",
            "instrument",
            "industry",
            "industry_level1_name",
            "industry_level2_name",
        ],
    ),
    "indchg": (
        "cn_stock_industry_change",
        [
            "date",
            "instrument",
            "industry",
            "industry_level",
            "industry_name",
            "change_flag",
        ],
    ),
    "forecast": (
        "forecast",
        [
            "ts_code",
            "ann_date",
            "end_date",
            "type",
            "last_parent_net",
            "net_profit_min",
            "net_profit_max",
        ],
    ),
    "notes": (
        "cn_stock_financial_notes_shift",
        [
            "date",
            "instrument",
            "report_date",
            "shift",
            "nonrecurring_income_to_owner_ttm",
        ],
    ),
    "tdays": ("all_trading_days", None),
    "valuation": (
        "cn_stock_valuation",
        [
            "date",
            "instrument",
            "total_market_cap",
            "pe_ttm",
            "pb",
            "ps_ttm",
            "pcf_op_ttm",
            "dividend_yield_ratio",
        ],
    ),
}


def _to_int_date(s: pd.Series) -> np.ndarray:
    if s.dtype == object:  # tushare "YYYYMMDD" string
        return s.astype("int64").astype("int32").values
    dt = pd.to_datetime(s)
    return (dt.dt.year * 10000 + dt.dt.month * 100 + dt.dt.day).astype("int32").values


def months() -> list[str]:
    return sorted(m for m in os.listdir(DATA) if len(m) == 7 and m[4] == "-")


_TABLE_MEM = {}  # 进程内表缓存; main 里预热后, fork worker 经 COW 只读共享, 不再各自 re-read feather


def load(key: str) -> pd.DataFrame:
    if key in _TABLE_MEM:
        return _TABLE_MEM[key]
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, key + ".feather")
    name, cols = TABLES[key]
    if os.path.exists(path):
        df = pd.read_feather(path)
        # TABLES 加列后旧缓存会缺列, 静默用旧 schema 会让 worker 报假结果 → 重建
        if cols is None or set(cols).issubset(df.columns):
            _TABLE_MEM[key] = df
            return df
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
        if c in (
            "date",
            "report_date",
            "publish_date",
            "ex_date",
            "ann_date",
            "end_date",
        ):
            df[c] = _to_int_date(df[c])
    tmp = path + ".tmp"
    df.to_feather(tmp)
    os.replace(tmp, path)
    _TABLE_MEM[key] = df
    return df


# ============================================================================
# 公共上下文: 果仁 CSV + 交易日轴
# ============================================================================


def to_inst(code: str) -> str:
    return code + (".SH" if code[0] == "6" else ".SZ")


def load_basic_info():
    df = pq.read_table(
        os.path.join(DATA, "_meta", "cn_stock_basic_info.parquet"),
        columns=["instrument", "list_date", "delist_date", "list_sector"],
    ).to_pandas()
    for c in ("list_date", "delist_date"):
        dt = pd.to_datetime(df[c])
        df[c] = (
            (dt.dt.year * 10000 + dt.dt.month * 100 + dt.dt.day)
            .fillna(0)
            .astype("int32")
        )
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
    pmap = {int(t): (int(tdays[i - 1]) if i >= 1 else 0)
            for t, i in zip(uts, idx)}
    p2map = {int(t): (int(tdays[i - 2]) if i >= 2 else 0)
             for t, i in zip(uts, idx)}
    csv["P"] = csv["T"].map(pmap).astype("int32")
    csv["P2"] = csv["T"].map(p2map).astype("int32")
    return csv, tdays


# ============================================================================
# 对比工具
# ============================================================================


def numeric_report(name, csv, ours, theirs, atol, note=""):
    m = pd.DataFrame(
        {"T": csv["T"], "inst": csv["inst"], "ours": ours, "theirs": theirs}
    )
    nodata = m["ours"].isna() & m["theirs"].notna()
    both = m["ours"].notna() & m["theirs"].notna()
    d = (m["ours"] - m["theirs"]).abs()
    rel = d / m["theirs"].abs().clip(lower=1e-12)
    strict = both & ((d <= atol) | (rel <= 5e-4))
    loose = both & ((d <= atol) | (rel <= 1e-2))
    lines = [
        f"n={len(m)} nodata={nodata.sum()} ({nodata.mean():.2%})",
        f"strict(±{atol}或0.05%)={strict.sum()/max(both.sum(), 1):.4%}  loose(1%)={loose.sum()/max(both.sum(), 1):.4%}  "
        f"median_rel={rel[both].median():.2e}",
    ]
    bad = m[both & ~loose].copy()
    if len(bad):
        byy = (
            m[both]
            .assign(y=m["T"] // 10000, ok=loose[both])
            .groupby("y")["ok"]
            .agg(["mean", "count"])
        )
        worst = byy[byy["mean"] < 0.999]
        if len(worst):
            lines.append(
                "按年 loose 匹配率(<99.9%的年): "
                + "  ".join(f"{y}:{r['mean']:.2%}" for y,
                            r in worst.iterrows())
            )
        top = bad.assign(rel=rel[bad.index]).nlargest(5, "rel")
        for _, r in top.iterrows():
            lines.append(
                f"  例 {r['T']} {r['inst']} ours={r['ours']:.6g} theirs={r['theirs']:.6g}"
            )
        os.makedirs(OUT, exist_ok=True)
        bad.head(5000).to_csv(os.path.join(
            OUT, f"{name}_mismatch.csv"), index=False)
    if note:
        lines.append(note)
    return {
        "factor": name,
        "rate": strict.sum() / max(both.sum(), 1),
        "loose": loose.sum() / max(both.sum(), 1),
        "lines": lines,
    }


def binary_report(name, csv, ours, theirs, note=""):
    keep = theirs.notna()
    m = pd.DataFrame(
        {
            "T": csv["T"][keep],
            "inst": csv["inst"][keep],
            "ours": ours[keep].fillna(0).astype(int),
            "theirs": theirs[keep].astype(int),
        }
    )
    agree = m["ours"] == m["theirs"]
    tp = ((m.ours == 1) & (m.theirs == 1)).sum()
    fp = ((m.ours == 1) & (m.theirs == 0)).sum()
    fn = ((m.ours == 0) & (m.theirs == 1)).sum()
    lines = [
        f"n={len(m)}  agree={agree.mean():.4%}  csv_1s={int(m.theirs.sum())} ours_1s={int(m.ours.sum())} "
        f"tp={tp} fp={fp} fn={fn}"
    ]
    bad = m[~agree]
    if len(bad):
        for _, r in bad.head(5).iterrows():
            lines.append(
                f"  例 {r['T']} {r['inst']} ours={r['ours']} theirs={r['theirs']}"
            )
        os.makedirs(OUT, exist_ok=True)
        bad.head(5000).to_csv(os.path.join(
            OUT, f"{name}_mismatch.csv"), index=False)
    if note:
        lines.append(note)
    return {"factor": name, "rate": agree.mean(), "loose": agree.mean(), "lines": lines}


def grid_at(csv, table, col, on="P", ffill=False):
    """网格表取值: (date==csv[on], inst) 精确 join; ffill=True 时取 date≤csv[on] 最新行 (停牌沿用)."""
    t = load(table)
    if not ffill:
        m = csv[[on, "inst"]].merge(
            t.rename(columns={"date": on, "instrument": "inst"})[
                [on, "inst", col]],
            on=[on, "inst"],
            how="left",
        )
        return m[col]
    left = csv[[on, "inst"]].copy()
    left["k"] = left[on].astype("int64")
    left = left.sort_values("k", kind="stable").reset_index()
    right = t.rename(columns={"instrument": "inst"})[
        ["date", "inst", col]].copy()
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
    out = pd.merge_asof(
        left,
        right[["k", "inst"] + valcols],
        on="k",
        by="inst",
        allow_exact_matches=False,
    )
    return out.set_index("index").sort_index()[valcols]


def mcap_ours(csv):
    cl = grid_at(csv, "bar1d", "close", ffill=True)
    sh = grid_at(csv, "shares", "total_shares", ffill=True)
    return cl * sh


# ============================================================================
# 全市场截面中性化 (果仁 "中性BP/EP/CP/SP/ROE/ROA/股息率" 实测口径):
#   raw (倒数形式, 负值保留) → winsorize 1%-99% → OLS [1, log(mcap), 申万一级 dummy] 残
#   全市场截面; OLS 用 Frisch-Waugh-Lovell 等价 (行业内 demean + 标量回归) O(n).
#   跨因子共享 panel 缓存 (key = csv 唯一 T 元组), 单进程内复用.
# ============================================================================

_FRAMES_CACHE = {}
_RR_CACHE = {}
_DY_CACHE = {}
_BAL_CACHE = {}
_TTM_CACHE = {}
_GRP_CACHE = {}


def _tkey(csv):
    return tuple(np.sort(csv["T"].unique()))


def _csv_groups(csv):
    """{T: (行 index 数组, inst 数组)}; 一次 groupby, 替代 per-T 全列比较."""
    k = _tkey(csv)
    if k not in _GRP_CACHE:
        _GRP_CACHE[k] = {
            int(T): (g.index.values, g["inst"].values)
            for T, g in csv.groupby("T", sort=False)
        }
    return _GRP_CACHE[k]


def _balance_panel(csv, col):
    k = (col, _tkey(csv))
    if k not in _BAL_CACHE:
        _BAL_CACHE[k] = _pit_panel(load("balance"), csv, col)
    return _BAL_CACHE[k]


def _ttm0_panel(csv, col):
    k = (col, _tkey(csv))
    if k not in _TTM_CACHE:
        ttm0 = load("ttm")
        ttm0 = ttm0[ttm0["shift"] == 0]
        _TTM_CACHE[k] = _pit_panel(ttm0, csv, col)
    return _TTM_CACHE[k]


def _winsor(s):
    f = s.dropna()
    if len(f) < 2:
        return s
    return s.clip(f.quantile(0.01), f.quantile(0.99))


def _neutralize(raw, mc, ind):
    """FWL: y ~ 1 + log(mcap) + 行业 dummy 拼. 返 inst 索引的残差 Series (唯一)."""
    lm = np.log(mc)
    df = pd.concat([raw.rename("y"), lm.rename("lm")], axis=1)
    df["g"] = ind.reindex(df.index)
    m = df.dropna(subset=["y", "lm", "g"])
    if len(m) < 5:
        return pd.Series(dtype=float)
    gm = m.groupby("g")[["y", "lm"]].mean()
    ydm = m["y"] - gm.loc[m["g"], "y"].values
    ldm = m["lm"] - gm.loc[m["g"], "lm"].values
    den = (ldm**2).sum()
    b = (ydm * ldm).sum() / den if den > 0 else 0.0
    return ydm - b * ldm


def _assign(frames, csv, raw_fn):
    """逐 T 中性化后按 csv 行 (T, inst) 对齐成 ours Series (index=csv.index)."""
    grp = _csv_groups(csv)
    ours = pd.Series(np.nan, index=csv.index, dtype=float)
    for T, (mc, ind, insts) in frames.items():
        sub_idx, sub_inst = grp[T]
        raw = raw_fn(int(T), mc, insts, csv)
        r = _neutralize(_winsor(raw), mc, ind)
        ours.loc[sub_idx] = r.reindex(sub_inst).values
    return ours


def _corr_report(name, csv, ours, theirs):
    """中性化残差用相关性衡量 (果仁中性因子同为残差, 尺度一致). rate=pearson corr."""
    m = pd.DataFrame(
        {
            "T": csv["T"],
            "inst": csv["inst"],
            "ours": pd.to_numeric(ours, errors="coerce"),
            "theirs": pd.to_numeric(theirs, errors="coerce"),
        }
    )
    nodata = m["ours"].isna() & m["theirs"].notna()
    both = m.dropna(subset=["ours", "theirs"])
    corr = both["ours"].corr(both["theirs"]) if len(both) > 2 else 0.0
    rcorr = both["ours"].rank().corr(
        both["theirs"].rank()) if len(both) > 2 else 0.0
    so = both["ours"].std()
    st = both["theirs"].std()
    scale = so / st if st and st > 0 else float("nan")
    lines = [
        f"n={len(m)} nodata={nodata.sum()} ({nodata.mean():.2%})",
        f"pearson_corr={corr:.4f}  rank_corr={rcorr:.4f}  scale(ours/theirs)={scale:.3f}",
        f"ours std={so:.4f}  theirs std={st:.4f}",
    ]
    bad = m.dropna(subset=["ours", "theirs"]).copy()
    bad["d"] = (bad["ours"] - bad["theirs"]).abs()
    for _, r in bad.nlargest(3, "d").iterrows():
        lines.append(
            f"  例 {r['T']} {r['inst']} ours={r['ours']:.6g} theirs={r['theirs']:.6g}"
        )
    return {
        "factor": name,
        "rate": max(0.0, corr),
        "loose": max(0.0, rcorr),
        "lines": lines,
    }


def _full_frames(csv):
    """每个唯一 T → (mc, ind, insts): 全市场在 P 有 close 的 inst + mcap + 申万一级名."""
    k = _tkey(csv)
    if k in _FRAMES_CACHE:
        return _FRAMES_CACHE[k]
    bar = load("bar1d")
    sh = load("shares")
    indc = load("indcomp")
    uts = np.sort(csv["T"].unique())
    tdays = np.sort(load("tdays")["date"].unique())
    pmap = {int(t): int(tdays[np.searchsorted(
        tdays, t, "left") - 1]) for t in uts}
    bar = bar.rename(columns={"instrument": "inst"})
    sh = sh.rename(columns={"instrument": "inst"})
    mc_src = bar.merge(sh, on=["date", "inst"], how="inner")
    mc_src["mc"] = mc_src["close"] * mc_src["total_shares"]
    mc_by_date = {
        int(d): sub.set_index("inst")["mc"] for d, sub in mc_src.groupby("date")
    }
    indc = indc.sort_values(["instrument", "date"], kind="stable")
    indc = indc.drop_duplicates(["instrument", "date"], keep="last")
    ind_parts = []
    for inst, g in indc.groupby("instrument", sort=False):
        g = g.set_index("date")["industry_level1_name"].reindex(
            uts, method="ffill")
        g = g.rename_axis("T").reset_index()
        g["instrument"] = inst
        ind_parts.append(g[["instrument", "T", "industry_level1_name"]])
    ind_long = pd.concat(ind_parts, ignore_index=True)
    ind_by_T = {
        int(T): sub.set_index("instrument")["industry_level1_name"]
        for T, sub in ind_long.groupby("T")
    }
    out = {}
    for T in uts:
        P = pmap[int(T)]
        mc = mc_by_date.get(P, pd.Series(dtype=float))
        ind = ind_by_T.get(int(T), pd.Series(dtype=object))
        out[int(T)] = (mc, ind, mc.index.values)
    _FRAMES_CACHE[k] = out
    return out


def _pit_panel(df, csv, col):
    """df (date, instrument, report_date, col) → {T: Series(col, index=instrument)}.
    口径同 _latest_at: date<=T 内 max(report_date) 的值. 向量化 cummax + ffill, 一次建表缓存."""
    uts = np.sort(csv["T"].unique())
    d = df[["date", "instrument", "report_date", col]].dropna(subset=[col])
    d = d.sort_values(["instrument", "date", "report_date"], kind="stable")
    d["rmax"] = d.groupby("instrument", sort=False)["report_date"].cummax()
    d["active"] = d[col].where(d["report_date"] == d["rmax"])
    d["active"] = d.groupby("instrument", sort=False)["active"].ffill()
    d = d.drop_duplicates(["instrument", "date"], keep="last")
    parts = []
    for inst, g in d.groupby("instrument", sort=False):
        g = g.set_index("date")["active"].reindex(uts, method="ffill")
        g = g.rename_axis("T").reset_index()
        g["instrument"] = inst
        parts.append(g[["instrument", "T", "active"]
                       ].rename(columns={"active": col}))
    long = pd.concat(parts, ignore_index=True)
    p = {int(T): sub.set_index("instrument")[col]
         for T, sub in long.groupby("T")}
    return p


def match_factor(name, csv, ref_col, raw_fn, atol=0.006):
    """raw_fn(T, mc, insts, csv) → Series(raw, index=inst). 全市场中性化后比对 csv[ref_col]."""
    frames = _full_frames(csv)
    ours = _assign(frames, csv, raw_fn)
    return _corr_report(name, csv, ours, csv[ref_col])


def _raw_bp(T, mc, insts, csv):
    eq = _balance_panel(csv, "total_equity_to_parent_shareholders")[T]
    return (eq / mc).reindex(insts)


def _raw_ep(T, mc, insts, csv):
    ni = _ttm0_panel(csv, "net_profit_to_parent_shareholders_ttm")[T]
    return (ni / mc).reindex(insts)


def _raw_cp(T, mc, insts, csv):
    cf = _ttm0_panel(csv, "net_cffoa_ttm")[T]
    return (cf / mc).reindex(insts)


def _raw_sp(T, mc, insts, csv):
    rev = _ttm0_panel(csv, "total_operating_revenue_ttm")[T]
    rev = rev[rev > 0]
    return (rev / mc).reindex(insts)


def _prev_q_n(rd):
    y, md = rd // 10000, rd % 10000
    m2 = {1231: 930, 930: 630, 630: 331, 331: None}[md]
    return (y * 10000 + m2) if m2 else (y - 1) * 10000 + 1231


def _roe_roa_st_n():
    """复刻 _roe_roa_ours 的 per-inst 状态机, 返回全市场事件流 st (instrument, date, roe, roa)."""
    ttm = load("ttm")
    ttm0 = drop_pre_list(ttm[ttm["shift"] == 0])[
        ["date", "instrument", "report_date", "net_profit_ttm"]
    ]
    bal = drop_pre_list(load("balance"))[
        ["date", "instrument", "report_date", "total_owner_equity", "total_assets"]
    ]
    ev = pd.concat([ttm0.assign(src=1), bal.assign(src=0)], ignore_index=True)
    ev = ev.sort_values(["instrument", "date", "src"], kind="stable")
    rows = []
    for inst, g in ev.groupby("instrument", sort=False):
        vals = {}
        anchor = None
        npt = np.nan
        for date, rd, src, np_ttm, eq, ta in zip(
            g["date"].values,
            g["report_date"].values,
            g["src"].values,
            g["net_profit_ttm"].values,
            g["total_owner_equity"].values,
            g["total_assets"].values,
        ):
            if src == 0:
                vals[rd] = (eq, ta)
            else:
                anchor, npt = int(rd), np_ttm
            if anchor is None:
                continue
            ch = [anchor]
            for _ in range(4):
                ch.append(_prev_q_n(ch[-1]))
            es = [vals[r][0] for r in ch if r in vals]
            ts_ = [vals[r][1] for r in ch if r in vals]
            roe = npt / np.mean(es) if len(es) == 5 else np.nan
            roa = npt / np.mean(ts_) if len(ts_) == 5 else np.nan
            rows.append((inst, date, roe, roa))
    return (
        pd.DataFrame(rows, columns=["instrument", "date", "roe", "roa"])
        .groupby(["instrument", "date"], as_index=False)
        .last()
    )


def _ffill_panel(st, csv, valcols):
    """st (instrument, date, valcols) → {T: DataFrame(index=instrument, valcols)} 按 csv T ffill."""
    uts = np.sort(csv["T"].unique())
    st = st.sort_values(["instrument", "date"], kind="stable")
    parts = []
    for inst, g in st.groupby("instrument", sort=False):
        g = g.set_index("date")[valcols].reindex(uts, method="ffill")
        g = g.rename_axis("T").reset_index()
        g["instrument"] = inst
        parts.append(g[["instrument", "T"] + valcols])
    long = pd.concat(parts, ignore_index=True)
    return {
        int(T): sub.set_index("instrument")[valcols] for T, sub in long.groupby("T")
    }


def _roe_roa_panels(csv):
    k = _tkey(csv)
    if k in _RR_CACHE:
        return _RR_CACHE[k]
    st = _roe_roa_st_n()
    p = _ffill_panel(st, csv, ["roe", "roa"])
    _RR_CACHE[k] = p
    return p


def _dy_panels(csv):
    k = _tkey(csv)
    if k in _DY_CACHE:
        return _DY_CACHE[k]
    div = load("dividend")
    div = div[div["cash_before_tax"] > 0].copy()
    sh = load("shares")[["date", "instrument", "total_shares"]].copy()
    div["k"] = div["publish_date"].astype("int64")
    div = div[div["k"] > 0].sort_values("k", kind="stable")
    sh = sh.rename(columns={"date": "k"})
    sh["k"] = sh["k"].astype("int64")
    sh = sh.sort_values("k", kind="stable")
    div = pd.merge_asof(div, sh, on="k", by="instrument")
    div["v"] = div["cash_before_tax"] * div["total_shares"]
    div["pdt"] = pd.to_datetime(
        div["publish_date"].astype(str), format="%Y%m%d"
    ).values.astype("datetime64[D]")
    uts = np.sort(csv["T"].unique())
    ut_dt = pd.to_datetime(uts.astype(str), format="%Y%m%d").values.astype(
        "datetime64[D]"
    )
    ut_lo = ut_dt - np.timedelta64(365, "D")
    parts = []
    for inst, g in div.groupby("instrument", sort=False):
        g = g.sort_values("pdt")
        ps = g["pdt"].values
        vs = g["v"].values
        csum = np.concatenate([[0.0], np.cumsum(vs)])
        i = np.searchsorted(ps, ut_dt, "right")
        i0 = np.searchsorted(ps, ut_lo, "right")
        s = csum[i] - csum[i0]
        parts.append(pd.DataFrame({"instrument": inst, "T": uts, "dy": s}))
    long = pd.concat(parts, ignore_index=True)
    p = {int(T): sub.set_index("instrument")["dy"]
         for T, sub in long.groupby("T")}
    _DY_CACHE[k] = p
    return p


# ============================================================================
# 因子 worker (Python 参考重算)
# ============================================================================


def f_close(csv, tdays):
    ours = grid_at(csv, "bar1d", "close", ffill=True)
    return numeric_report("close", csv, ours, csv["收盘价"], atol=0.006)


def f_mcap(csv, tdays):
    return numeric_report(
        "mcap", csv, mcap_ours(csv) / 1e8, csv["总市值(亿)"], atol=0.006
    )


def f_mr_bal(csv, tdays):
    # 果仁 "昨日" 实际是 T-2 日余额 (T-1 余额 T 日盘前 10 点才入库, 果仁未用)
    ours = (grid_at(csv, "margin", "financing_balance", on="P2") / 1e4).fillna(0.0)
    return numeric_report("mr_bal", csv, ours, csv["昨日融资余额(万)"], atol=0.006)


def f_ms_bal(csv, tdays):
    ours = (grid_at(csv, "margin", "securities_lending_balance", on="P2") / 1e4).fillna(
        0.0
    )
    return numeric_report("ms_bal", csv, ours, csv["昨日融券余额(万)"], atol=0.006)


def f_st(csv, tdays):
    # 果仁 ST标记 = ST ∪ *ST (超集); 星ST标记 = 仅 *ST
    st = grid_at(csv, "status", "st_status", on="T")
    r1 = binary_report(
        "st", csv, (st >= 1).astype(float), csv["ST标记"], note="ST标记=st_status∈{1,2}"
    )
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
    # 果仁的涨跌停 filter 是"一字封板"(全天 O=H=L=C=涨跌停价, 真的买/卖不进),
    #   不是"收盘价触及涨跌停". 实测按后者判会有 4778 条假阳性, 其中 84-91% 是
    #   盘中正常成交、仅收在停板价的行 (例 600419 20150108 O=19.91 L=19.75 C=21.99=涨停).
    #   两边 close / upper_limit / lower_limit 数值完全一致 ⇒ 纯语义差, 非数据差.
    # 注: 项目 C++ 的 limit_up/limit_dn 仍用"收盘价触及"口径 — 那是策略侧的物理
    #   交易约束, 宁可保守; 这里只是为了让本对比校验数据而非语义.
    fresh = csv[~csv["stuck"]].reset_index(drop=True)
    o = grid_at(fresh, "bar1d", "open")
    h = grid_at(fresh, "bar1d", "high")
    lo = grid_at(fresh, "bar1d", "low")
    cl = grid_at(fresh, "bar1d", "close")
    up = grid_at(fresh, "limit", "upper_limit")
    dn = grid_at(fresh, "limit", "lower_limit")
    zeros = pd.Series(0.0, index=fresh.index)

    def sealed(lim):
        return (
            lim.notna()
            & (lim > 0)
            & (o - lim).abs().le(1e-4)
            & (h - lim).abs().le(1e-4)
            & (lo - lim).abs().le(1e-4)
            & (cl - lim).abs().le(1e-4)
        ).astype(float)

    r1 = binary_report("limit_up(非卡单应全0)", fresh, sealed(up), zeros)
    r2 = binary_report("limit_dn(非卡单应全0)", fresh, sealed(dn), zeros)
    return [r1, r2]


def drop_pre_list(ev):
    """丢弃 visible_date < list_date 的财务事件 (镜像 C++ scan_latest_* 的同名守卫).
    BigQuant 在上市日前就把招股书口径的行标为可见 (ttm 10.9% / balance 14.5%),
    且值不可信 (例 300417 上市前 rev_ttm = -1247 万)."""
    ld = load_basic_info()["list_date"]
    keep = ev["instrument"].map(ld)
    return ev[keep.notna() & (ev["date"] >= keep)]


def _ttm_latest(csv, col, positive=False):
    ttm = load("ttm")
    ev = ttm[ttm["shift"] == 0][["date", "instrument", col]]
    ev = drop_pre_list(ev)
    if positive:  # 营收类: <=0 物理不可能, 是 BigQuant 脏值 (1.35% 事件为负)
        ev = ev[ev[col] > 0]
    return asof_event(csv, ev, [col])[col]


def f_rev_ttm(csv, tdays):
    ours = _ttm_latest(csv, "total_operating_revenue_ttm", positive=True) / 1e4
    return numeric_report("rev_ttm", csv, ours, csv["营业总收入TTM(万)"], atol=0.006)


def f_ni_ttm(csv, tdays):
    ours = _ttm_latest(csv, "net_profit_to_parent_shareholders_ttm") / 1e4
    return numeric_report(
        "ni_ttm", csv, ours, csv["归属于母公司所有者的净利润TTM(万)"], atol=0.006
    )


def f_pe(csv, tdays):
    ni = _ttm_latest(csv, "net_profit_to_parent_shareholders_ttm")
    ours = mcap_ours(csv) / ni.where(ni != 0)
    return numeric_report("pe", csv, ours, csv["市盈率"], atol=0.006)


def f_ps(csv, tdays):
    rev = _ttm_latest(csv, "total_operating_revenue_ttm", positive=True)
    ours = mcap_ours(csv) / rev
    return numeric_report("ps", csv, ours, csv["市销率"], atol=0.006)


def f_pcf(csv, tdays):
    cf = _ttm_latest(csv, "net_cffoa_ttm")
    ours = mcap_ours(csv) / cf.where(cf != 0)
    return numeric_report("pcf", csv, ours, csv["市现率"], atol=0.006)


def _balance_latest(csv, col):
    """balance PIT: visible < T 内 max(report_date) 的最新可见行."""
    bal = drop_pre_list(load("balance")).sort_values(
        ["instrument", "date", "report_date"], kind="stable"
    )
    # 按 (inst) 沿 visible 扫: 维护 max(report_date) 对应值
    out_rows = []
    for inst, g in bal.groupby("instrument", sort=False):
        best_rd = -1
        vals = {}
        for date, rd, v in zip(
            g["date"].values, g["report_date"].values, g[col].values
        ):
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
    r1 = numeric_report(
        "pb(含少数)", csv, mc / eq_all.where(eq_all != 0), csv["市净率"], atol=0.006
    )
    r2 = numeric_report(
        "pb(归母)", csv, mc / eq_par.where(eq_par != 0), csv["市净率"], atol=0.006
    )
    return [r1, r2]


def _prev_q(rd):
    y, md = rd // 10000, rd % 10000
    m2 = {1231: 930, 930: 630, 630: 331, 331: None}[md]
    return (y * 10000 + m2) if m2 else (y - 1) * 10000 + 1231


_RR_OURS_CACHE = {}


def _roe_roa_ours(csv):
    """果仁口径 (实测): ROE = net_profit_ttm(含少数) / TTM窗口5季度点平均 total_owner_equity;
       ROA 同分子 / 平均 total_assets. 合并 ttm+balance 事件流, per-inst 状态机后 asof.

    这里刻意算的是**果仁口径**, 用途是校验数据本身 (比值中位数 0.996, 老股 5% 容差
    下 95.7%) — 证明两边底层财务数据一致. 项目生产口径是**归母/归母**
    (见 README roe_raw): 分子 net_profit_to_parent_shareholders_ttm, 分母
    total_equity_to_parent_shareholders 的同一 5 点平均. 分母的"TTM 窗口平均"这一
    点两边一致 (教科书 ROE = NI / average equity, 与果仁无关, 已并入生产实现);
    含少数 vs 归母的分子分母选择是果仁自己的口径, 不追."""
    k = _tkey(csv)
    if k not in _RR_OURS_CACHE:
        _RR_OURS_CACHE[k] = _roe_roa_ours_compute(csv)
    return _RR_OURS_CACHE[k]


def _roe_roa_ours_compute(csv):
    ttm = load("ttm")
    ttm0 = drop_pre_list(ttm[ttm["shift"] == 0])[
        ["date", "instrument", "report_date", "net_profit_ttm"]
    ]
    bal = drop_pre_list(load("balance"))[
        ["date", "instrument", "report_date", "total_owner_equity", "total_assets"]
    ]
    ev = pd.concat([ttm0.assign(src=1), bal.assign(src=0)], ignore_index=True)
    ev = ev.sort_values(["instrument", "date", "src"], kind="stable")

    rows = []
    for inst, g in ev.groupby("instrument", sort=False):
        vals = {}
        anchor, npt = None, np.nan
        for date, rd, src, np_ttm, eq, ta in zip(
            g["date"].values,
            g["report_date"].values,
            g["src"].values,
            g["net_profit_ttm"].values,
            g["total_owner_equity"].values,
            g["total_assets"].values,
        ):
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
    st = (
        pd.DataFrame(rows, columns=["instrument", "date", "roe", "roa"])
        .groupby(["instrument", "date"], as_index=False)
        .last()
    )
    out = asof_event(csv, st, ["roe", "roa"])
    return out["roe"], out["roa"]


def f_roe(csv, tdays):
    roe, _roa = _roe_roa_ours(csv)
    note = (
        "算的是果仁口径(含少数净利TTM/5季度点平均)以校验数据: 比值中位数 0.996, "
        "老股 5% 容差 95.7% ⇒ 数据一致, 差异全在平均法细节. "
        "生产口径是归母/归母 + 同样的 5 点平均, 刻意不追果仁的含少数口径"
    )
    return numeric_report("roe", csv, roe, csv["净资产收益率"], atol=0.00011, note=note)


def f_roa(csv, tdays):
    _roe, roa = _roe_roa_ours(csv)
    note = (
        "算的是果仁口径(含少数净利TTM/5季度点平均)以校验数据: 比值中位数 0.996, "
        "老股 5% 容差 95.7% ⇒ 数据一致, 差异全在平均法细节. "
        "生产口径是归母/归母 + 同样的 5 点平均, 刻意不追果仁的含少数口径"
    )
    return numeric_report("roa", csv, roa, csv["资产回报率"], atol=0.00011, note=note)


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
    tsd = pd.to_datetime(csv["T"].astype(str), format="%Y%m%d").values.astype(
        "datetime64[D]"
    )
    by_inst = {k: g for k, g in div.groupby("instrument")}
    res = np.zeros(len(csv))
    for i, (inst, td) in enumerate(zip(csv["inst"].values, tsd)):
        g = by_inst.get(inst)
        if g is None:
            continue
        ad = pd.to_datetime(
            g["publish_date"].astype(str), format="%Y%m%d", errors="coerce"
        ).values.astype("datetime64[D]")
        w = (ad <= td) & (ad > td - np.timedelta64(365, "D"))
        res[i] = (g["cash_before_tax"].values[w] *
                  g["total_shares"].values[w]).sum()
    ours = pd.Series(res, index=csv.index) / mc
    note = "口径=Σ(税前每股×公告日股本, 预案公告∈(T-365,T])/当前总市值; 残差疑似果仁用实施公告日(缺此字段)"
    return numeric_report("dy", csv, ours, csv["股息率TTM"], atol=0.00011, note=note)


def _industry_got(csv, version, level=1):
    col = f"industry_level{level}_name"
    comp = load("indcomp")
    comp = comp[comp["industry"] == version][["date", "instrument", col]]
    chg = load("indchg")
    chg = chg[
        (chg["industry"] == version)
        & (chg["industry_level"] == level)
        & (chg["change_flag"] == 1)
    ]
    chg = chg.rename(columns={"industry_name": col})[
        ["date", "instrument", col]]
    ev = pd.concat([comp, chg], ignore_index=True).sort_values(
        ["instrument", "date"], kind="stable"
    )
    return asof_event(csv, ev, [col])[col]


def f_industry(csv, tdays):
    # 果仁 2021.8 起用申万2021 (发布日 2021-07-30, 实测 100% 对上); 之前 = sw2014 结构但显示新版名
    # → 用 sw2014 L2→果仁名 学习映射
    CUTOVER = 20210801
    got21 = _industry_got(csv, "sw2021")
    l1_14 = _industry_got(csv, "sw2014", 1)
    l2_14 = _industry_got(csv, "sw2014", 2)
    pre = csv["T"] < CUTOVER
    key = l2_14.where(l2_14.notna(), l1_14)
    learn = pd.DataFrame({"key": key[pre], "gr": csv["行业分类"][pre]}).dropna()
    mode = learn.groupby("key")["gr"].agg(lambda s: s.mode().iat[0])
    consist = (learn["gr"] == learn["key"].map(mode)).mean()
    got = key.map(mode).where(pre, got21)
    ok = got == csv["行业分类"]
    both = got.notna()
    lines = [
        f"n={len(csv)} nodata={(~both).sum()}  agree={ok[both].mean():.4%}  (切换日 {CUTOVER})",
        f"pre-2022 映射: sw2014-L2→果仁名 共 {len(mode)} 键, 键内一致率 {consist:.4%} (由数据学得)",
    ]
    m = csv[both].assign(y=csv["T"][both] // 10000, ok=ok[both])
    byy = m.groupby("y")["ok"].mean()
    worst = byy[byy < 0.999]
    if len(worst):
        lines.append(
            "按年(<99.9%): " +
            "  ".join(f"{y}:{v:.2%}" for y, v in worst.items())
        )
    bad = csv[both & ~ok].assign(ours=got[both & ~ok])
    if len(bad):
        pairs = bad.groupby(["ours", "行业分类"]).size().nlargest(8)
        lines.append(
            "常见错对 (ours→果仁): "
            + "  ".join(f"{a}→{b}:{n}" for (a, b), n in pairs.items())
        )
        os.makedirs(OUT, exist_ok=True)
        bad[["T", "inst", "ours", "行业分类"]].head(5000).to_csv(
            os.path.join(OUT, "industry_mismatch.csv"), index=False
        )
    return {
        "factor": "industry",
        "rate": ok[both].mean(),
        "loose": ok[both].mean(),
        "lines": lines,
    }


def f_loss2y(csv, tdays):
    fc = load("forecast")
    fc = fc[
        (fc["end_date"] % 10000 // 100 == 12)
        & fc["type"].isin(["首亏", "续亏"])
        & (fc["last_parent_net"] < 0)
    ]
    inc = load("income")
    ann = inc[inc["fs_quarter_index"] == 4]
    fin_first = ann.groupby(["instrument", "report_date"])[
        "date"
    ].min()  # 正式年报首次可见日
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
    return binary_report(
        "loss2y", csv, pd.Series(ours, index=csv.index), csv["预期连续两年亏损"]
    )


def f_g9_fin(csv, tdays):
    # 国九条财务 = "预期"口径: 年度预告窗口内 (公告后→正式年报披露/4.30 止)
    #   (预告净利下限<0 ∨ 扣非TTM<0) ∧ rev<阈 (主板3亿/创科1亿, 现行规则全历史回填)
    fc = load("forecast")
    fc = fc[fc["end_date"] % 10000 // 100 == 12].copy()
    fc["pred_neg"] = np.where(
        fc["net_profit_min"].notna(),
        fc["net_profit_min"] < 0,
        fc["type"].isin(["首亏", "续亏"]),
    )
    inc = load("income")
    ann = inc[inc["fs_quarter_index"] == 4]
    fin_first = ann.groupby(["instrument", "report_date"])["date"].min()
    csv_i = csv.reset_index(drop=True)

    def window_flag(sub):
        out = np.zeros(len(csv_i), bool)
        by = {k: g for k, g in sub.groupby("ts_code")}
        tarr, iarr = csv_i["T"].values, csv_i["inst"].values
        for i in range(len(csv_i)):
            g = by.get(iarr[i])
            if g is None:
                continue
            for ad, ed in zip(g["ann_date"].values, g["end_date"].values):
                if not (ad < tarr[i]) or tarr[i] >= (ed // 10000 + 1) * 10000 + 430:
                    continue
                fv = fin_first.get((iarr[i], ed), None)
                if fv is not None and tarr[i] > fv:
                    continue
                out[i] = True
                break
        return out

    pred = window_flag(fc[fc["pred_neg"]])
    anyfc = window_flag(fc)
    np_ttm = _ttm_latest(csv_i, "net_profit_to_parent_shareholders_ttm").values
    notes = load("notes")
    nr = notes[notes["shift"] == 0][
        ["date", "instrument", "nonrecurring_income_to_owner_ttm"]
    ]
    nrv = asof_event(csv_i, nr, ["nonrecurring_income_to_owner_ttm"])[
        "nonrecurring_income_to_owner_ttm"
    ].values
    kf_neg = np.isfinite(np_ttm - nrv) & (np_ttm - nrv < 0)

    rev_ttm = _ttm_latest(
        csv_i, "total_operating_revenue_ttm", positive=True).values
    ann2 = ann.sort_values(["instrument", "date"], kind="stable").copy()
    ann2["rdm"] = ann2.groupby("instrument")["report_date"].cummax()
    ev = ann2[ann2["report_date"] == ann2["rdm"]]
    va = asof_event(
        csv_i,
        ev[["date", "instrument", "total_operating_revenue"]],
        ["total_operating_revenue"],
    )
    rev = np.where(np.isfinite(rev_ttm), rev_ttm,
                   va["total_operating_revenue"].values)
    thr = np.where(csv_i["inst"].str.startswith(("30", "68")), 1e8, 3e8)
    ours = ((pred | (anyfc & kf_neg)) & (rev < thr)).astype(float)
    return binary_report(
        "g9_fin",
        csv,
        pd.Series(ours, index=csv.index),
        csv["国九条财务退市预警"],
        note="预告窗口 ∧ (预告净利下限<0 ∨ 扣非TTM<0) ∧ rev<3亿(主板)/1亿(创科); "
        "残差=果仁或用预告扣非原文(缺数据)",
    )


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
    return (
        t.pivot_table(index="date", columns="instrument",
                      values=col, aggfunc="last")
        .reindex(tdays)
        .ffill()
    )


def f_trade_warn(csv, tdays):
    cl = _pivot_csv_insts(csv, tdays, "bar1d", "close")
    sh = _pivot_csv_insts(csv, tdays, "shares", "total_shares")
    mc = cl * sh
    mainb = pd.Series(
        {
            c: c[0] in ("6",)
            and not c.startswith("68")
            or c.startswith(("000", "001", "002", "003"))
            for c in cl.columns
        }
    )
    thr = mainb.map({True: 5e8, False: 3e8})
    low_p = (cl < 1.0) & cl.notna()
    low_mc = mc.lt(thr, axis=1) & mc.notna()
    r_trade = binary_report(
        "trade_warn(=20D面值<1)",
        csv,
        _roll20_all(csv, tdays, low_p).astype(float),
        csv["交易类退市预警"],
    )
    r_g9t = binary_report(
        "g9_trade(=20D市值<阈)",
        csv,
        _roll20_all(csv, tdays, low_mc).astype(float),
        csv["国九条交易退市预警"],
    )
    return [r_trade, r_g9t]


def f_pool(csv, tdays, min_list_days=0):
    """截面攻坚: 全市场 mcap 升序 + 默认 filter + 剔风险警示 → 每日 bottom-N vs 果仁当日新调入 (非卡单).
    卡单持仓从两边剔除 (不受 filter/rank 约束).
    已否定的假设: 流通市值/复合排名/市值下限/上市天数/两融标的/科创板/审计意见/近20日封板停牌;
    残差 = 果仁 universe 存在未知硬排除 (74% 被跳过的干净股全年未被持有), 待与人确认策略筛选条件."""
    bar = load("bar1d")
    sh = load("shares")[["date", "instrument", "total_shares"]]
    lim = load("limit")
    st = load("status")[["date", "instrument", "suspended", "is_risk_warning"]]
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
        s = (
            sh_g[P].set_index("instrument")["total_shares"]
            if P in sh_g
            else pd.Series(dtype=float)
        )
        u = lim_g[P].set_index("instrument") if P in lim_g else None
        stt = st_g[T].set_index("instrument") if T in st_g else None
        df = pd.DataFrame({"close": b}).join(s.rename("shares"))
        df["mcap"] = df["close"] * df["shares"]
        df = df[df["mcap"].notna() & (df["mcap"] > 0)]
        if u is not None:
            df = df.join(u)
            df = df[
                ~((df["close"] >= df["upper_limit"] - 1e-4)
                  & (df["upper_limit"] > 0))
            ]
            df = df[
                ~((df["close"] <= df["lower_limit"] + 1e-4)
                  & (df["lower_limit"] > 0))
            ]
        if stt is not None:
            df = df.join(stt[["suspended", "is_risk_warning"]])
            df = df[df["suspended"].fillna(0) == 0]
            df = df[df["is_risk_warning"].fillna(0) != 1]
        df = df.drop(
            index=[i for i in stuck if i in df.index], errors="ignore")
        if min_list_days > 0:
            la = np.array([days_since(i, T, list_d) for i in df.index])
            df = df[np.isfinite(la) & (la >= min_list_days)]
        pick = set(df["mcap"].nsmallest(len(want)).index)
        inter = len(pick & want)
        stats.append((T, len(want), inter))
        if inter < len(want):
            rank = df["mcap"].rank()
            for m in want - pick:
                diffs.append(
                    (
                        T,
                        "果仁有",
                        m,
                        float(rank.get(m, np.nan)),
                        days_since(m, T, list_d),
                        days_since(m, T, delist_d),
                    )
                )
            for e in pick - want:
                diffs.append(
                    (
                        T,
                        "我们有",
                        e,
                        float(rank.get(e, np.nan)),
                        days_since(e, T, list_d),
                        days_since(e, T, delist_d),
                    )
                )
    sdf = pd.DataFrame(stats, columns=["T", "n", "inter"])
    ov = sdf["inter"].sum() / sdf["n"].sum()
    full = (sdf["inter"] == sdf["n"]).mean()
    lines = [
        f"days={len(sdf)}  股票·日重合率={ov:.4%}  全对天数占比={full:.2%}  min_list_days={min_list_days}",
        f"平均每天差异股数={(sdf['n'] - sdf['inter']).mean():.2f}",
    ]
    byy = (
        sdf.assign(y=sdf["T"] // 10000)
        .groupby("y")
        .apply(lambda g: g["inter"].sum() / g["n"].sum(), include_groups=False)
    )
    lines.append("按年重合率: " + "  ".join(f"{y}:{v:.2%}" for y, v in byy.items()))
    ddf = pd.DataFrame(
        diffs, columns=["T", "which", "inst",
                        "rank_in_ours", "list_age", "delist_age"]
    )
    if len(ddf):
        os.makedirs(OUT, exist_ok=True)
        ddf.to_csv(os.path.join(OUT, "pool_diff.csv"), index=False)
        for w in ("果仁有", "我们有"):
            g = ddf[ddf["which"] == w]
            if not len(g):
                continue
            la = g["list_age"]
            lines.append(
                f"{w}: {len(g)} 条  list_age 分位[10,50,90]={la.quantile([.1, .5, .9]).round(0).tolist()}"
                f"  list_age<365 占 {(la < 365).mean():.2%}  距退市<60d 占 {(g['delist_age'] > -60).mean():.2%}"
            )
        miss = ddf[ddf["which"] == "果仁有"]
        lines.append(
            f"果仁有我们没有: 查无此股占 {miss['rank_in_ours'].isna().mean():.2%}, "
            f"rank>N 占 {(miss['rank_in_ours'] > 0).mean():.2%}"
        )
    return {"factor": "pool", "rate": ov, "loose": full, "lines": lines}


# ---- 中性因子 (全市场截面中性化, _corr_report 出相关性) ----


def f_nbp(csv, tdays):
    return match_factor("中性BP", csv, "中性BP", _raw_bp)


def f_nep(csv, tdays):
    return match_factor("中性EP", csv, "中性EP", _raw_ep)


def f_ncp(csv, tdays):
    return match_factor("中性CP", csv, "中性CP", _raw_cp)


def f_nsp(csv, tdays):
    return match_factor("中性SP", csv, "中性SP", _raw_sp)


def f_nroe(csv, tdays):
    panels = _roe_roa_panels(csv)
    frames = _full_frames(csv)
    grp = _csv_groups(csv)
    ours = pd.Series(np.nan, index=csv.index, dtype=float)
    for T, (mc, ind, insts) in frames.items():
        sub_idx, sub_inst = grp[T]
        r = _neutralize(_winsor(panels[T]["roe"]), mc, ind)
        ours.loc[sub_idx] = r.reindex(sub_inst).values
    return _corr_report("中性ROE", csv, ours, csv["中性ROE"])


def f_nroa(csv, tdays):
    panels = _roe_roa_panels(csv)
    frames = _full_frames(csv)
    grp = _csv_groups(csv)
    ours = pd.Series(np.nan, index=csv.index, dtype=float)
    for T, (mc, ind, insts) in frames.items():
        sub_idx, sub_inst = grp[T]
        r = _neutralize(_winsor(panels[T]["roa"]), mc, ind)
        ours.loc[sub_idx] = r.reindex(sub_inst).values
    return _corr_report("中性ROA", csv, ours, csv["中性ROA"])


def f_ndy(csv, tdays):
    panels = _dy_panels(csv)
    frames = _full_frames(csv)
    grp = _csv_groups(csv)
    ours = pd.Series(np.nan, index=csv.index, dtype=float)
    for T, (mc, ind, insts) in frames.items():
        sub_idx, sub_inst = grp[T]
        r = _neutralize(_winsor(panels[T] / mc), mc, ind)
        ours.loc[sub_idx] = r.reindex(sub_inst).values
    return _corr_report("中性股息率", csv, ours, csv["中性股息率"])


# ============================================================================
# C++ 生产张量对账 (直读 output/tensor/*.npy, 与 Python 参考同指标并列)
#   前置: cpp/include/config_main.hpp 里 TENSOR_DUMP_ENABLE=true, 且跑过 python run.py
# ============================================================================

# 果仁行 (T, inst) 该对张量的哪一天不是先验已知: C++ 各 feature CUTOFF 不同
#   (close_raw 自带 -1 滞后; 两融 CUTOFF=0 不滞后), 果仁自身也不统一
#   (收盘价 T-1, 两融 T-2). 故三种锚点全试, 取最优; 全部打印以透明.
_CPP_ANCHORS = ["T", "P", "P2"]

# C++ feature → (单位换算除数, 指标, atol, loose 相对容差)
#   value: 与 Python 参考同口径的 strict/loose 命中率
#   rank : C++ 中性因子经 z + pct_rank 输出 [0,1] 分位, 果仁给裸残差,
#          只有逐日 spearman 可比 (取均值)
#   roe/roa 的 loose 放 5%: 分母 5 季末点平均, 果仁的平均法细节无从复刻.
_CPP_SPEC = {
    "close_raw": (1.0, "value", 0.006, 1e-2),
    "mcap_raw": (1e8, "value", 0.006, 1e-2),
    "mr_bal_raw": (1e4, "value", 0.006, 1e-2),
    "ms_bal_raw": (1e4, "value", 0.006, 1e-2),
    "pe_raw": (1.0, "value", 0.006, 1e-2),
    "pb_raw": (1.0, "value", 0.006, 1e-2),
    "ps_raw": (1.0, "value", 0.006, 1e-2),
    "pcf_raw": (1.0, "value", 0.006, 1e-2),
    "rev_raw": (1e4, "value", 0.006, 1e-2),
    # roe_raw / roa_raw 在 C++ 里已 ×100 输出百分数, 果仁列是小数 → 除回 100
    "roe_raw": (100.0, "value", 0.00011, 5e-2),
    "roa_raw": (100.0, "value", 0.00011, 5e-2),
    "dy_raw": (1.0, "value", 0.00011, 1e-2),
    "bp_ttm3": (1.0, "rank", 0.0, 0.0),
    "ep_ttm12": (1.0, "rank", 0.0, 0.0),
    "cp_ttm12": (1.0, "rank", 0.0, 0.0),
    "sp_ttm12": (1.0, "rank", 0.0, 0.0),
    "roe_ttm12": (1.0, "rank", 0.0, 0.0),
    "roa_ttm12": (1.0, "rank", 0.0, 0.0),
    "dy_ttm12": (1.0, "rank", 0.0, 0.0),
}

# 用 0 表示"未上市/已退市"的 feature → 0 视作缺失
_CPP_ZERO_MISSING = {"close_raw", "mcap_raw", "fmcap_raw", "share_raw"}

# 中性因子 → 其原料 raw. raw 缺 ⇒ 残差 NaN ⇒ neutral_pipeline 用截面均值兜底 (≈0.5).
#   那种格子不含本股信息, 计进秩相关等于拿常数去对果仁真实值, 白扣分.
#   打分时按本表剔掉填充格 (与 Python 参考侧 dropna 后算相关对齐).
_CPP_FILLED_FROM = {
    "bp_ttm3": "pb_raw",
    "ep_ttm12": "pe_raw",
    "cp_ttm12": "pcf_raw",
    "sp_ttm12": "ps_raw",
    "roe_ttm12": "roe_raw",
    "roa_ttm12": "roa_raw",
    "dy_ttm12": "dy_raw",
}


def _cpp_load_axes():
    assert os.path.exists(META), f"缺 {META}, 先跑 python run.py"
    m = json.load(open(META))
    return (
        {s: i for i, s in enumerate(m["dates"])},
        {s: i for i, s in enumerate(m["codes"])},
    )


def _cpp_load_mat(name):
    p = os.path.join(TENSOR, name + ".npy")
    if not os.path.exists(p):
        return None  # 节点不在当前计算图 (无策略引用) → 无 dump; 调用方判 None 跳过
    return np.load(p, mmap_mode="r")  # (n_a, n_d) float32, a-major


def _cpp_pick(mat, a_idx, d_idx, zero_missing):
    """按 (a, d) 取值; 越界/0-哨兵 → NaN."""
    ok = (a_idx >= 0) & (d_idx >= 0)
    out = np.full(len(a_idx), np.nan, dtype=np.float64)
    if ok.any():
        out[ok] = mat[a_idx[ok], d_idx[ok]]
    out[~np.isfinite(out)] = np.nan
    if zero_missing:
        out[out == 0.0] = np.nan
    return out


def _cpp_score_value(ours, theirs, atol, rtol):
    m = pd.DataFrame({"o": ours, "t": theirs}).dropna()
    if len(m) == 0:
        return 0.0, 0.0, np.nan, 0
    d = (m["o"] - m["t"]).abs()
    rel = d / m["t"].abs().clip(lower=1e-12)
    strict = ((d <= atol) | (rel <= 5e-4)).mean()
    loose = ((d <= atol) | (rel <= rtol)).mean()
    ratio = (m["o"] / m["t"].replace(0, np.nan)).median()
    return strict, loose, ratio, len(m)


def _cpp_score_rank(ours, theirs, dates):
    """逐日 spearman 再取均值: C++ 输出是逐日分位, 跨日 pool 无意义."""
    m = pd.DataFrame({"o": ours, "t": theirs, "d": dates}).dropna()
    if len(m) == 0:
        return 0.0, 0.0, np.nan, 0
    cs = []
    for _, g in m.groupby("d"):
        if len(g) < 5 or g["o"].nunique() < 2 or g["t"].nunique() < 2:
            continue
        c = g["o"].rank().corr(g["t"].rank())
        if np.isfinite(c):
            cs.append(c)
    mean_rho = float(np.mean(cs)) if cs else 0.0
    pooled = m["o"].rank().corr(m["t"].rank())
    return mean_rho, (pooled if np.isfinite(pooled) else 0.0), np.nan, len(m)


def _cpp_run_one(feature, col, csv, d_of, a_of):
    """读 C++ 张量对果仁列, 返与 Python 参考同结构的 report dict."""
    div, kind, atol, rtol = _CPP_SPEC[feature]
    theirs = pd.to_numeric(csv[col], errors="coerce").values
    mat = _cpp_load_mat(feature)
    if mat is None:
        return {"factor": feature, "rate": float("nan"), "loose": float("nan"),
                "lines": ["— (节点不在当前计算图, 无 dump)"]}
    src = (
        _cpp_load_mat(_CPP_FILLED_FROM[feature])
        if feature in _CPP_FILLED_FROM
        else None
    )
    a_idx = csv["inst"].map(lambda s: a_of.get(s, -1)).values.astype(np.int64)

    best = None
    all_anchors = {}
    for anchor in _CPP_ANCHORS:
        d_idx = (
            csv[anchor]
            .astype(str)
            .map(lambda s: d_of.get(s, -1))
            .values.astype(np.int64)
        )
        ours = _cpp_pick(mat, a_idx, d_idx, feature in _CPP_ZERO_MISSING) / div
        if src is not None:
            ours[np.isnan(_cpp_pick(src, a_idx, d_idx, False))] = np.nan
        if kind == "value":
            s, l, ratio, n = _cpp_score_value(ours, theirs, atol, rtol)
        else:
            s, l, ratio, n = _cpp_score_rank(ours, theirs, csv[anchor].values)
        all_anchors[anchor] = s
        if best is None or s > best[1]:
            best = (anchor, s, l, ratio, n)

    anchor, s, l, ratio, n = best
    lines = [
        f"锚点={anchor} (三锚: "
        + " ".join(f"{k}={v:.2%}" for k, v in all_anchors.items())
        + ")",
        f"n={n} 缺={1 - n / max(len(csv), 1):.2%}",
    ]
    if np.isfinite(ratio):
        lines.append(f"中位比值={ratio:.4f}")
    return {"factor": feature, "rate": s, "loose": l, "lines": lines}


# ============================================================================
# 统一因子表 — 一个因子一行, Python 列 + C++ 列并列 (对仗)
#   key   = 因子名 (命令行选择 / 输出排序)
#   py    = Python 参考重算 worker (签名 (csv, tdays) → report dict 或 list[dict])
#   cpp   = (C++ 张量特征名, 果仁列) 或 None (无对应张量, C++ 列写 —)
# ============================================================================

FACTORS = {
    "close": (f_close, ("close_raw", "收盘价")),
    "mcap": (f_mcap, ("mcap_raw", "总市值(亿)")),
    "mr_bal": (f_mr_bal, ("mr_bal_raw", "昨日融资余额(万)")),
    "ms_bal": (f_ms_bal, ("ms_bal_raw", "昨日融券余额(万)")),
    "pe": (f_pe, ("pe_raw", "市盈率")),
    "pb": (f_pb, ("pb_raw", "市净率")),
    "ps": (f_ps, ("ps_raw", "市销率")),
    "pcf": (f_pcf, ("pcf_raw", "市现率")),
    "rev_ttm": (f_rev_ttm, ("rev_raw", "营业总收入TTM(万)")),
    "roe": (f_roe, ("roe_raw", "净资产收益率")),
    "roa": (f_roa, ("roa_raw", "资产回报率")),
    "dy": (f_dy, ("dy_raw", "股息率TTM")),
    "中性BP": (f_nbp, ("bp_ttm3", "中性BP")),
    "中性EP": (f_nep, ("ep_ttm12", "中性EP")),
    "中性CP": (f_ncp, ("cp_ttm12", "中性CP")),
    "中性SP": (f_nsp, ("sp_ttm12", "中性SP")),
    "中性ROE": (f_nroe, ("roe_ttm12", "中性ROE")),
    "中性ROA": (f_nroa, ("roa_ttm12", "中性ROA")),
    "中性股息率": (f_ndy, ("dy_ttm12", "中性股息率")),
    # Python-only (无对应 C++ 张量, cpp=None)
    "st": (f_st, (None, "ST标记")),
    "susp": (f_susp, (None, "备注")),
    "limit": (f_limit, (None, "备注")),
    "ni_ttm": (f_ni_ttm, (None, "归属于母公司所有者的净利润TTM(万)")),
    "industry": (f_industry, (None, "行业分类")),
    "loss2y": (f_loss2y, (None, "预期连续两年亏损")),
    "g9_fin": (f_g9_fin, (None, "国九条财务退市预警")),
    "trade_warn": (f_trade_warn, (None, "交易类退市预警")),
    "pool": (f_pool, (None, "总排名分")),
}


def _prewarm(csv):
    """main 一次性建好通用且向量化快的面板; fork worker 经 COW 继承, 不再各自重建.
    慢的纯 python per-instrument 状态机 (_roe_roa_panels/_roe_roa_ours) 留给 worker
    并行跑 (roe/roa/中性ROE/中性ROA 4 个 worker 并行, 不在此串行阻塞)."""
    _csv_groups(csv)  # _GRP_CACHE (trivial)
    _full_frames(csv)  # _FRAMES_CACHE (向量化, 所有中性因子共用, 共享避免 7× 重建)


def run_one(name):
    """worker 进程入口: 跑 Python 参考, 再跑 C++ 张量, 返 list[report]."""
    csv, tdays = load_ctx()
    py_worker, cpp = FACTORS[name]
    out = []
    try:
        r = py_worker(csv, tdays)
        out.extend(r if isinstance(r, list) else [r])
    except Exception as e:
        import traceback

        out.append(
            {
                "factor": name,
                "rate": 0.0,
                "loose": 0.0,
                "lines": [f"PY EXCEPTION: {e}", traceback.format_exc()[-1500:]],
            }
        )

    if cpp[0] is not None:  # cpp=(feat,col) 恒为元组; feat=None 表 Python-only, 无 C++ 张量
        feat, col = cpp
        try:
            d_of, a_of = _cpp_load_axes()
            r = _cpp_run_one(feat, col, csv, d_of, a_of)
            r["factor"] = name + "/cpp"
            out.append(r)
        except Exception as e:
            out.append(
                {
                    "factor": name + "/cpp",
                    "rate": 0.0,
                    "loose": 0.0,
                    "lines": [f"CPP EXCEPTION: {e}"],
                }
            )
    return out


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
    # 派生面板在 main 一次性建好; ProcessPool 用 fork → worker 经 COW 只读继承,
    # 不再各自重建 (否则 N worker × 全表内存 爆 swap, CPU 卡单核).
    csv, _tdays = load_ctx()
    _prewarm(csv)
    results = []
    with ProcessPoolExecutor(args.j, mp_context=mp.get_context("fork")) as ex:
        for rs in ex.map(run_one, args.factors):
            results.extend(rs)
            for r in rs:
                rate = r["rate"]
                rs_ = f"{rate:.4%}" if np.isfinite(rate) else "   —"
                lo_ = f"{r['loose']:.4%}" if np.isfinite(
                    r["loose"]) else "   —"
                print(f"\n=== {r['factor']}  strict={rs_}  loose={lo_}")
                for ln in r["lines"]:
                    print("  " + ln)
                sys.stdout.flush()
    print("\n" + "=" * 60)
    for r in sorted(results, key=lambda x: (-x["rate"] if np.isfinite(x["rate"]) else 1.0)):
        rate = r["rate"]
        rs_ = f"{rate:>9.4%}" if np.isfinite(rate) else "       —"
        lo_ = f"{r['loose']:>9.4%}" if np.isfinite(r["loose"]) else "       —"
        print(f"{r['factor']:<24} strict={rs_}  loose={lo_}")


if __name__ == "__main__":
    main()
