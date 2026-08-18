"""读 output/ 下 cpp 写出的 npy/json, 用 plotly 出一个 HTML 报告.

用法:
    python -m py.report          # 生成 output/report.html 并打开浏览器

布局: 4 个 TAG 板块, 每块用 view-buttons 在子视图间切换.
    TAG 1: 指标卡 (策略 / pool 指数) | 交易统计
    TAG 2: 回测曲线 (净值/回撤/仓位 3 子图, 含买/卖 marker 与因子 top-2N hover,
             legendgroup 按策略统一, 支持多策略对比)
           | 年收益表 | 月收益表 | 交易收益分布
    TAG 3: 今日持仓 | 交易记录
    TAG 4: 排名分析 (分层年收益/累计/累积IC) | 因子相关性 (汇总表/相关阵)

数据全部来自:
    output/meta.json
    output/backtest/*.npy + labels.json   (后者: 当日历史简称)
    output/analysis/*.npy
    output/tensor/close_raw.npy           (今日持仓最近收盘价)
    data/*/cn_stock_industry_component.parquet  (申万 L1 -- L2)
"""
from __future__ import annotations

import html
import json
import webbrowser
from pathlib import Path

import numpy as np
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# ============================================================================
# 数据加载
# ============================================================================

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "output"
BT_DIR = OUT_DIR / "backtest"
AN_DIR = OUT_DIR / "analysis"

TRADING_DAYS = 252

# 各 TAG 外层 .view-container 高度 (px); TAG 内所有子图共用, 切换对齐
# TAG1: header(22) + 4 cell rows(22 each) + margin(16) = 126, +6 余量
_TAG_H = (132, 480, 620, 480)
_TAG1_H, _TAG2_H, _TAG3_H, _TAG4_H = _TAG_H

_MARGIN_TBL = dict(t=8, b=8, l=8, r=8)
_MARGIN_PLT = dict(t=30, b=30, l=50, r=20)

# 表格仅固定字号与紧凑行高, 其他布局交给 Plotly 自动处理
_FONT_HEADER = 12
_FONT_CELL = 12
_HEADER_H = 22
_CELL_H = 22
_STRAT_COLOR = "crimson"
_RANK_UP = "green"
_RANK_DN = "red"
_HOVER_LABEL = dict(
    bgcolor="white",
    bordercolor="#ddd",
    font=dict(size=9, color="#111",
              family="Consolas, 'Courier New', monospace"),
    align="left",
)


