"""共用工具: tushare data/ 落地加载 + bigquant DAI 查询封装 + 差异比对.

依赖: pandas. (parquet 需 pyarrow, csv 中转最稳, 不依赖 pyarrow.)
约定:
  - tushare 日期统一字符串 'YYYYMMDD' (data 落地原生格式).
  - bigquant 日期 pandas Timestamp; 比对前统一转 'YYYYMMDD' 字符串.
  - 所有 assertion 越早 fail 越好.
"""

import json
import os
import pathlib
import subprocess
import sys
import tempfile
import time

import pandas as pd
from tqdm import tqdm

ROOT = pathlib.Path(__file__).resolve().parents[2]
DATA = ROOT / "data"
META = DATA / "_meta"

DATE_FROM = "2024-01-01"
DATE_TO = "2025-12-31"


# ============================================================
# tushare 落地加载
# ============================================================

def load_meta(name):
    """读 data/_meta/<name>.json. name 含扩展 (e.g. 'stock_basic.json')."""
    p = META / name
    assert p.exists(), f"meta 不存在: {p}"
    return json.loads(p.read_text())


def _iter_day_dirs(date_from=DATE_FROM, date_to=DATE_TO):
    """遍历 data/YYYY/MM/DD 目录, 返回 (date_str='YYYYMMDD', dir_path); 按日期升序."""
    df = date_from.replace("-", "")
    dt = date_to.replace("-", "")
    out = []
    for y in sorted(os.listdir(DATA)):
        if not y.isdigit() or len(y) != 4:
            continue
        ydir = DATA / y
        for m in sorted(os.listdir(ydir)):
            if not m.isdigit() or len(m) != 2:
                continue
            mdir = ydir / m
            for d in sorted(os.listdir(mdir)):
                if not d.isdigit() or len(d) != 2:
                    continue
                date_str = f"{y}{m}{d}"
                if df <= date_str <= dt:
                    out.append((date_str, mdir / d))
    return out


def _iter_month_dirs(date_from=DATE_FROM, date_to=DATE_TO):
    """遍历 data/YYYY/MM, 返回 (year, month, mdir)."""
    df = date_from.replace("-", "")[:6]
    dt = date_to.replace("-", "")[:6]
    out = []
    for y in sorted(os.listdir(DATA)):
        if not y.isdigit() or len(y) != 4:
            continue
        ydir = DATA / y
        for m in sorted(os.listdir(ydir)):
            if not m.isdigit() or len(m) != 2:
                continue
            ym = f"{y}{m}"
            if df <= ym <= dt:
                out.append((y, m, ydir / m))
    return out


def load_itf(itf, date_from=DATE_FROM, date_to=DATE_TO):
    """加载某 itf 全期落地为 DataFrame.

    返回 columns: 原 itf 字段 + visible_date='YYYYMMDD' (文件所在日).
    跳过文件不存在的日子 (后续 status() 会统计三态).
    """
    days = _iter_day_dirs(date_from, date_to)
    frames = []
    n_rows = 0
    bar = tqdm(days, desc=f"tushare {itf}", file=sys.stderr, unit="day", dynamic_ncols=True)
    for date_str, ddir in bar:
        f = ddir / f"{itf}.json"
        if not f.exists():
            continue
        recs = json.loads(f.read_text())
        if not recs:
            continue
        df = pd.DataFrame(recs)
        df["visible_date"] = date_str
        frames.append(df)
        n_rows += len(df)
        bar.set_postfix(files=len(frames), rows=n_rows)
    bar.close()
    assert frames, f"itf={itf} 全期 [{date_from},{date_to}] 无任何文件"
    out = pd.concat(frames, ignore_index=True)
    print(f"  load_itf({itf}): {len(out)} rows, {out['visible_date'].nunique()} days",
          file=sys.stderr)
    return out


