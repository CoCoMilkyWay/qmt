"""BigQuant DAI 数据下载脚本.

按 visible_date (因果安全的可见日) 主键, 在给定 [START_MONTH, END_MONTH] 月度区间内,
逐月逐表拉取, 主输出 parquet (高压缩), 可选切片为 JSON.

落盘结构 (脚本同目录下 ./data/):
    parquet (主):
        data/parquet/_meta/<table>.parquet                   # 静态/全量快照表
        data/parquet/<yyyy>-<mm>/<table>.parquet             # 每月每表一个 parquet
    json (可选, SPLIT_JSON=True 时同步生成):
        data/json/_meta/<table>.json                         # 静态表
        data/json/<yyyy>/<mm>/<dd>/<table>.json              # 按 visible_date 逐日切片

约定:
    - parquet 用 zstd 高压缩 (level=22), pyarrow 引擎.
    - JSON 用 records 数组, indent=2, 中文不转义;
      日期类列 -> YYYYMMDD 字符串, NaT/NaN -> null.

visible_date 主键的判定:
    - 大部分表的 `date` 列就是当日盘后可见 (T+0 收盘后, 或 PIT 公告日);
    - 财报派生 (PIT/ttm/notes) 的 `date` = 首次公告可见日;
    - 公告类事件 (dividend/allotment/capital/shareholder) 用 publish_date;
    - 名称变更用 start_date (新简称生效日);
    - basic_info / financial_changedate 为静态快照 (无 date 维度), 全量取.

配置见文件顶部 CONFIG 区块, 直接改变量, 不走命令行.
依赖 BigQuant AI Studio 运行时的 `dai` 模块 (与 bigtrader 同环境).
"""

import calendar
import json
import os
from collections import namedtuple

import pandas as pd  # pyright: ignore[reportMissingImports]

import dai  # pyright: ignore[reportMissingImports]

# ============================================================================
# CONFIG  (直接改这里)
# ============================================================================
# 月度区间, 闭区间, 格式 'YYYY-MM' (整月对齐, 不支持精确到日)
START_MONTH = "2024-01"
END_MONTH = "2024-02"

# 模式: "all" | "static" | "range"
MODE = "all"

# 表名子串过滤; None 表示不过滤
KEYWORD = None

# 是否同步把 parquet 数据按 visible_date 切到 yyyy/mm/dd/<table>.json
SPLIT_JSON = False

# parquet 压缩 (pyarrow 引擎)
PARQUET_COMPRESSION = "zstd"
PARQUET_COMPRESSION_LEVEL = 22  # zstd 最高级

# ============================================================================
# 表清单 & visible_date 主键
# ============================================================================
# strategy:
#   "partition" : date 是分区列且 = visible_date, 用 filters={"date":[s,e]} 加速;
#   "where"     : visible_date 不是分区列, 用 SQL WHERE;
#   "static"    : 无 date 维度, 全量拉, 不接受时间区间.

Table = namedtuple("Table", "name visible_date strategy")

TABLES = [
    # ---- 静态快照 (优先下载, 无时间维度) ----
    Table("cn_stock_basic_info", None, "static"),
    Table("cn_stock_financial_changedate", None, "static"),
    # ---- 通用数据 (date = 当日) ----
    Table("trading_days", "date", "partition"),
    Table("holidays", "date", "partition"),
    Table("cn_stock_instruments", "date", "partition"),
    Table("cn_index_instruments", "date", "partition"),
    Table("cn_fund_instruments", "date", "partition"),
    Table("cn_future_instruments", "date", "partition"),
    Table("cn_cbond_instruments", "date", "partition"),
    # ---- 行业 ----
    Table("cn_stock_industry_component", "date", "partition"),
    Table("cn_stock_industry_bar1d", "date", "partition"),
    Table("cn_stock_industry_valuation", "date", "partition"),
    # ---- 股票日度状态/行情 ----
    Table("cn_stock_shares", "date", "partition"),
    Table("cn_stock_status", "date", "partition"),
    Table("cn_stock_suspend", "date", "partition"),
    Table("cn_stock_bar1d", "date", "partition"),
    Table("cn_stock_limit_price", "date", "partition"),
    Table("cn_stock_margin_trading_detail", "date", "partition"),
    Table("cn_stock_margin_trading_market", "date", "partition"),
    Table("cn_stock_dragon_list", "date", "partition"),
    # ---- 事件型 (visible_date = publish_date / start_date) ----
    Table("cn_stock_capital", "publish_date", "where"),
    Table("cn_stock_dividend", "publish_date", "where"),
    Table("cn_stock_allotment", "publish_date", "where"),
    Table("cn_stock_shareholder", "publish_date", "where"),
    Table("cn_stock_name_change", "start_date", "where"),
    # ---- 财务 PIT (date = 首次公告可见日) ----
    Table("cn_stock_financial_income_general_pit", "date", "partition"),
    Table("cn_stock_financial_cashflow_general_pit", "date", "partition"),
    Table("cn_stock_financial_balance_general_pit", "date", "partition"),
    Table("cn_stock_financial_ttm_shift", "date", "partition"),
    Table("cn_stock_financial_notes_shift", "date", "partition"),
]


# ============================================================================
# 月度迭代工具
# ============================================================================
def _month_list(start_month, end_month):
    """['2024-01', '2024-02', ...] 闭区间"""
    y0, m0 = map(int, start_month.split("-"))
    y1, m1 = map(int, end_month.split("-"))
    assert (y0, m0) <= (y1, m1), f"START_MONTH > END_MONTH: {start_month} > {end_month}"
    out = []
    y, m = y0, m0
    while (y, m) <= (y1, m1):
        out.append(f"{y:04d}-{m:02d}")
        m += 1
        if m == 13:
            m = 1
            y += 1
    return out