def _fit_cell_h(total_h: int, n_rows: int,
                margin: dict = _MARGIN_TBL) -> int:
    """让单元格高度以紧凑值为主, 小屏下不致撑开."""
    assert n_rows > 0, "n_rows must be > 0"
    avail = total_h - margin["t"] - margin["b"] - _HEADER_H
    target = min(_CELL_H, avail // n_rows)
    return max(12, target)


def _fmt_ymd(v) -> str:
    s = str(v).replace("-", "")
    assert len(s) == 8, f"bad yyyymmdd: {v!r}"
    return f"{s[:4]}-{s[4:6]}-{s[6:8]}"


def _code6(code) -> str:
    return str(code).split(".")[0]


def _sw_industry_map() -> dict[str, str]:
    files = sorted(
        (ROOT / "data").glob("*/cn_stock_industry_component.parquet"))
    assert files, "data/*/cn_stock_industry_component.parquet missing"
    df = pd.read_parquet(
        files[-1],
        columns=["instrument", "industry",
                 "industry_level1_name", "industry_level2_name"],
    )
    df = df[df["industry"] == "sw2021"]
    out = {}
    for inst, l1, l2 in zip(df["instrument"], df["industry_level1_name"],
                            df["industry_level2_name"]):
        a = "" if pd.isna(l1) else str(l1)
        b = "" if pd.isna(l2) else str(l2)
        out[str(inst)] = f"{a} -- {b}" if a else "未知"
    return out


def _open_from_fills(bt) -> dict[int, tuple[int, float]]:
    """fills 时序回放: a → (open_d, open_px), 终态 = 当前仍持有."""
    cur: dict[int, tuple[int, float]] = {}
    for k in range(len(bt["fills_a"])):
        a = int(bt["fills_a"][k])
        side = int(bt["fills_side"][k])
        if side == 1:
            cur[a] = (int(bt["fills_d"][k]), float(bt["fills_px"][k]))
        else:
            assert side == -1, f"fills_side={side}"
            assert a in cur, "sell fill without buy"
            del cur[a]
    return cur


def _html_table(header: list, cols: list, *, compact: bool = False) -> str:
    """原生 HTML 表 (固定行高 + 外层 overflow 滚动). 不要用 Plotly Table 撑开高度."""
    n = len(cols[0]) if cols else 0
    for c in cols:
        assert len(c) == n, "html table columns length mismatch"
    th = "".join(f"<th>{html.escape(str(h))}</th>" for h in header)
    trs = []
    for i in range(n):
        tds = "".join(f"<td>{html.escape(str(col[i]))}</td>" for col in cols)
        trs.append(f"<tr>{tds}</tr>")
    cls = "tbl tbl-compact" if compact else "tbl"
    return (
        f'<table class="{cls}"><thead><tr>{th}</tr></thead>'
        f'<tbody>{"".join(trs)}</tbody></table>'
    )


def _tbl_header(values: list, align: str = "left") -> dict:
    return dict(
        values=values, fill_color="lightsteelblue", align=align,
        font=dict(size=_FONT_HEADER, color="#222"), height=_HEADER_H,
    )


def _tbl_cells(values: list, align: str = "left", height: int | None = None,
               fill_color=None) -> dict:
    kw = dict(values=values, align=align, font=dict(size=_FONT_CELL))
    if height is not None:
        kw["height"] = height
    if fill_color is not None:
        kw["fill_color"] = fill_color
    return kw


def _load() -> dict:
    meta = json.loads((OUT_DIR / "meta.json").read_text(encoding="utf-8"))
    dates_all = np.array(meta["dates"])
    codes = np.array(meta["codes"])

    bt_d_idx = np.load(BT_DIR / "dates.npy")
    bt_dates_str = dates_all[bt_d_idx]
    bt_dates = pd.to_datetime(bt_dates_str, format="%Y%m%d")

    labels = json.loads((BT_DIR / "labels.json").read_text(encoding="utf-8"))

    bt = {
        "dates_idx": bt_d_idx,
        "dates": bt_dates,
        "strategy_nav": np.load(BT_DIR / "strategy_nav.npy"),
        "pool_nav": np.load(BT_DIR / "pool_nav.npy"),
        "tradable_nav": np.load(BT_DIR / "tradable_nav.npy"),
        "position_count": np.load(BT_DIR / "position_count.npy"),
        "position_pct": np.load(BT_DIR / "position_pct.npy"),
        "turnover": np.load(BT_DIR / "turnover.npy"),
        "susp_pct": np.load(BT_DIR / "susp_pct.npy"),
        "executable_pct": np.load(BT_DIR / "executable_pct.npy"),
        "holdings_offsets": np.load(BT_DIR / "holdings_offsets.npy"),
        "holdings_codes": np.load(BT_DIR / "holdings_codes.npy"),
        "holdings_weights": np.load(BT_DIR / "holdings_weights.npy"),
        "holdings_names": np.asarray(labels["holdings_names"], dtype=object),
        "watch_offsets": np.load(BT_DIR / "watch_offsets.npy"),
        "watch_codes": np.load(BT_DIR / "watch_codes.npy"),
        "watch_scores": np.load(BT_DIR / "watch_scores.npy"),
        "watch_rank_chg": np.load(BT_DIR / "watch_rank_chg.npy"),
        "watch_names": np.asarray(labels["watch_names"], dtype=object),
        "trades_inst": np.load(BT_DIR / "trades_inst.npy"),
        "trades_open_d": np.load(BT_DIR / "trades_open_d.npy"),
        "trades_close_d": np.load(BT_DIR / "trades_close_d.npy"),
        "trades_open_px": np.load(BT_DIR / "trades_open_px.npy"),
        "trades_close_px": np.load(BT_DIR / "trades_close_px.npy"),
        "trades_open_names": np.asarray(labels["trades_open_names"], dtype=object),
        "trades_close_names": np.asarray(labels["trades_close_names"], dtype=object),
        "fills_d": np.load(BT_DIR / "fills_d.npy"),
        "fills_a": np.load(BT_DIR / "fills_a.npy"),
        "fills_side": np.load(BT_DIR / "fills_side.npy"),
        "fills_px": np.load(BT_DIR / "fills_px.npy"),
        "fills_names": np.asarray(labels["fills_names"], dtype=object),
    }
    # 健全性: labels 长度对齐
    assert len(bt["holdings_names"]) == len(bt["holdings_codes"]), \
        "labels.holdings_names 与 holdings_codes 长度不一致"
    assert len(bt["watch_names"]) == len(bt["watch_codes"]), \
        "labels.watch_names 与 watch_codes 长度不一致"
    assert len(bt["watch_scores"]) == len(bt["watch_codes"]), \
        "watch_scores 与 watch_codes 长度不一致"
    assert len(bt["watch_rank_chg"]) == len(bt["watch_codes"]), \
        "watch_rank_chg 与 watch_codes 长度不一致"
    assert len(bt["watch_offsets"]) == len(bt["holdings_offsets"]), \
        "watch_offsets 与 holdings_offsets 长度不一致"
    assert len(bt["trades_open_names"]) == len(bt["trades_inst"]), \
        "labels.trades_open_names 与 trades_inst 长度不一致"
    assert len(bt["trades_close_names"]) == len(bt["trades_inst"]), \
        "labels.trades_close_names 与 trades_inst 长度不一致"
    assert len(bt["fills_names"]) == len(bt["fills_d"]), \
        "labels.fills_names 与 fills_d 长度不一致"
    assert len(bt["fills_a"]) == len(bt["fills_d"]), \
        "fills_a 与 fills_d 长度不一致"
    assert len(bt["fills_side"]) == len(bt["fills_d"]), \
        "fills_side 与 fills_d 长度不一致"
    assert len(bt["fills_px"]) == len(bt["fills_d"]), \
        "fills_px 与 fills_d 长度不一致"

    an_d_idx = np.load(AN_DIR / "dates.npy")
    assert np.array_equal(an_d_idx, bt_d_idx), "analysis/backtest dates 不一致"
    assert "hold_n" in meta["config"], "meta.json config 缺 hold_n"
    an = {
        "dates": bt_dates,
        "factor_ic": np.load(AN_DIR / "factor_ic.npy"),
        "factor_turnover": np.load(AN_DIR / "factor_turnover.npy"),
        "factor_corr": np.load(AN_DIR / "factor_corr.npy"),
        "score_ic": np.load(AN_DIR / "score_ic.npy"),
        "score_turnover": np.load(AN_DIR / "score_turnover.npy"),
        "pool_ret": np.load(AN_DIR / "pool_ret.npy"),
        "quantile_ret": np.load(AN_DIR / "quantile_ret.npy"),
    }

    return {
        "meta": meta,
        "dates_all": dates_all,
        "codes": codes,
        "bt": bt,
        "an": an,
    }


# ============================================================================
# 指标计算
# ============================================================================

def _annualize_return(nav: np.ndarray) -> float:
    n = len(nav)
    if n < 2 or nav[0] <= 0:
        return float("nan")
    total = nav[-1] / nav[0]
    years = n / TRADING_DAYS
    return float(total ** (1.0 / years) - 1.0)


def _annualize_vol(daily_ret: np.ndarray) -> float:
    if len(daily_ret) < 2:
        return float("nan")
    return float(np.nanstd(daily_ret) * np.sqrt(TRADING_DAYS))


def _sharpe(daily_ret: np.ndarray, rf: float = 0.0) -> float:
    excess = daily_ret - rf / TRADING_DAYS
    s = np.nanstd(excess)
    if s == 0:
        return float("nan")
    return float(np.nanmean(excess) / s * np.sqrt(TRADING_DAYS))


def _max_drawdown(nav: np.ndarray) -> float:
    if len(nav) == 0:
        return float("nan")
    peak = np.maximum.accumulate(nav)
    dd = (nav - peak) / peak
    return float(dd.min())


def _drawdown_curve(nav: np.ndarray) -> np.ndarray:
    peak = np.maximum.accumulate(nav)
    return (nav - peak) / peak


def _info_ratio(strat_ret: np.ndarray, bench_ret: np.ndarray) -> float:
    diff = strat_ret - bench_ret
    s = np.nanstd(diff)
    if s == 0:
        return float("nan")
    return float(np.nanmean(diff) / s * np.sqrt(TRADING_DAYS))


def _tracking_error(strat_ret: np.ndarray, bench_ret: np.ndarray) -> float:
    return float(np.nanstd(strat_ret - bench_ret) * np.sqrt(TRADING_DAYS))


def _beta_alpha(strat_ret: np.ndarray, bench_ret: np.ndarray) -> tuple[float, float]:
    valid = np.isfinite(strat_ret) & np.isfinite(bench_ret)
    if valid.sum() < 5:
        return float("nan"), float("nan")
    x = bench_ret[valid]
    y = strat_ret[valid]
    cov = np.cov(x, y, ddof=0)
    var_x = cov[0, 0]
    if var_x <= 0:
        return float("nan"), float("nan")
    beta = cov[0, 1] / var_x
    alpha_daily = float(np.mean(y) - beta * np.mean(x))
    alpha_ann = alpha_daily * TRADING_DAYS
    return float(beta), alpha_ann


def _longest_no_new_high(nav: np.ndarray) -> int:
    peak = -np.inf
    cur = 0
    longest = 0
    for v in nav:
        if v > peak:
            peak = v
            cur = 0
        else:
            cur += 1
            longest = max(longest, cur)
    return int(longest)


def _stat_indicators(bt, an, meta) -> dict:
    """TAG 1 视图1: 策略指标. 相对指标对 pool指数; tradable 只给绝对指标."""
    nav_s = bt["strategy_nav"]
    nav_p = bt["pool_nav"]
    nav_t = bt["tradable_nav"]
    ret_s = np.diff(nav_s) / nav_s[:-1]
    ret_s = np.concatenate([[0.0], ret_s])
    ret_p = np.diff(nav_p) / nav_p[:-1]
    ret_p = np.concatenate([[0.0], ret_p])
    ret_t = np.diff(nav_t) / nav_t[:-1]
    ret_t = np.concatenate([[0.0], ret_t])

    beta, alpha = _beta_alpha(ret_s, ret_p)

    return {
        "策略": {
            "天数": len(nav_s),
            "年化": _annualize_return(nav_s),
            "波动率": _annualize_vol(ret_s),
            "夏普": _sharpe(ret_s),
            "最大回撤": _max_drawdown(nav_s),
            "信息比率": _info_ratio(ret_s, ret_p),
            "Beta": beta,
            "Alpha": alpha,
            "跟踪误差": _tracking_error(ret_s, ret_p),
            "创新高最长天数": _longest_no_new_high(nav_s),
        },
        "pool指数": {
            "天数": len(nav_p),
            "年化": _annualize_return(nav_p),
            "波动率": _annualize_vol(ret_p),
            "夏普": _sharpe(ret_p),
            "最大回撤": _max_drawdown(nav_p),
        },
        "tradable指数": {
            "天数": len(nav_t),
            "年化": _annualize_return(nav_t),
            "波动率": _annualize_vol(ret_t),
            "夏普": _sharpe(ret_t),
            "最大回撤": _max_drawdown(nav_t),
        },
    }


def _trade_stats(bt, an, meta) -> dict:
    """TAG 1 视图2: 交易统计."""
    n_trades = len(bt["trades_inst"])
    rets = (bt["trades_close_px"] / bt["trades_open_px"] - 1.0
            if n_trades else np.array([]))
    pos_rets = rets[rets > 0] if n_trades else rets
    neg_rets = rets[rets < 0] if n_trades else rets
    win_rate = float((rets > 0).mean()) if n_trades else float("nan")

    nav_s = bt["strategy_nav"]
    n_d = len(nav_s)
    hold_n = int(meta["config"]["hold_n"])
    turn = bt["turnover"]
    mean_turn = float(np.nanmean(turn)) if n_d else float("nan")
    # turnover = 双边: 当日买卖额 / 2 / 组合市值; 满额换 1 个成分股 = 1/HOLD_N
    n_replace = float(np.nansum(turn) * hold_n) if n_d else 0.0
    avg_hold_days = (1.0 / mean_turn) if mean_turn > 0 else float("nan")

    daily_ret_s = np.concatenate([[0.0], np.diff(nav_s) / nav_s[:-1]])

    dates = bt["dates"]
    daily_df = pd.DataFrame({"date": dates, "ret": daily_ret_s})
    daily_df = daily_df.set_index("date")

    weekly = daily_df.resample("W").apply(lambda x: (1 + x).prod() - 1)
    monthly = daily_df.resample("ME").apply(lambda x: (1 + x).prod() - 1)

    return {
        "年换手率": mean_turn * TRADING_DAYS,
        "平均持有天数": avg_hold_days,
        "平均持仓股票数": float(np.mean(bt["position_count"])),
        "平均交易收益": float(np.mean(rets)) if n_trades else float("nan"),
        "正收益平均": float(np.mean(pos_rets)) if len(pos_rets) else float("nan"),
        "负收益平均": float(np.mean(neg_rets)) if len(neg_rets) else float("nan"),
        "交易赢率": win_rate,
        "换股次数": n_replace,
        "持仓停牌股票比例": float(np.nanmean(bt["susp_pct"])),
        "月赢率": float((monthly["ret"] > 0).mean()),
        "周赢率": float((weekly["ret"] > 0).mean()),
        "日赢率": float((daily_df["ret"] > 0).mean()),
        "调仓指令可执行比例": float(np.nanmean(bt["executable_pct"])),
        "指数跟踪误差": _tracking_error(daily_ret_s,
                                  np.concatenate([[0.0],
                                                  np.diff(bt["pool_nav"]) /
                                                  bt["pool_nav"][:-1]])),
        "平均持仓仓位": float(np.nanmean(bt["position_pct"])),
        "创新高最长天数": _longest_no_new_high(nav_s),
        "CPU时长(秒)": (
            float(meta["timing"].get("backtest_seconds", 0.0)) +
            float(meta["timing"].get("analysis_seconds", 0.0))
        ),
        "Tensor内存(GB)": float(meta["timing"].get("tensor_bytes", 0)) / 1024 / 1024 / 1024,
    }


# ============================================================================
# TAG 1: 指标卡 (table) + 交易统计 (table). updatemenu 切换.
# ============================================================================

# 格式化规则：按指标名分类
_PCT_METRICS = {"年化", "波动率", "最大回撤", "Alpha", "跟踪误差",
                "平均交易收益", "正收益平均", "负收益平均", "交易赢率",
                "持仓停牌股票比例", "月赢率", "周赢率", "日赢率",
                "调仓指令可执行比例", "指数跟踪误差", "平均持仓仓位"}
_INT_METRICS = {"天数", "创新高最长天数"}


def _fmt_v(v, key=""):
    if v == "—":
        return "—"
    if isinstance(v, int):
        return str(v)
    if not isinstance(v, float):
        return str(v)
    if not np.isfinite(v):
        return "nan"
    if key in _INT_METRICS:
        return str(int(round(v)))
    if key in _PCT_METRICS:
        return f"{v*100:.2f}%"
    return f"{v:.4f}"


def _transposed_table(col_headers: list,
                      row_labels: list,
                      row_data: list,
                      cell_height: int | None = None,
                      ) -> go.Table:
    """列 = 指标, 行 = 实体."""
    n_rows = len(row_labels)

    cols = [row_labels]
    for metric in col_headers:
        cols.append([_fmt_v(row.get(metric, "—"), metric) for row in row_data])

    ch = _fit_cell_h(
        _TAG1_H, n_rows=n_rows) if cell_height is None else cell_height

    return go.Table(
        header=_tbl_header(["指标"] + col_headers, align="center"),
        cells=_tbl_cells(cols, align="center", height=ch),
    )


def _wide_metric_table(index: pd.Index,
                       rows: pd.DataFrame,
                       *,
                       align: str = "left") -> str:
    """期次行在左, 指标列在上 (竖排 HTML 表)."""
    del align
    metric_names = list(rows.columns)
    if rows.empty:
        return _html_table(["日期"] + metric_names, [[] for _ in range(1 + len(metric_names))])

    def _fmt(v, metric_key: str):
        if not isinstance(v, (int, float, np.integer, np.floating)):
            return str(v)
        if not np.isfinite(v):
            return "nan"
        if metric_key in {"策略收益", "基准收益", "策略最大回撤", "基准最大回撤", "跟踪误差",
                          "信息比率", "波动率", "夏普比率"}:
            return f"{v*100:.2f}%"
        return f"{v:.4f}"

    cols = [list(index.astype(str))]
    for m in metric_names:
        cols.append([_fmt(rows.loc[k, m], m) for k in index])
    return _html_table(["日期"] + metric_names, cols)


def _trade_stats_fig(trades: dict) -> go.Figure:
    """交易统计: 单张 Plotly Table (9 列 × 1 header + 3 cell rows).

    上下两组指标拼成一张表, 中段用表头底色作为第二段视觉表头, 无 subplot 间隙.
    """
    trade_metrics = [
        "换股次数", "平均持有天数", "平均持仓股票数", "平均持仓仓位",
        "调仓指令可执行比例", "持仓停牌股票比例",
        "平均交易收益", "正收益平均", "负收益平均",
        "年换手率", "交易赢率", "日赢率", "周赢率", "月赢率",
        "指数跟踪误差", "创新高最长天数",
        "CPU时长(秒)", "Tensor内存(GB)",
    ]
    n_cols = 9
    assert len(trade_metrics) == 2 * n_cols, "需 2*n_cols 个指标"
    top_metrics = trade_metrics[:n_cols]
    bot_metrics = trade_metrics[n_cols:]

    top_vals = [_fmt_v(trades.get(k, "—"), k) for k in top_metrics]
    bot_vals = [_fmt_v(trades.get(k, "—"), k) for k in bot_metrics]

    cells_values = [
        [top_vals[i], bot_metrics[i], bot_vals[i]]
        for i in range(n_cols)
    ]
    fill_per_col = [["white", "lightsteelblue", "white"]] * n_cols

    fig = go.Figure(go.Table(
        header=_tbl_header(top_metrics, align="center"),
        cells=dict(
            values=cells_values,
            fill_color=fill_per_col,
            align="center",
            height=_CELL_H,
            font=dict(size=_FONT_CELL),
        ),
    ))
    fig.update_layout(height=_TAG1_H, margin=_MARGIN_TBL, autosize=True)
    return fig


def fig_tag1(bt, an, meta) -> list[tuple[str, go.Figure]]:
    indicators = _stat_indicators(bt, an, meta)
    trades = _trade_stats(bt, an, meta)

    # ── view1: 策略指标横排 (策略 / pool指数 / tradable指数 / 超额 vs pool) ──
    strat = indicators["策略"]
    pool = indicators["pool指数"]
    trad = indicators["tradable指数"]

    excess: dict = {}
    for k in ["年化", "波动率", "夏普", "最大回撤"]:
        sv, pv = strat.get(k), pool.get(k)
        excess[k] = (sv - pv) if isinstance(sv,
                                            float) and isinstance(pv, float) else "—"
    for k in ["信息比率", "Alpha", "跟踪误差"]:
        excess[k] = strat.get(k, "—")

    metrics_v1 = ["天数", "年化", "波动率", "夏普", "最大回撤",
                  "信息比率", "Beta", "Alpha", "跟踪误差", "创新高最长天数"]
    fig1 = go.Figure(_transposed_table(
        col_headers=metrics_v1,
        row_labels=["策略", "pool指数", "tradable指数", "超额"],
        row_data=[strat, pool, trad, excess],
    ))
    fig1.update_layout(height=_TAG1_H, margin=_MARGIN_TBL, autosize=True)

    # ── view2: 交易统计 (同风格) ──
    fig2 = _trade_stats_fig(trades)
    return [
        ("策略 / pool指数 指标", fig1),
        ("交易统计", fig2),
    ]


# ============================================================================
# TAG 2: 收益/回撤 / 分布 / 表格. updatemenu 切换.
# ============================================================================

def _resample_table(daily_ret: np.ndarray, daily_bench: np.ndarray,
                    dates: pd.DatetimeIndex, freq: str) -> pd.DataFrame:
    """生成 年/月 收益对比 + 风险表."""
    df = pd.DataFrame({"date": dates,
                       "strat": daily_ret,
                       "bench": daily_bench}).set_index("date")
    grp = df.resample(freq)

    def _agg_one(g):
        if len(g) == 0:
            return pd.Series({"策略收益": np.nan, "基准收益": np.nan,
                              "策略最大回撤": np.nan, "基准最大回撤": np.nan,
                              "跟踪误差": np.nan, "信息比率": np.nan,
                              "波动率": np.nan, "夏普比率": np.nan})
        cum_s = (1 + g["strat"]).cumprod().values
        cum_b = (1 + g["bench"]).cumprod().values
        return pd.Series({
            "策略收益": cum_s[-1] - 1,
            "基准收益": cum_b[-1] - 1,
            "策略最大回撤": _max_drawdown(cum_s),
            "基准最大回撤": _max_drawdown(cum_b),
            "跟踪误差": _tracking_error(g["strat"].values, g["bench"].values),
            "信息比率": _info_ratio(g["strat"].values, g["bench"].values),
            "波动率": _annualize_vol(g["strat"].values),
            "夏普比率": _sharpe(g["strat"].values),
        })

    out = grp.apply(_agg_one)
    out = out.dropna(how="all")
    out.index = out.index.strftime("%Y" if freq == "YE" else "%Y-%m")
    return out


def _build_trade_markers(bt, codes, side: str, nav_norm: np.ndarray):
    """聚合同日多笔为一个 marker. side ∈ {'buy','sell'}.

    返回 (xs, ys, hovers). marker 直接落在 NAV 曲线上.
    - buy:  x=open_d, name 取 trades_open_names (PIT 开仓日)
    - sell: x=close_d, name 取 trades_close_names (PIT 平仓日)
    """
    inst = bt["trades_inst"]
    px_open = bt["trades_open_px"]
    px_close = bt["trades_close_px"]
    if side == "buy":
        ds = bt["trades_open_d"]
        names = bt["trades_open_names"]
    else:
        ds = bt["trades_close_d"]
        names = bt["trades_close_names"]

    dates_idx = bt["dates_idx"]
    if len(dates_idx) == 0:
        return [], [], []
    d0 = int(dates_idx[0])
    n_d_bt = len(dates_idx)

    by_d: dict[int, list[str]] = {}
    for k in range(len(inst)):
        d = int(ds[k])
        i_bt = d - d0
        if i_bt < 0 or i_bt >= n_d_bt:
            continue
        a = int(inst[k])
        op = float(px_open[k])
        cp = float(px_close[k])
        pnl = (cp / op - 1.0) * 100.0 if op > 0 else float("nan")
        if side == "buy":
            line = f"{names[k]} ({codes[a]}) @ {op:.2f}"
        else:
            line = f"{names[k]} ({codes[a]}) @ {cp:.2f} ({pnl:+.2f}%)"
        by_d.setdefault(i_bt, []).append(line)

    xs, ys, hovers = [], [], []
    bt_dates = bt["dates"]
    label = "买入" if side == "buy" else "卖出"
    for i_bt in sorted(by_d):
        xs.append(bt_dates[i_bt])
        ys.append(float(nav_norm[i_bt]))
        lines = by_d[i_bt]
        head = f"<b>{label} ×{len(lines)}</b>"
        hovers.append(head + "<br>" + "<br>".join(lines))
    return xs, ys, hovers


def _rank_arrow(chg: float, n_rank: int) -> str:
    """相对 5 日因子排名均线: |Δ|≤0.1N 横线, >0.1N 单箭头, >0.2N 双箭头.

    始终两个同宽几何符 (单档第二枚涂白占位; 横档用同宽 ▬▬), 字号跟正文走.
    """
    assert n_rank > 0, "n_rank must be > 0"
    t1 = 0.1 * n_rank
    t2 = 0.2 * n_rank
    sty = "font-weight:900"

    def _pair(ch: str, color: str, n_vis: int) -> str:
        vis = f'<span style="color:{color};{sty}">{ch * n_vis}</span>'
        if n_vis == 2:
            return vis
        return vis + f'<span style="color:#fff;{sty}">{ch}</span>'
    if chg > t2:
        return _pair("▲", _RANK_UP, 2)
    if chg > t1:
        return _pair("▲", _RANK_UP, 1)
    if chg < -t2:
        return _pair("▼", _RANK_DN, 2)
    if chg < -t1:
        return _pair("▼", _RANK_DN, 1)
    return _pair("▬", "#111", 2)


def _pad_name(name: str, n_chars: int) -> str:
    s = str(name)
    if len(s) >= n_chars:
        return s
    return s + "\u3000" * (n_chars - len(s))


def _build_strategy_hover(bt, codes, n_rank: int) -> list[str]:
    """每个 bt 交易日的 hover: 因子 top-(HOLD_N*2) + 正式调仓买卖.

    再平衡加仓不进 tooltip / trades_*; 换手统计仍按成交额计入.
    """
    n = len(bt["dates"])
    off = bt["holdings_offsets"]
    hcodes = bt["holdings_codes"]
    hweights = bt["holdings_weights"]
    pos_pct = bt["position_pct"]
    woff = bt["watch_offsets"]
    wcodes = bt["watch_codes"]
    wscores = bt["watch_scores"]
    wchg = bt["watch_rank_chg"]
    wnames = bt["watch_names"]

    streak_start: dict[int, int] = {}
    days_held = [0] * len(hcodes)
    for i in range(n):
        lo, hi = int(off[i]), int(off[i + 1])
        cur: set[int] = set()
        for k in range(lo, hi):
            a = int(hcodes[k])
            cur.add(a)
            if a not in streak_start:
                streak_start[a] = i
            days_held[k] = i - streak_start[a] + 1
        for a in list(streak_start):
            if a not in cur:
                del streak_start[a]

    weight_map: dict[tuple[int, int], float] = {}
    held_info: dict[tuple[int, int], tuple[float, int]] = {}
    for i in range(n):
        lo, hi = int(off[i]), int(off[i + 1])
        for k in range(lo, hi):
            a = int(hcodes[k])
            w = float(hweights[k])
            weight_map[(i, a)] = w
            held_info[(i, a)] = (w, days_held[k])

    inst = bt["trades_inst"]
    open_d = bt["trades_open_d"]
    close_d = bt["trades_close_d"]
    px_open = bt["trades_open_px"]
    px_close = bt["trades_close_px"]
    dates_idx = bt["dates_idx"]
    d0 = int(dates_idx[0]) if len(dates_idx) > 0 else 0

    pnl_at: dict[tuple[int, int], float] = {}
    for k in range(len(inst)):
        a = int(inst[k])
        cb = int(close_d[k]) - d0
        op = float(px_open[k])
        cp = float(px_close[k])
        pnl_at[(cb, a)] = (cp / op - 1.0) * 100.0 if op > 0 else float("nan")

    buys: dict[int, list[str]] = {}
    sells: dict[int, list[str]] = {}
    buy_as: dict[int, set[int]] = {}
    fd, fa = bt["fills_d"], bt["fills_a"]
    fs, fpx, fn = bt["fills_side"], bt["fills_px"], bt["fills_names"]
    for k in range(len(fd)):
        a = int(fa[k])
        i_bt = int(fd[k]) - d0
        if i_bt < 0 or i_bt >= n:
            continue
        px = float(fpx[k])
        if int(fs[k]) > 0:
            w_buy = weight_map.get((i_bt, a), 0.0) * 100.0
            buys.setdefault(i_bt, []).append(
                f"<b>{fn[k]} ({codes[a]}) @ {px:.2f} ({w_buy:.0f}%)</b>")
            buy_as.setdefault(i_bt, set()).add(a)
        else:
            pnl = pnl_at.get((i_bt, a), float("nan"))
            w_pre = weight_map.get((i_bt - 1, a), 0.0) * 100.0
            sells.setdefault(i_bt, []).append(
                f"<b>{fn[k]} ({codes[a]}) @ {px:.2f} "
                f"({pnl:+.0f}%, {w_pre:.0f}%)</b>")

    out = [""] * n
    for i in range(n):
        lo, hi = int(woff[i]), int(woff[i + 1])
        if hi == lo:
            watch_str = "(空)"
        else:
            name_w = max((len(str(wnames[k]))
                         for k in range(lo, hi)), default=4)
            name_w = max(name_w, 4)
            lines = []
            for k in range(lo, hi):
                a = int(wcodes[k])
                score = float(wscores[k])
                arrow = _rank_arrow(float(wchg[k]), n_rank)
                name_col = _pad_name(str(wnames[k]), name_w)
                score_col = f"{score:5.2f}"
                info = held_info.get((i, a))
                if info is not None:
                    w, dh = info
                    extra = f" {int(round(w * 100)):2d}% {dh:3d}d"
                    rest = (f'<span style="color:{_STRAT_COLOR}">'
                            f"<b>{score_col}{extra}</b></span>")
                    if a in buy_as.get(i, ()):
                        name_h = f"<b>{name_col}</b>"
                    else:
                        name_h = (f'<span style="color:{_STRAT_COLOR}">'
                                  f"<b>{name_col}</b></span>")
                    body = name_h + rest
                else:
                    body = f"{name_col}{score_col}"
                lines.append(f"{arrow} {body}")
            watch_str = "<br>".join(lines)
        parts = [f"仓位 {pos_pct[i] * 100:.0f}%<br>{watch_str}"]
        if i in buys:
            parts.append(
                f"<b>买入 ×{len(buys[i])}</b><br>" + "<br>".join(buys[i]))
        if i in sells:
            parts.append(
                f"<b>卖出 ×{len(sells[i])}</b><br>" + "<br>".join(sells[i]))
        out[i] = "<br><br>".join(parts)
    return out


def fig_tag2(bt, an, meta, codes) -> list[tuple[str, go.Figure]]:
    nav_s = bt["strategy_nav"]
    nav_p = bt["pool_nav"]
    nav_t = bt["tradable_nav"]
    dates = bt["dates"]
    daily_s = np.concatenate([[0.0], np.diff(nav_s) / nav_s[:-1]])
    daily_p = np.concatenate([[0.0], np.diff(nav_p) / nav_p[:-1]])
    dd_s = _drawdown_curve(nav_s)
    dd_p = _drawdown_curve(nav_p)
    dd_t = _drawdown_curve(nav_t)

    def _chart(*traces, **kw):
        fig = go.Figure()
        for tr in traces:
            fig.add_trace(tr)
        fig.update_layout(height=_TAG2_H, margin=_MARGIN_PLT,
                          autosize=True, font=dict(size=_FONT_CELL), **kw)
        return fig

    # view 1: 回测曲线 — 净值 / 回撤 / 仓位 3 子图, shared x.
    #   - 净值 hover: 因子 top-(HOLD_N*2) (历史简称 / code / 分数, 持仓高亮)
    #   - 同日多笔买/卖 聚合为 1 个圆点 marker, 直接落在 NAV 曲线上
    #   - legendgroup 按策略名统一: 一个 legend 项联动该策略在 3 个子图的
    #     全部 trace, 后续多策略横向对比同一套 legend 语义
    nav_s_norm = nav_s / nav_s[0]
    nav_p_norm = nav_p / nav_p[0]
    nav_t_norm = nav_t / nav_t[0]
    n_rank = int(meta["config"]["hold_n"]) * 2
    strat_hover = _build_strategy_hover(bt, codes, n_rank)

    fig1 = make_subplots(
        rows=3, cols=1, shared_xaxes=True,
        row_heights=[3, 1, 1], vertical_spacing=0.04,
    )

    fig1.add_trace(go.Scatter(
        x=dates, y=nav_s_norm, mode="lines", name="策略",
        legendgroup="策略", line=dict(color=_STRAT_COLOR),
        customdata=strat_hover,
        hovertemplate="%{customdata}<extra></extra>",
        hoverlabel=_HOVER_LABEL,
    ), row=1, col=1)
    fig1.add_trace(go.Scatter(
        x=dates, y=nav_p_norm, mode="lines", name="pool指数",
        legendgroup="pool指数", line=dict(color="steelblue"),
        hoverinfo="skip",
    ), row=1, col=1)
    fig1.add_trace(go.Scatter(
        x=dates, y=nav_t_norm, mode="lines", name="tradable指数",
        legendgroup="tradable指数", line=dict(color="seagreen"),
        hoverinfo="skip",
    ), row=1, col=1)

    # 正式调仓日 (fills: 因子 pop/补槽, 含仍持有的新买; 不含再平衡).
    d0 = int(bt["dates_idx"][0]) if len(bt["dates_idx"]) else 0
    seen = {}
    for k in range(len(bt["fills_d"])):
        i_bt = int(bt["fills_d"][k]) - d0
        if 0 <= i_bt < len(nav_s_norm):
            seen[bt["dates"][i_bt]] = float(nav_s_norm[i_bt])
    if seen:
        mx, my = zip(*sorted(seen.items()))
        fig1.add_trace(go.Scatter(
            x=list(mx), y=list(my), mode="markers", name="调仓",
            legendgroup="策略", showlegend=False,
            marker=dict(symbol="triangle-down", color="black", size=8),
            hoverinfo="skip",
        ), row=1, col=1)

    fig1.add_trace(go.Scatter(
        x=dates, y=dd_s, mode="lines", name="策略",
        legendgroup="策略", showlegend=False,
        line=dict(color="crimson"), fill="tozeroy",
        hoverinfo="skip",
    ), row=2, col=1)
    fig1.add_trace(go.Scatter(
        x=dates, y=dd_p, mode="lines", name="pool指数",
        legendgroup="pool指数", showlegend=False,
        line=dict(color="steelblue"), fill="tozeroy",
        hoverinfo="skip",
    ), row=2, col=1)
    fig1.add_trace(go.Scatter(
        x=dates, y=dd_t, mode="lines", name="tradable指数",
        legendgroup="tradable指数", showlegend=False,
        line=dict(color="seagreen"), fill="tozeroy",
        hoverinfo="skip",
    ), row=2, col=1)

    fig1.add_trace(go.Scatter(
        x=dates, y=bt["position_pct"], mode="lines", name="策略",
        legendgroup="策略", showlegend=False,
        line=dict(color="crimson"), fill="tozeroy",
        fillcolor="rgba(220,20,60,0.18)",
        hoverinfo="skip",
    ), row=3, col=1)

    # zoom 只支持横向: 三个子图 y 轴全部 fixedrange (纵向由前端 JS 在横向
    # zoom 后对净值图重算 full range, 见 RENORM_JS).
    fig1.update_yaxes(title_text="净值 (归一)", fixedrange=True, row=1, col=1)
    fig1.update_yaxes(title_text="回撤", tickformat=".0%",
                      fixedrange=True, row=2, col=1)
    fig1.update_yaxes(title_text="仓位", tickformat=".0%",
                      range=[0, 1.02], fixedrange=True, row=3, col=1)
    fig1.update_layout(
        height=_TAG2_H, margin=_MARGIN_PLT, autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode="closest", hoverdistance=10,
        hoverlabel=_HOVER_LABEL,
        legend=dict(orientation="h", y=1.06, x=0),
    )

    # view 2: 年收益表 (竖排 + 外层滚动)
    yt = _resample_table(daily_s, daily_p, dates, "YE")
    fig5 = _wide_metric_table(yt.index, yt)

    # view 3: 月收益表 (竖排 + 外层滚动)
    mt = _resample_table(daily_s, daily_p, dates, "ME")
    fig6 = _wide_metric_table(mt.index, mt)

    # view 4: 交易收益分布
    if len(bt["trades_open_px"]) > 0:
        rets = bt["trades_close_px"] / bt["trades_open_px"] - 1.0
        fig7 = _chart(go.Histogram(x=rets, name="交易收益",
                      nbinsx=80, marker_color="teal"))
    else:
        fig7 = _chart(go.Scatter(x=[], y=[], name="无交易"))

    return [
        ("回测曲线", fig1),
        ("年收益表", fig5),
        ("月收益表", fig6),
        ("交易收益分布", fig7),
    ]


# ============================================================================
# TAG 3: 今日持仓 / 交易记录 (table 切换)
# ============================================================================

def fig_tag3(bt, codes, dates_all) -> list[tuple[str, go.Figure]]:
    ind_map = _sw_industry_map()
    opens = _open_from_fills(bt)
    close_raw_p = OUT_DIR / "tensor" / "close_raw.npy"
    assert close_raw_p.exists(), "output/tensor/close_raw.npy missing"
    close_raw = np.load(close_raw_p, mmap_mode="r")

    hold_header = ["序号", "股票", "行业分类", "买入日期", "买入价格",
                   "最近收盘价", "当前仓位", "累计涨幅"]
    trade_header = ["序号", "股票", "行业分类", "买入日期", "卖出日期",
                    "持仓天数", "买入价格", "卖出价格", "涨幅"]

    # view 1: 今日持仓 — 最后一天, 段内权重降序 (cpp 已排好)
    n_d_bt = len(bt["dates"])
    off = bt["holdings_offsets"]
    lo, hi = int(off[n_d_bt - 1]), int(off[n_d_bt])
    d_last = int(bt["dates_idx"][-1])
    h_seq, h_stock, h_ind, h_od, h_opx, h_cpx, h_w, h_pnl = (
        [], [], [], [], [], [], [], [])
    for i, k in enumerate(range(lo, hi), start=1):
        a = int(bt["holdings_codes"][k])
        code = str(codes[a])
        assert a in opens, f"holding {code} has no buy fill"
        od, opx = opens[a]
        cpx = float(close_raw[a, d_last])
        h_seq.append(str(i))
        h_stock.append(f"{bt['holdings_names'][k]} ({_code6(code)})")
        h_ind.append(ind_map.get(code, "未知"))
        h_od.append(_fmt_ymd(dates_all[od]))
        h_opx.append(f"{opx:.2f}")
        h_cpx.append(f"{cpx:.2f}")
        h_w.append(f"{float(bt['holdings_weights'][k])*100:.2f}%")
        h_pnl.append(f"{(cpx / opx - 1) * 100:.2f}%")
    fig1 = _html_table(
        hold_header,
        [h_seq, h_stock, h_ind, h_od, h_opx, h_cpx, h_w, h_pnl],
        compact=True,
    )

    # view 2: 交易记录 (名称取平仓当日历史简称)
    t_seq, t_stock, t_ind, t_od, t_cd, t_days, t_opx, t_cpx, t_pnl = (
        [], [], [], [], [], [], [], [], [])
    if len(bt["trades_inst"]) > 0:
        order = np.argsort(-bt["trades_close_d"])
        for i, k in enumerate(order, start=1):
            a = int(bt["trades_inst"][k])
            code = str(codes[a])
            od = int(bt["trades_open_d"][k])
            cd = int(bt["trades_close_d"][k])
            op = float(bt["trades_open_px"][k])
            cp = float(bt["trades_close_px"][k])
            t_seq.append(str(i))
            t_stock.append(f"{bt['trades_close_names'][k]} ({_code6(code)})")
            t_ind.append(ind_map.get(code, "未知"))
            t_od.append(_fmt_ymd(dates_all[od]))
            t_cd.append(_fmt_ymd(dates_all[cd]))
            t_days.append(str(cd - od))
            t_opx.append(f"{op:.2f}")
            t_cpx.append(f"{cp:.2f}")
            t_pnl.append(f"{(cp / op - 1) * 100:.2f}%")
    fig2 = _html_table(
        trade_header,
        [t_seq, t_stock, t_ind, t_od, t_cd, t_days, t_opx, t_cpx, t_pnl],
        compact=True,
    )

    return [
        ("今日持仓", fig1),
        ("交易记录", fig2),
    ]


# ============================================================================
# TAG 4: 排名分析 / 因子相关性
# ============================================================================

def _rolling_mean(x: np.ndarray, w: int) -> np.ndarray:
    s = pd.Series(x, dtype=float)
    return s.rolling(window=w, min_periods=max(1, w // 4)).mean().values


def _quantile_nav(ret: np.ndarray) -> np.ndarray:
    # NaN 视作 0 收益 (与 backtest pool_nav 在 dr_n=0 时 dr=0 同口径).
    # 不 nan_to_num 的话, numpy.cumprod 遇 NaN 后续全 NaN → 图断在首个空桶日.
    return np.cumprod(1.0 + np.nan_to_num(ret, nan=0.0))


def _cagr_from_nav(nav: np.ndarray) -> float:
    n = len(nav)
    if n < 1 or nav[-1] <= 0:
        return float("nan")
    return float(nav[-1] ** (TRADING_DAYS / n) - 1.0)


def _cumsum_ic(x: np.ndarray) -> np.ndarray:
    return np.nancumsum(np.nan_to_num(x.astype(float), nan=0.0))


def _end_label(x, y, name: str, color: str) -> go.Scatter:
    return go.Scatter(
        x=[x[-1]], y=[float(y[-1])], mode="text",
        text=[name], textposition="middle right",
        textfont=dict(size=9, color=color),
        legendgroup=name, showlegend=False, hoverinfo="skip",
        cliponaxis=False,
    )


def fig_tag4(an, meta) -> list[tuple[str, go.Figure]]:
    factor_names = meta["factor_names"]
    n_factor = len(factor_names)
    dates = an["dates"]
    qret = an["quantile_ret"]
    Q = qret.shape[0]
    pool_ret = an["pool_ret"]

    # view 1: 分层年化 — 与累计图同一条净值, 全程 CAGR (不是各年年化再算术平均)
    q_navs = [_quantile_nav(qret[q]) for q in range(Q)]
    pool_nav_q = _quantile_nav(pool_ret)
    labels_q = [f"Q{q+1}" for q in range(Q)] + ["pool"]
    cagr_q = [_cagr_from_nav(v) for v in q_navs] + [_cagr_from_nav(pool_nav_q)]
    fig1 = go.Figure(go.Bar(x=labels_q, y=cagr_q,
                            marker_color=["steelblue"] * Q + ["darkred"],
                            name="年化"))
    fig1.update_layout(height=_TAG4_H, margin=_MARGIN_PLT,
                       autosize=True, font=dict(size=_FONT_CELL),
                       yaxis=dict(tickformat=".0%"))

    # view 2: 分层累计收益. 横向 zoom + 15 日均线锚, 见 RENORM_JS.
    colors = px_colors(Q + 1)
    fig2 = go.Figure()
    for q in range(Q):
        nm = f"Q{q+1}"
        fig2.add_trace(go.Scatter(
            x=dates, y=q_navs[q], mode="lines",
            name=nm, legendgroup=nm, showlegend=False,
            hoverinfo="skip",
            line=dict(color=colors[q])))
        fig2.add_trace(_end_label(dates, q_navs[q], nm, colors[q]))
    fig2.add_trace(go.Scatter(
        x=dates, y=pool_nav_q, mode="lines",
        name="pool", legendgroup="pool", showlegend=False,
        hoverinfo="skip",
        line=dict(color="black", dash="dash")))
    fig2.add_trace(_end_label(dates, pool_nav_q, "pool", "black"))
    fig2.update_yaxes(title_text="累计净值", fixedrange=True)
    fig2.update_layout(
        height=_TAG4_H, margin=dict(t=16, b=30, l=50, r=64),
        autosize=True, font=dict(size=_FONT_CELL),
        hovermode=False, dragmode="zoom", showlegend=False,
    )

    # view 3: 累积 IC (单因子 + 聚合). NaN 当 0, 与分层累计同口径.
    # 横向 zoom + 15 日均线锚, 见 RENORM_JS.
    colors_f = px_colors(n_factor)
    fig3 = go.Figure()
    for f in range(n_factor):
        nm = factor_names[f]
        cum = _cumsum_ic(an["factor_ic"][f])
        fig3.add_trace(go.Scatter(
            x=dates, y=cum, mode="lines",
            name=nm, legendgroup=nm, showlegend=False,
            hoverinfo="skip",
            line=dict(color=colors_f[f])))
        fig3.add_trace(_end_label(dates, cum, nm, colors_f[f]))
    score_cum = _cumsum_ic(an["score_ic"])
    fig3.add_trace(go.Scatter(
        x=dates, y=score_cum, mode="lines",
        name="聚合", legendgroup="聚合", showlegend=False,
        hoverinfo="skip",
        line=dict(color=_STRAT_COLOR, width=2)))
    fig3.add_trace(_end_label(dates, score_cum, "聚合", _STRAT_COLOR))
    fig3.update_yaxes(title_text="累积IC", fixedrange=True)
    fig3.update_layout(
        height=_TAG4_H, margin=dict(t=16, b=30, l=50, r=72),
        autosize=True, font=dict(size=_FONT_CELL),
        hovermode=False, dragmode="zoom", showlegend=False,
    )

    # view 4: 因子表格
    summary_rows = []
    for f in range(n_factor):
        ic_full = an["factor_ic"][f]
        tail = ic_full[-20:]
        ic_now = ic_full[-1] if np.isfinite(ic_full[-1]) else (
            np.nanmean(tail) if np.any(np.isfinite(tail)) else float("nan"))
        ic_ma_full = _rolling_mean(ic_full, meta["config"]["ic_ma_window"])
        ic_ma_now = ic_ma_full[-1]
        ic_mean = float(np.nanmean(ic_full))
        ic_std = float(np.nanstd(ic_full))
        ir = ic_mean / ic_std * \
            np.sqrt(TRADING_DAYS) if ic_std > 0 else float("nan")
        turn = float(np.nanmean(an["factor_turnover"][f]))
        summary_rows.append(
            (factor_names[f], ic_now, ic_ma_now, ic_mean, ir, turn))
    df_sum = pd.DataFrame(summary_rows, columns=[
        "因子", "当期IC", f"IC均值({meta['config']['ic_ma_window']})",
        "平均IC(全range)", "IR", "换手率"])
    fig5 = go.Figure(go.Table(
        header=_tbl_header(list(df_sum.columns), align="left"),
        cells=_tbl_cells(
            [
                df_sum["因子"],
                [f"{v:.4f}" for v in df_sum["当期IC"]],
                [f"{v:.4f}" for v in df_sum[f"IC均值({meta['config']['ic_ma_window']})"]],
                [f"{v:.4f}" for v in df_sum["平均IC(全range)"]],
                [f"{v:.4f}" for v in df_sum["IR"]],
                [f"{v*100:.2f}%" for v in df_sum["换手率"]],
            ],
            align="left",
            height=_fit_cell_h(_TAG4_H, n_rows=len(df_sum)),
        ),
    ))
    fig5.update_layout(height=_TAG4_H, margin=_MARGIN_TBL, autosize=True)

    # view 5: 因子相关性矩阵
    fig6 = go.Figure(go.Heatmap(z=an["factor_corr"], x=factor_names, y=factor_names,
                                colorscale="RdBu", zmid=0,
                                colorbar=dict(title="corr")))
    fig6.update_layout(
        height=_TAG4_H, margin=_MARGIN_PLT, autosize=True,
        font=dict(size=_FONT_CELL), xaxis=dict(tickangle=-45),
    )

    return [
        ("分层年化对比", fig1),
        ("分层累计收益", fig2),
        ("累积因子IC", fig3),
        ("因子表格", fig5),
        ("因子相关性矩阵", fig6),
    ]


def px_colors(n: int) -> list[str]:
    """生成 n 个区分色 (HSL 均分)."""
    return [f"hsl({int(i * 360 / max(n, 1))},65%,50%)" for i in range(n)]


# ============================================================================
# 报告组装
# ============================================================================

REPORT_HEAD = """<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>策略回测 / 因子分析报告</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei", sans-serif;
       margin: 0; padding: 12px 24px; background: #fafafa; }
h1 { font-size: 20px; }
h2 { font-size: 15px; margin: 0 0 10px; color: #444; }
.tag-section { margin-top: 32px; padding: 16px; background: #fff;
               border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,.08);
               box-sizing: border-box; }
.view-buttons { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 14px; }
.view-btn { padding: 6px 14px; border: 1px solid #ccc; border-radius: 4px;
            background: #f5f5f5; cursor: pointer; font-size: 14px; white-space: nowrap; }
.view-btn.active { background: steelblue; color: #fff; border-color: steelblue; }
.view-btn:hover:not(.active) { background: #e0e8f0; }
.view-container { position: relative; }
.view-container .view {
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  visibility: hidden;
}
.view-container .view.active { visibility: visible; }
.view-container .view.scroll { overflow: auto; }
table.tbl { border-collapse: collapse; width: max-content; min-width: 100%;
            font-size: 12px; }
table.tbl th, table.tbl td {
  height: 22px; padding: 0 8px; white-space: nowrap; text-align: left;
  border-bottom: 1px solid #eee;
}
table.tbl th {
  position: sticky; top: 0; background: lightsteelblue; color: #222;
  font-weight: 600; z-index: 1;
}
table.tbl td:first-child { background: #d9e9fb; }
table.tbl-compact { font-size: 11px; }
table.tbl-compact th, table.tbl-compact td {
  height: 18px; padding: 0 5px; line-height: 18px; white-space: nowrap;
}
</style>
<script>
function showView(tagIdx, viewIdx) {
  var section = document.querySelectorAll('.tag-section')[tagIdx];
  section.querySelectorAll('.view').forEach(function(v, i) {
    v.classList.toggle('active', i === viewIdx);
  });
  section.querySelectorAll('.view-btn').forEach(function(b, i) {
    b.classList.toggle('active', i === viewIdx);
  });
}
</script>
</head><body>
<h1>策略回测 / 因子分析报告</h1>
"""

REPORT_FOOT = "</body></html>\n"

# 横向 zoom 后重新对齐 y (tag2_view1 净值 / tag4_view2 分层累计 / tag4_view3 累积IC):
#   - 每条线用窗口起点处自己的 15 日均线做锚: y' = y / ma[i0] - 1
#     起点高于均线 2% → 从 2% 起, 不再把所有线拧到同一点
#   - 纵向按窗口内可见数据重算 full range (仅 yaxis 'y'; TAG2 回撤/仓位不动)
#   - 双击 reset (xaxis.autorange) 恢复原始 y
RENORM_JS = """<script>
(function () {
  var MA_W = 15;
  function toArr(v) { return Array.prototype.slice.call(v); }
  function ms(v) {
    if (typeof v === 'string') v = v.replace(' ', 'T');
    var t = new Date(v).getTime();
    return isNaN(t) ? null : t;
  }
  function rollingMa(y0, w) {
    var n = y0.length, ma = new Array(n);
    var sum = 0, cnt = 0;
    for (var i = 0; i < n; i++) {
      var v = y0[i];
      if (isFinite(v)) { sum += v; cnt++; }
      var j = i - w;
      if (j >= 0 && isFinite(y0[j])) { sum -= y0[j]; cnt--; }
      ma[i] = cnt > 0 ? sum / cnt : NaN;
    }
    return ma;
  }
  function bindRenorm(id) {
    var gd = document.getElementById(id);
    if (!gd) return;
    function ready() {
      if (!gd.data || !gd._fullData || !gd._fullLayout) {
        setTimeout(ready, 80); return;
      }

      // gd.data 里的数值数组可能是 {dtype, bdata} base64 编码 (plotly.py >= 6);
      // 一律从 gd._fullData 取已解码的 TypedArray 作基线.
      var navTraces = [];  // {idx, group, y0} 线 / marker
      var lines = {};      // group -> {y0, ma}
      var labels = [];     // {idx, group} 线尾文字
      var xs = null;       // line x (ms)
      var xsRaw = null;
      gd.data.forEach(function (tr, i) {
        if ((tr.yaxis || 'y') !== 'y' || !tr.legendgroup) return;
        var ft = gd._fullData[i];
        var y0 = toArr(ft.y);
        if (!y0.length) return;
        var mode = tr.mode || '';
        if (mode.indexOf('text') !== -1 && mode.indexOf('lines') === -1) {
          labels.push({ idx: i, group: tr.legendgroup });
          return;
        }
        navTraces.push({ idx: i, group: tr.legendgroup, y0: y0 });
        if (mode.indexOf('lines') !== -1) {
          lines[tr.legendgroup] = { y0: y0, ma: rollingMa(y0, MA_W) };
          if (xs === null) {
            xsRaw = toArr(ft.x);
            xs = xsRaw.map(function (v) {
              return new Date(v).getTime();
            });
          }
        }
      });
      var groups = Object.keys(lines);
      if (!groups.length || xs === null) return;

      var ylay = gd.layout.yaxis || {};
      var origFmt = ylay.tickformat || '';
      var origTitle = (ylay.title && ylay.title.text) || '';

      function visY(y, m) { return y / m - 1; }

      function pinLabels(iAt, yAt) {
        if (!labels.length) return Promise.resolve();
        var raw = labels.map(function (lb) { return yAt(lb.group); });
        var lo = Infinity, hi = -Infinity;
        raw.forEach(function (v) {
          if (!isFinite(v)) return;
          if (v < lo) lo = v;
          if (v > hi) hi = v;
        });
        var gap = ((hi - lo) || 1) * 0.035;
        var order = raw.map(function (_, i) { return i; })
          .sort(function (a, b) { return raw[a] - raw[b]; });
        var placed = raw.slice();
        for (var k = 1; k < order.length; k++) {
          var i = order[k], p = order[k - 1];
          if (placed[i] < placed[p] + gap) placed[i] = placed[p] + gap;
        }
        return Plotly.restyle(gd, {
          x: labels.map(function () { return [xsRaw[iAt]]; }),
          y: placed.map(function (v) { return [v]; })
        }, labels.map(function (lb) { return lb.idx; }));
      }

      function restore() {
        var ys = navTraces.map(function (t) { return t.y0; });
        var idxs = navTraces.map(function (t) { return t.idx; });
        var n = xs.length;
        Plotly.restyle(gd, { y: ys }, idxs).then(function () {
          return pinLabels(n - 1, function (g) { return lines[g].y0[n - 1]; });
        }).then(function () {
          var lo = Infinity, hi = -Infinity;
          groups.forEach(function (g) {
            var y0 = lines[g].y0;
            for (var i = 0; i < y0.length; i++) {
              var v = y0[i];
              if (!isFinite(v)) continue;
              if (v < lo) lo = v;
              if (v > hi) hi = v;
            }
          });
          if (!isFinite(lo) || !isFinite(hi)) return;
          var pad = (hi - lo) * 0.05 || 0.05;
          return Plotly.relayout(gd, {
            'yaxis.range': [lo - pad, hi + pad],
            'yaxis.tickformat': origFmt,
            'yaxis.title.text': origTitle
          });
        });
      }

      function renorm(t0, t1) {
        var i0 = 0, i1 = xs.length - 1;
        if (t0 !== null) {
          while (i0 < i1 && xs[i0] < t0) i0++;
          while (i1 > i0 && xs[i1] > t1) i1--;
        }
        var anchor = {}, ok = true;
        groups.forEach(function (g) {
          var m = lines[g].ma[i0];
          if (!isFinite(m) || Math.abs(m) < 1e-12) m = lines[g].y0[i0];
          if (!isFinite(m) || Math.abs(m) < 1e-12) { ok = false; return; }
          anchor[g] = m;
        });
        if (!ok) return;
        var ys = navTraces.map(function (t) {
          var m = anchor[t.group];
          return t.y0.map(function (v) { return visY(v, m); });
        });
        var idxs = navTraces.map(function (t) { return t.idx; });
        Plotly.restyle(gd, { y: ys }, idxs).then(function () {
          return pinLabels(i1, function (g) {
            return visY(lines[g].y0[i1], anchor[g]);
          });
        }).then(function () {
          var lo = Infinity, hi = -Infinity;
          groups.forEach(function (g) {
            var y0 = lines[g].y0, m = anchor[g];
            for (var i = i0; i <= i1; i++) {
              var v = visY(y0[i], m);
              if (!isFinite(v)) continue;
              if (v < lo) lo = v;
              if (v > hi) hi = v;
            }
          });
          if (!isFinite(lo) || !isFinite(hi)) return;
          var pad = (hi - lo) * 0.05 || 0.05;
          return Plotly.relayout(gd, {
            'yaxis.range': [lo - pad, hi + pad],
            'yaxis.tickformat': '.0%',
            'yaxis.title.text': '相对15日均线'
          });
        });
      }

      gd.on('plotly_relayout', function (ev) {
        if (!ev) return;
        var keys = Object.keys(ev);
        var reset = keys.some(function (k) {
          return /^xaxis\\d*\\.autorange$/.test(k) && ev[k];
        });
        if (reset) { restore(); return; }
        var t0 = null, t1 = null;
        keys.forEach(function (k) {
          var m = k.match(/^xaxis\\d*\\.range(?:\\[(\\d)\\])?$/);
          if (!m) return;
          if (m[1] === '0') t0 = ev[k];
          else if (m[1] === '1') t1 = ev[k];
          else { t0 = ev[k][0]; t1 = ev[k][1]; }
        });
        if (t0 === null || t1 === null) return;
        // plotly range 字符串是 '2021-03-04 12:34:56.789' (空格分隔),
        // 换成 'T' 保证 Date 解析跨浏览器一致.
        var a = ms(t0), b = ms(t1);
        if (a === null || b === null) return;
        renorm(a, b);
      });
      pinLabels(xs.length - 1, function (g) {
        return lines[g].y0[xs.length - 1];
      });
    }
    ready();
  }
  bindRenorm('tag2_view1');
  bindRenorm('tag4_view2');
  bindRenorm('tag4_view3');
})();
</script>"""


def main():
    data = _load()
    bt = data["bt"]
    an = data["an"]
    meta = data["meta"]
    codes = data["codes"]
    dates_all = data["dates_all"]

    tag_views = [
        ("TAG 1: 策略指标 / 交易统计", fig_tag1(bt, an, meta)),
        ("TAG 2: 回测曲线 / 分布 / 表格", fig_tag2(bt, an, meta, codes)),
        ("TAG 3: 今日持仓 / 交易记录", fig_tag3(bt, codes, dates_all)),
        ("TAG 4: 排名分析 / 因子相关性", fig_tag4(an, meta)),
    ]

    tag_heights = list(_TAG_H)

    out_path = OUT_DIR / "report.html"
    parts = [REPORT_HEAD]
    for tag_i, (tag_name, views) in enumerate(tag_views):
        h = tag_heights[tag_i]
        parts.append('<div class="tag-section">')
        parts.append(f'<h2>{tag_name}</h2>')
        parts.append('<div class="view-buttons">')
        for view_i, (label, _) in enumerate(views):
            active = " active" if view_i == 0 else ""
            parts.append(
                f'<button class="view-btn{active}" '
                f'onclick="showView({tag_i},{view_i})">{label}</button>'
            )
        parts.append('</div>')
        parts.append(f'<div class="view-container" style="height:{h}px">')
        for view_i, (label, fig) in enumerate(views):
            cls = "view active" if view_i == 0 else "view"
            if isinstance(fig, str):
                cls += " scroll"
                parts.append(f'<div class="{cls}">{fig}</div>')
                continue
            div = fig.to_html(full_html=False, include_plotlyjs=False,
                              div_id=f"tag{tag_i+1}_view{view_i+1}",
                              default_width="100%",
                              default_height="100%",
                              config={"responsive": True})
            parts.append(f'<div class="{cls}">{div}</div>')
        parts.append('</div>')
        parts.append('</div>')
    parts.append(RENORM_JS)
    parts.append(REPORT_FOOT)
    out_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"report -> {out_path}")
    webbrowser.open(out_path.as_uri())


if __name__ == "__main__":
    main()