def itf_status(itf, date_from=DATE_FROM, date_to=DATE_TO):
    """三态统计 (按月扫 _empty.json + 实文件):

    返回 (has_data, pulled_empty) 两个 set[str 'YYYYMMDD'].
    - has_data: data/YYYY/MM/DD/<itf>.json 存在
    - pulled_empty: 月 _empty.json[itf] 含 DD
    其余日子 (在 date_from..date_to 内但都不在) = 未拉 (调用方自己算).
    """
    has_data = set()
    pulled_empty = set()
    for y, m, mdir in _iter_month_dirs(date_from, date_to):
        empty_f = mdir / "_empty.json"
        if empty_f.exists():
            ej = json.loads(empty_f.read_text())
            for dd in ej.get(itf, []):
                pulled_empty.add(f"{y}{m}{dd}")
        for d in sorted(os.listdir(mdir)):
            if not d.isdigit() or len(d) != 2:
                continue
            f = mdir / d / f"{itf}.json"
            if f.exists():
                has_data.add(f"{y}{m}{d}")
    return has_data, pulled_empty


# ============================================================
# bigquant DAI 查询封装 (csv 中转)
# ============================================================

def bq_query(sql, filters=None, full_scan=False, tag=""):
    """执行 bq dai query, csv 输出到 tempfile, 返回 DataFrame.

    filters: dict, 透传 --filters JSON (常用 {"date":[F,T]}).
    full_scan: 无 filters 时需要 --full-db-scan.
    tag: 进度行 label, 默认取 SQL 前 60 字.

    进度: Popen + 每秒打印已耗时 + csv 文件大小 (bq 自身可能 buffer 后一次性写,
          那种情况下文件大小为 0 直到完成, 至少能看耗时).
    """
    fd, path = tempfile.mkstemp(suffix=".csv", prefix="bq_verify_")
    os.close(fd)
    cmd = ["bq", "dai", "query", sql, "--limit", "0", "-o", path]
    if filters:
        cmd += ["--filters", json.dumps(filters)]
    if full_scan:
        cmd += ["--full-db-scan"]

    label = tag or (sql.replace("\n", " ").strip()[:60])
    flt_s = json.dumps(filters) if filters else "-"
    print(f"  bq query: {label}  filters={flt_s}", file=sys.stderr)
    t0 = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        while True:
            try:
                rc = proc.wait(timeout=1.0)
                break
            except subprocess.TimeoutExpired:
                sz = os.path.getsize(path) if os.path.exists(path) else 0
                el = time.time() - t0
                sys.stderr.write(f"\r    waiting {el:6.1f}s  csv={sz/1e6:7.2f} MB")
                sys.stderr.flush()
        sys.stderr.write("\n")
        sys.stderr.flush()
        stdout, stderr = proc.communicate()
        elapsed = time.time() - t0
        assert rc == 0, (
            f"bq rc={rc} elapsed={elapsed:.1f}s\nstderr: {stderr.strip()[:1500]}\n"
            f"stdout: {stdout.strip()[:1000]}\nSQL: {sql}\nfilters: {flt_s}"
        )
        assert os.path.exists(path) and os.path.getsize(path) > 0, f"bq 输出空: {path}"
        sz_mb = os.path.getsize(path) / 1e6
        print(f"    bq done {elapsed:.1f}s  csv={sz_mb:.2f} MB  reading...", file=sys.stderr)
        df = pd.read_csv(path)
        print(f"    parsed {len(df)} rows  cols={list(df.columns)}", file=sys.stderr)
        return df
    finally:
        if proc.poll() is None:
            proc.kill()
        if os.path.exists(path):
            os.remove(path)