def _month_bounds(ym):
    """'2024-01' -> ('2024-01-01', '2024-01-31')"""
    y, m = map(int, ym.split("-"))
    last = calendar.monthrange(y, m)[1]
    return f"{y:04d}-{m:02d}-01", f"{y:04d}-{m:02d}-{last:02d}"


# ============================================================================
# 数据拉取
# ============================================================================
def _query(t, start=None, end=None):
    if t.strategy == "static":
        return dai.query(f"SELECT * FROM {t.name}").df()
    if t.strategy == "partition":
        return dai.query(
            f"SELECT * FROM {t.name}", filters={"date": [start, end]}
        ).df()
    if t.strategy == "where":
        col = t.visible_date
        sql = (
            f"SELECT * FROM {t.name} "
            f"WHERE {col} >= '{start}' AND {col} <= '{end}'"
        )
        return dai.query(sql).df()
    assert False, f"unknown strategy: {t.strategy}"


# ============================================================================
# parquet 落盘
# ============================================================================
def _save_parquet(df, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    df.to_parquet(
        out_path,
        engine="pyarrow",
        compression=PARQUET_COMPRESSION,
        compression_level=PARQUET_COMPRESSION_LEVEL,
        index=False,
    )
    return os.path.getsize(out_path)


# ============================================================================
# JSON 切片 (可选)
# ============================================================================
def _normalize_dates(df):
    """所有 datetime 列 -> YYYYMMDD 字符串; NaT -> None."""
    df = df.copy()
    for col in df.columns:
        if pd.api.types.is_datetime64_any_dtype(df[col]):
            ts = df[col]
            df[col] = ts.dt.strftime("%Y%m%d").where(ts.notna(), None)
    return df


def _to_records(df):
    """df -> records list, NaN/NaT/None 一律 None."""
    obj = df.astype(object).where(df.notna(), None)
    return obj.to_dict(orient="records")


def _dump_json(records, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(records, f, ensure_ascii=False, indent=2)


def _save_json_static(t, df, json_root):
    out_path = os.path.join(json_root, "_meta", f"{t.name}.json")
    df_n = _normalize_dates(df)
    _dump_json(_to_records(df_n), out_path)


def _save_json_day_split(t, df, json_root):
    if len(df) == 0:
        return 0
    col = t.visible_date
    key = pd.to_datetime(df[col]).dt.strftime("%Y%m%d")
    assert key.notna().all(), f"{t.name}: {col} 含 NaT, 无法切片"
    df_n = _normalize_dates(df)
    nfile = 0
    for ymd, sub in df_n.groupby(key, sort=True):
        yyyy, mm, dd = ymd[:4], ymd[4:6], ymd[6:8]
        out_path = os.path.join(json_root, yyyy, mm, dd, f"{t.name}.json")
        _dump_json(_to_records(sub), out_path)
        nfile += 1
    return nfile


# ============================================================================
# 主流程
# ============================================================================
def _select():
    assert MODE in ("all", "static", "range"), f"bad MODE: {MODE}"
    sel = TABLES
    if MODE == "static":
        sel = [t for t in sel if t.strategy == "static"]
    elif MODE == "range":
        sel = [t for t in sel if t.strategy != "static"]
    if KEYWORD:
        sel = [t for t in sel if KEYWORD in t.name]
    assert sel, "no table selected"
    return sel


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    pq_root = os.path.join(here, "data", "parquet")
    json_root = os.path.join(here, "data", "json")

    sel = _select()
    static_tables = [t for t in sel if t.strategy == "static"]
    range_tables = [t for t in sel if t.strategy != "static"]
    months = _month_list(START_MONTH, END_MONTH)

    print(
        f"BigQuant 数据下载  months=[{START_MONTH}..{END_MONTH}] ({len(months)} 个月)  "
        f"MODE={MODE}  SPLIT_JSON={SPLIT_JSON}\n"
        f"  static tables: {len(static_tables)}\n"
        f"  range tables : {len(range_tables)}\n"
    )

    # ---- 1. 静态表 (只拉一次) ----
    for i, t in enumerate(static_tables, 1):
        df = _query(t)
        pq_path = os.path.join(pq_root, "_meta", f"{t.name}.parquet")
        size = _save_parquet(df, pq_path)
        msg_json = ""
        if SPLIT_JSON:
            _save_json_static(t, df, json_root)
            msg_json = "  +json"
        print(
            f"[static {i}/{len(static_tables)}] {t.name:<42} "
            f"rows={len(df):>9}  pq={size/1024:>9.1f} KB{msg_json}"
        )

    # ---- 2. 区间表: 月 × 表 ----
    for mi, ym in enumerate(months, 1):
        start, end = _month_bounds(ym)
        print(f"\n== [{mi}/{len(months)}] month {ym}  ({start} .. {end}) ==")
        for ti, t in enumerate(range_tables, 1):
            df = _query(t, start, end)
            pq_path = os.path.join(pq_root, ym, f"{t.name}.parquet")
            size = _save_parquet(df, pq_path)
            msg_json = ""
            if SPLIT_JSON:
                nfile = _save_json_day_split(t, df, json_root)
                msg_json = f"  +json({nfile}d)"
            print(
                f"  [{ti:>2}/{len(range_tables)}] {t.name:<42} "
                f"({t.strategy:<9} key={t.visible_date:<12})  "
                f"rows={len(df):>9}  pq={size/1024:>9.1f} KB{msg_json}"
            )


if __name__ == "__main__":
    main()
