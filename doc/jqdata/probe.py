"""
jqdata 接口能力实测 (走本地 jqbridge: py/jqbridge.py 子进程 + length-prefix 帧协议).

目的: 验证 README_jqdata.md 中 ITFS 表每个接口的真实行为:
  1. 是否可调 (jqbridge status=0 + 非空 body)
  2. 返回行数 / 列名是否与 README 字段表描述一致
  3. 单次 run_query 默认行数上限 (jqdatasdk 内部 RESULT_ROWS_LIMIT=5000)

凭据从 cpp/include/config_main.hpp 解析 (避免硬编码), 与 cpp::jqdata::Bridge 一致.
"""

import csv
import io
import json
import os
import re
import subprocess
import sys
import datetime as dt
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]   # qmt/
CONFIG_HPP = REPO / "cpp" / "include" / "config_main.hpp"
BRIDGE_PY = REPO / "py" / "jqbridge.py"


def parse_credentials():
    """从 cpp/include/config_main.hpp 解析 JQDATA_MOB / JQDATA_PWD 字符串字面量."""
    src = CONFIG_HPP.read_text(encoding="utf-8")
    def grab(name):
        m = re.search(rf'{name}\s*=\s*"([^"]*)"', src)
        assert m, f"{name} not found in config_main.hpp"
        return m.group(1)
    mob = grab("JQDATA_MOB")
    pwd = grab("JQDATA_PWD")
    assert mob and pwd, "JQDATA_MOB / JQDATA_PWD must be non-empty in config.hpp"
    return mob, pwd


# ----------------------------------------------------------------------------
# bridge client: 启子进程 + 帧协议封装. 与 cpp::jqdata::Bridge 等价 (调试用)
# ----------------------------------------------------------------------------