def bq_query_yearly(sql, date_from=DATE_FROM, date_to=DATE_TO, tag=""):
    """按年分段拉 bq (单次 >200MB 上限的破解): 全期切 N 年, 各拉一次, 合并.

    sql: 不要带 WHERE date 限制 (filters 自动加).
    tag: 总 label, 每段自动加 year={Y} 后缀.
    """
    y0 = int(date_from.split("-")[0])
    y1 = int(date_to.split("-")[0])
    label = tag or (sql.replace("\n", " ").strip()[:50])
    print(f"  bq query yearly: {label}  years=[{y0}, {y1}]", file=sys.stderr)
    frames = []
    for y in range(y0, y1 + 1):
        df_y = bq_query(
            sql,
            filters={"date": [f"{y}-01-01", f"{y}-12-31"]},
            tag=f"{label} y={y}",
        )
        frames.append(df_y)
    out = pd.concat(frames, ignore_index=True)
    print(f"  bq query yearly done: total {len(out)} rows", file=sys.stderr)
    return out


def bq_dates_to_str(s):
    """bigquant date 列 (TIMESTAMP_NS pandas 读为 datetime64) → 'YYYYMMDD' 字符串 Series."""
    return pd.to_datetime(s).dt.strftime("%Y%m%d")


# ============================================================
# 比对工具
# ============================================================

def diff_sets(name, ts_set, bq_set, sample=10):
    """集合一致性: ∩/∪/各仅一侧. 打印 + 返回 (only_ts, only_bq) sorted list."""
    only_ts = sorted(ts_set - bq_set)
    only_bq = sorted(bq_set - ts_set)
    inter = ts_set & bq_set
    print(f"  [{name}] ts={len(ts_set)}  bq={len(bq_set)}  ∩={len(inter)}  "
          f"仅ts={len(only_ts)}  仅bq={len(only_bq)}")
    if only_ts:
        print(f"    仅ts 前 {sample}: {only_ts[:sample]}")
    if only_bq:
        print(f"    仅bq 前 {sample}: {only_bq[:sample]}")
    return only_ts, only_bq


def diff_values(name, df, lhs, rhs, eps=1e-6, scale=1.0, sample=10, chunk=200_000):
    """DataFrame 内两列浮点比对, chunked + tqdm 动态显示 mismatch.

    df: merge 后的 DataFrame, 含 lhs/rhs 两列 + 一些 key 列用于打印.
    lhs/rhs: 列名. 比较 |lhs*scale - rhs| < eps * max(1, |rhs|).
    返回不一致行 DataFrame (未截断).
    """
    n = len(df)
    n_bad_total = 0
    bad_idx_list = []
    bar = tqdm(total=n, desc=f"diff {name}", file=sys.stderr, unit="row",
               dynamic_ncols=True)
    for start in range(0, n, chunk):
        sub = df.iloc[start:start + chunk]
        a = pd.to_numeric(sub[lhs], errors="coerce") * scale
        b = pd.to_numeric(sub[rhs], errors="coerce")
        both_nan = a.isna() & b.isna()
        d = (a - b).abs()
        tol = eps * b.abs().clip(lower=1.0)
        bad = (~both_nan) & (a.isna() | b.isna() | (d > tol))
        n_bad = int(bad.sum())
        n_bad_total += n_bad
        if n_bad:
            bad_idx_list.append(sub.index[bad])
        bar.update(len(sub))
        bar.set_postfix(mismatch=n_bad_total)
    bar.close()
    print(f"  [{name}] 比对 {n}  一致 {n - n_bad_total}  不一致 {n_bad_total}")
    if n_bad_total:
        import numpy as np
        bad_idx = np.concatenate([x.values for x in bad_idx_list])
        cols = [c for c in df.columns if c not in (lhs, rhs)] + [lhs, rhs]
        head = df.loc[bad_idx[:sample], cols]
        print(head.to_string(index=False))
        return df.loc[bad_idx]
    return df.iloc[0:0]


# ============================================================
# main 框架: 子脚本 main() 末尾调用
# ============================================================

def finish(passed):
    """统一收尾: assert 越早 fail."""
    print("=" * 60)
    if passed:
        print("PASS")
    else:
        print("FAIL")
    assert passed, "verify 失败"


if __name__ == "__main__":
    print("This is a library, run verify_*.py instead.")
    sys.exit(1)