class Bridge:
    def __init__(self):
        self.p = subprocess.Popen(
            [sys.executable, str(BRIDGE_PY)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr,
            bufsize=0,
        )
        mob, pwd = parse_credentials()
        resp = self.call("auth", mob=mob, pwd=pwd)
        assert resp == "auth ok", f"jqbridge auth failed: {resp!r}"

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self.p.stdin and not self.p.stdin.closed:
            self.p.stdin.close()
        self.p.wait(timeout=10)

    def _read_exact(self, n):
        """循环读 n 字节; bufsize=0 时 p.stdout 是 FileIO 可 short-read, 必须自管."""
        buf = b""
        while len(buf) < n:
            chunk = self.p.stdout.read(n - len(buf))
            assert chunk, f"jqbridge EOF: got {len(buf)}/{n}"
            buf += chunk
        return buf

    def _read_line(self):
        """逐字节读到 '\\n' (不含)."""
        line = b""
        while True:
            ch = self.p.stdout.read(1)
            assert ch, "jqbridge EOF before \\n"
            if ch == b"\n":
                return line
            line += ch

    def call(self, method, **params):
        """发一帧请求, 阻塞读响应; status=1 → raise."""
        body = json.dumps({"method": method, "params": params}).encode("utf-8")
        self.p.stdin.write(f"{len(body)}\n".encode("ascii"))
        self.p.stdin.write(body)
        self.p.stdin.write(b"\n")
        self.p.stdin.flush()

        head = self._read_line()
        status_str, n_str = head.decode().split("\t")
        status = int(status_str)
        n = int(n_str)
        resp = self._read_exact(n)
        tail = self._read_exact(1)
        assert tail == b"\n", f"jqbridge missing body tail \\n (method={method})"
        resp = resp.decode("utf-8")
        assert status == 0, f"jqbridge method={method} FAILED:\n{resp}"
        return resp


# ----------------------------------------------------------------------------
# helpers
# ----------------------------------------------------------------------------

def parse_csv(text):
    if not text.strip():
        return [], []
    reader = csv.reader(io.StringIO(text))
    cols = next(reader, [])
    rows = list(reader)
    return cols, rows


RESULTS = []


def _report(label, ok, *, cols=None, n_rows=None, note=None, sample=None, body=None):
    RESULTS.append({"label": label, "ok": ok, "cols": cols, "n_rows": n_rows,
                    "note": note, "sample": sample, "body": body})
    status = "OK " if ok else "FAIL"
    head = f"[{status}] {label}"
    if n_rows is not None:
        head += f"  rows={n_rows}"
    if cols is not None:
        head += f"  cols={len(cols)}"
    print(head)
    if note:
        print(f"       note: {note}")
    if cols is not None:
        print(f"       columns: {cols[:30]}{' ...' if len(cols) > 30 else ''}")
    if sample is not None:
        print(f"       sample[0]: {sample}")
    if body is not None and not ok:
        print(f"       body: {body[:300]}")
    print()


def probe_csv(bridge, label, method, **params):
    try:
        text = bridge.call(method, **params)
    except AssertionError as e:
        _report(label, False, body=str(e))
        return None, None
    cols, rows = parse_csv(text)
    sample = rows[0] if rows else None
    _report(label, True, cols=cols, n_rows=len(rows), sample=sample)
    return cols, rows


def probe_text(bridge, label, method, **params):
    try:
        text = bridge.call(method, **params)
    except AssertionError as e:
        _report(label, False, body=str(e))
        return None
    snippet = text[:200].replace("\n", " | ")
    _report(label, True, note=f"body[:200]: {snippet}")
    return text


# 账号数据窗口锚: 试用/订阅有效期内的工作日. 当前账号窗口 = 2025-02-01 ~ 2026-02-08.
# 跨窗口测试会被服务端拒 (Exception "您的账号权限仅能获取..."), 因此 probe 锚到 2026-01-15.
D_ANCHOR = "2026-01-15"   # probe 主锚日 (周四, 在账号窗口内)
D_ANCHOR_BACK60 = "2025-11-17"  # D_ANCHOR 前约 60 个自然日 (周一), 用于事件类窗口起点


def trade_day_at(offset_back=10):
    """挑一个相对锚日 D_ANCHOR 往前 offset_back 天的工作日 (避开周末)."""
    anchor = dt.date.fromisoformat(D_ANCHOR)
    d = anchor - dt.timedelta(days=offset_back)
    while d.weekday() >= 5:
        d -= dt.timedelta(days=1)
    return d.isoformat()


# ----------------------------------------------------------------------------
# probes (按 README_jqdata.md ITFS 表逐项)
# ----------------------------------------------------------------------------

def main():
    print("=" * 78)
    print("jqdata probe (via jqbridge subprocess)")
    print("=" * 78)
    print()

    with Bridge() as bridge:
        # ---- 0. quota
        probe_text(bridge, "get_query_count [start]", "get_query_count")

        # ---- 1. axis: calendar
        probe_csv(bridge, "get_all_trade_days [calendar]", "get_all_trade_days")

        # ---- 2. asset: securities
        probe_csv(bridge, "get_all_securities (stock) [_meta/securities]",
                  "get_all_securities", types="stock")

        # ---- 3. asset: industry sw_l1 (传 date 避开 today 越权)
        cols, rows = probe_csv(bridge, f"get_industries (sw_l1, date={D_ANCHOR}) [行业列表]",
                               "get_industries", name="sw_l1", date=D_ANCHOR)
        industry_code = rows[0][0] if rows else "801010"

        # 单行业月末快照 (industry_history 拼装单元)
        probe_csv(bridge, f"get_industry_stocks ({industry_code}, {D_ANCHOR})",
                  "get_industry_stocks", industry_code=industry_code,
                  date=D_ANCHOR)

        # ---- 4. asset: namechange (run_query finance.STK_NAME_HISTORY)
        probe_csv(bridge, "run_query(STK_NAME_HISTORY, limit=10) [_meta/namechange]",
                  "run_query", table="STK_NAME_HISTORY", limit="10")

        # ---- 5. 网格: get_price (daily, 单股, 近 30 天)
        D = D_ANCHOR
        Dminus30 = trade_day_at(30)
        print(f"--- probing per-day full-market itf with date={D} ---\n")
        probe_csv(bridge, f"get_price(000001.XSHE, {Dminus30}~{D}) [网格 daily]",
                  "get_price", security="000001.XSHE",
                  start_date=Dminus30, end_date=D,
                  fields="open,close,high,low,volume,money,paused,factor,high_limit,low_limit")

        # ---- 6. 网格: get_fundamentals valuation (单 D 全市场)
        probe_csv(bridge, f"get_fundamentals(valuation, date={D}) [网格 valuation]",
                  "get_fundamentals", table="valuation", date=D)

        # ---- 7. 网格: 融资融券名单
        probe_csv(bridge, f"get_margincash_stocks (date={D}) [网格 margincash]",
                  "get_margincash_stocks", date=D)
        probe_csv(bridge, f"get_marginsec_stocks (date={D}) [网格 marginsec]",
                  "get_marginsec_stocks", date=D)

        # ---- 8. 网格: get_mtss (单股, 近 30 天)
        probe_csv(bridge, f"get_mtss(000001.XSHE, {Dminus30}~{D}) [网格 mtss]",
                  "get_mtss", security="000001.XSHE",
                  start_date=Dminus30, end_date=D)

        # ---- 9. 事件: run_query finance.* (相对锚日往前 60 天的窗口起点)
        print("--- event itf via run_query (finance.*) ---\n")
        D60 = D_ANCHOR_BACK60
        probe_csv(bridge, "run_query(STK_STATUS_CHANGE, pub_date>=D60, limit=10) [status_change]",
                  "run_query", table="STK_STATUS_CHANGE",
                  where=f"pub_date >= '{D60}'", limit="10")
        probe_csv(bridge, "run_query(STK_FIN_FORCAST, pub_date>=D60, limit=10) [forecast]",
                  "run_query", table="STK_FIN_FORCAST",
                  where=f"pub_date >= '{D60}'", limit="10")
        probe_csv(bridge, "run_query(STK_PERFORMANCE_LETTERS, pub_date>=D60, limit=10) [express]",
                  "run_query", table="STK_PERFORMANCE_LETTERS",
                  where=f"pub_date >= '{D60}'", limit="10")
        probe_csv(bridge, "run_query(STK_REPORT_DISCLOSURE, pub_date>=D60, limit=10) [disclosure]",
                  "run_query", table="STK_REPORT_DISCLOSURE",
                  where=f"pub_date >= '{D60}'", limit="10")
        probe_csv(bridge, "run_query(STK_INCOME_STATEMENT, pub_date>=D60 & rt=0, limit=10) [income]",
                  "run_query", table="STK_INCOME_STATEMENT",
                  where=f"pub_date >= '{D60}' AND report_type = 0", limit="10")
        probe_csv(bridge, "run_query(STK_CASHFLOW_STATEMENT, pub_date>=D60 & rt=0, limit=10) [cash_flow]",
                  "run_query", table="STK_CASHFLOW_STATEMENT",
                  where=f"pub_date >= '{D60}' AND report_type = 0", limit="10")

        # ---- 10. 事件 indicator: get_fundamentals indicator + statDate (单季)
        # statDate 必须在账号窗口内 (如 2025q3 ≈ 2025-09-30 在 [2025-02-01, 2026-02-08] 内)
        probe_csv(bridge, f"get_fundamentals(indicator, statDate=2025q3) [indicator 单季]",
                  "get_fundamentals", table="indicator", statDate="2025q3")

        # ---- 11. 事件: dividend (run_query STK_XR_XD)
        probe_csv(bridge, "run_query(STK_XR_XD, a_registration_date>=D60, limit=10) [dividend]",
                  "run_query", table="STK_XR_XD",
                  where=f"a_registration_date >= '{D60}'", limit="10")

        # ---- 12. quota 末态
        probe_text(bridge, "get_query_count [end]", "get_query_count")

    # ---- 汇总
    print("=" * 78)
    print("SUMMARY")
    print("=" * 78)
    ok_cnt = sum(1 for r in RESULTS if r["ok"])
    fail_cnt = len(RESULTS) - ok_cnt
    print(f"total: {len(RESULTS)}  ok: {ok_cnt}  fail: {fail_cnt}")
    print()
    if fail_cnt:
        print("FAILED probes:")
        for r in RESULTS:
            if not r["ok"]:
                print(f"  - {r['label']}")
                print(f"      body: {(r['body'] or '')[:200]}")


if __name__ == "__main__":
    main()
