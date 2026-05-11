"""读 output/ 下 cpp 写出的 npy/json, 用 plotly 出一个 HTML 报告.

用法:
    python -m py.report          # 生成 output/report.html 并打开浏览器

布局: 4 个 TAG 板块, 每块用 plotly updatemenu (dropdown) 在子视图间切换.
    TAG 1: 指标卡 (策略 / pool 指数) | 交易统计
    TAG 2: 收益/回撤曲线 | 收益分布 (年/月/周) | 月换手率分布 | 日仓位分布 |
            年收益表 | 月收益表 | 交易收益分布
    TAG 3: 每天持仓 | 交易记录
    TAG 4: 排名分析 (分层年收益/累计/聚合IC) | 因子相关性 (单因子IC/汇总表/相关阵)

数据全部来自:
    output/meta.json
    output/backtest/*.npy
    output/analysis/*.npy
"""
from __future__ import annotations

import json
import webbrowser
from pathlib import Path

import numpy as np
import pandas as pd
import plotly.graph_objects as go

# ============================================================================
# 数据加载
# ============================================================================

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "output"
BT_DIR = OUT_DIR / "backtest"
AN_DIR = OUT_DIR / "analysis"

TRADING_DAYS = 252

# 各 TAG 外层 .view-container 高度 (px); TAG 内所有子图共用, 切换对齐
_TAG_H = (160, 480, 620, 480)
_TAG1_H, _TAG2_H, _TAG3_H, _TAG4_H = _TAG_H

_MARGIN_TBL = dict(t=8, b=8, l=8, r=8)
_MARGIN_PLT = dict(t=30, b=30, l=50, r=20)

# 表格唯一字号 / 行高 (px); 不设 columnwidth, 列宽由 Plotly 均分
_FONT_HEADER = 13
_FONT_CELL = 13
_HEADER_H = 30
_MIN_CELL_H = 24


def _fit_cell_h(total_h: int, n_rows: int, header_h: int = _HEADER_H,
                margin: dict = _MARGIN_TBL) -> int:
    """让 header + n_rows*cell_h 尽量填满 figure 的可绘制区."""
    assert n_rows > 0, "n_rows must be > 0"
    avail = total_h - margin["t"] - margin["b"] - header_h
    return max(_MIN_CELL_H, avail // n_rows)


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

    basic = json.loads(
        (ROOT / "data" / "_meta" / "stock_basic.json").read_text(encoding="utf-8")
    )
    name_map = {x["ts_code"]: x["name"] for x in basic}
    for c in codes:
        assert c in name_map, f"缺中文名: {c}"

    bt_d_idx = np.load(BT_DIR / "dates.npy")
    bt_dates_str = dates_all[bt_d_idx]
    bt_dates = pd.to_datetime(bt_dates_str, format="%Y%m%d")

    bt = {
        "dates_idx": bt_d_idx,
        "dates": bt_dates,
        "strategy_nav": np.load(BT_DIR / "strategy_nav.npy"),
        "pool_nav": np.load(BT_DIR / "pool_nav.npy"),
        "position_count": np.load(BT_DIR / "position_count.npy"),
        "position_pct": np.load(BT_DIR / "position_pct.npy"),
        "turnover": np.load(BT_DIR / "turnover.npy"),
        "susp_pct": np.load(BT_DIR / "susp_pct.npy"),
        "executable_pct": np.load(BT_DIR / "executable_pct.npy"),
        "holdings_offsets": np.load(BT_DIR / "holdings_offsets.npy"),
        "holdings_codes": np.load(BT_DIR / "holdings_codes.npy"),
        "holdings_weights": np.load(BT_DIR / "holdings_weights.npy"),
        "trades_inst": np.load(BT_DIR / "trades_inst.npy"),
        "trades_open_d": np.load(BT_DIR / "trades_open_d.npy"),
        "trades_close_d": np.load(BT_DIR / "trades_close_d.npy"),
        "trades_open_px": np.load(BT_DIR / "trades_open_px.npy"),
        "trades_close_px": np.load(BT_DIR / "trades_close_px.npy"),
        "trades_buy_value": np.load(BT_DIR / "trades_buy_value.npy"),
        "trades_pv_at_open": np.load(BT_DIR / "trades_pv_at_open.npy"),
    }

    an_d_idx = np.load(AN_DIR / "dates.npy")
    assert np.array_equal(an_d_idx, bt_d_idx), "analysis/backtest dates 不一致"
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
        "name_map": name_map,
        "bt": bt,
        "an": an,
    }


def _label(code: str, name_map: dict) -> str:
    """标的显示: 中文名在前, 括号注释代码."""
    return f"{name_map[code]} ({code})"


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
    """TAG 1 视图1: 策略/pool 指数指标."""
    nav_s = bt["strategy_nav"]
    nav_p = bt["pool_nav"]
    ret_s = np.diff(nav_s) / nav_s[:-1]
    ret_s = np.concatenate([[0.0], ret_s])
    ret_p = np.diff(nav_p) / nav_p[:-1]
    ret_p = np.concatenate([[0.0], ret_p])

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
    }


def _trade_stats(bt, an, meta) -> dict:
    """TAG 1 视图2: 交易统计."""
    n_trades = len(bt["trades_inst"])
    if n_trades == 0:
        return {"换股次数": 0}
    holding_days = bt["trades_close_d"] - bt["trades_open_d"]
    rets = bt["trades_close_px"] / bt["trades_open_px"] - 1.0
    pos_rets = rets[rets > 0]
    neg_rets = rets[rets < 0]
    win_rate = float((rets > 0).mean())

    nav_s = bt["strategy_nav"]
    n_d = len(nav_s)
    years = n_d / TRADING_DAYS

    daily_ret_s = np.concatenate([[0.0], np.diff(nav_s) / nav_s[:-1]])

    dates = bt["dates"]
    daily_df = pd.DataFrame({"date": dates, "ret": daily_ret_s})
    daily_df = daily_df.set_index("date")

    weekly = daily_df.resample("W").apply(lambda x: (1 + x).prod() - 1)
    monthly = daily_df.resample("ME").apply(lambda x: (1 + x).prod() - 1)

    return {
        "年换手率": float(np.nanmean(bt["turnover"]) * TRADING_DAYS),
        "平均持有天数": float(np.mean(holding_days)),
        "平均持仓股票数": float(np.mean(bt["position_count"])),
        "平均交易收益": float(np.mean(rets)),
        "正收益平均": float(np.mean(pos_rets)) if len(pos_rets) else float("nan"),
        "负收益平均": float(np.mean(neg_rets)) if len(neg_rets) else float("nan"),
        "交易赢率": win_rate,
        "换股次数": int(n_trades),
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
_INT_METRICS  = {"天数", "创新高最长天数", "换股次数"}


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
                      row_colors: list | None = None) -> go.Table:
    """列 = 指标, 行 = 实体."""
    n_rows = len(row_labels)
    if row_colors is None:
        palette = ["#eef2f7", "#ffffff", "#fdf6e3", "#f0faf0", "#faf0f0"]
        row_colors = [palette[i % len(palette)] for i in range(n_rows)]

    cols = [row_labels]
    for metric in col_headers:
        cols.append([_fmt_v(row.get(metric, "—"), metric) for row in row_data])

    fill = [[row_colors[r] for r in range(n_rows)] for _ in range(len(cols))]
    ch = _fit_cell_h(_TAG1_H, n_rows=n_rows)

    return go.Table(
        header=_tbl_header([""] + col_headers, align="center"),
        cells=_tbl_cells(cols, align="center", height=ch, fill_color=fill),
    )


def _trade_stats_fig(trades: dict) -> go.Figure:
    """交易统计: 单表密集 2 行 (9 列), 中间一行为第二组表头."""
    keys1 = ["换股次数", "平均持有天数", "平均持仓股票数", "平均持仓仓位",
             "调仓指令可执行比例", "持仓停牌股票比例",
             "平均交易收益", "正收益平均", "负收益平均"]
    keys2 = ["交易赢率", "日赢率", "周赢率", "月赢率", "年换手率",
             "指数跟踪误差", "创新高最长天数", "CPU时长(秒)", "Tensor内存(GB)"]
    row_v1 = [_fmt_v(trades.get(k, "—"), k) for k in keys1]
    row_v2 = [_fmt_v(trades.get(k, "—"), k) for k in keys2]
    steel = "lightsteelblue"
    fill_2d = [["#f5f7fb"] * 9, [steel] * 9, ["#f5f7fb"] * 9]

    fig = go.Figure(go.Table(
        header=_tbl_header(keys1, align="center"),
        cells=_tbl_cells([row_v1, keys2, row_v2], align="center",
                         height=_fit_cell_h(_TAG1_H, n_rows=3),
                         fill_color=fill_2d),
    ))
    fig.update_layout(height=_TAG1_H, margin=_MARGIN_TBL, autosize=True)
    return fig


def fig_tag1(bt, an, meta) -> list[tuple[str, go.Figure]]:
    indicators = _stat_indicators(bt, an, meta)
    trades = _trade_stats(bt, an, meta)

    # ── view1: 策略指标横排 (3 行: 策略 / pool指数 / 超额) ──
    strat = indicators["策略"]
    pool  = indicators["pool指数"]

    excess: dict = {}
    for k in ["年化", "波动率", "夏普", "最大回撤"]:
        sv, pv = strat.get(k), pool.get(k)
        excess[k] = (sv - pv) if isinstance(sv, float) and isinstance(pv, float) else "—"
    for k in ["信息比率", "Alpha", "跟踪误差"]:
        excess[k] = strat.get(k, "—")

    metrics_v1 = ["天数", "年化", "波动率", "夏普", "最大回撤",
                  "信息比率", "Beta", "Alpha", "跟踪误差", "创新高最长天数"]
    fig1 = go.Figure(_transposed_table(
        col_headers=metrics_v1,
        row_labels=["策略", "pool指数", "超额"],
        row_data=[strat, pool, excess],
        row_colors=["#ddeeff", "#f5f5f5", "#fff8e6"],
    ))
    fig1.update_layout(height=_TAG1_H, margin=_MARGIN_TBL, autosize=True)

    # ── view2: 交易统计 (2 行 x 9 列) ──
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


def fig_tag2(bt, an, meta) -> list[tuple[str, go.Figure]]:
    nav_s = bt["strategy_nav"]
    nav_p = bt["pool_nav"]
    dates = bt["dates"]
    daily_s = np.concatenate([[0.0], np.diff(nav_s) / nav_s[:-1]])
    daily_p = np.concatenate([[0.0], np.diff(nav_p) / nav_p[:-1]])
    dd_s = _drawdown_curve(nav_s)
    dd_p = _drawdown_curve(nav_p)

    def _chart(*traces, **kw):
        fig = go.Figure()
        for tr in traces:
            fig.add_trace(tr)
        fig.update_layout(height=_TAG2_H, margin=_MARGIN_PLT,
                          autosize=True, font=dict(size=_FONT_CELL), **kw)
        return fig

    # view 1: 收益曲线
    fig1 = _chart(
        go.Scatter(x=dates, y=nav_s / nav_s[0],
                   mode="lines", name="策略", line=dict(color="crimson")),
        go.Scatter(x=dates, y=nav_p / nav_p[0],
                   mode="lines", name="pool指数", line=dict(color="steelblue")),
    )

    # view 2: 回撤曲线
    fig2 = _chart(
        go.Scatter(x=dates, y=dd_s, mode="lines",
                   name="策略回撤", line=dict(color="crimson"), fill="tozeroy"),
        go.Scatter(x=dates, y=dd_p, mode="lines",
                   name="pool回撤", line=dict(color="steelblue"), fill="tozeroy"),
    )

    # view 3: 年/月/周收益分布
    df = pd.DataFrame({"d": dates, "r": daily_s}).set_index("d")
    yearly = df.resample("YE").apply(lambda x: (1 + x).prod() - 1)["r"].dropna().values
    monthly = df.resample("ME").apply(lambda x: (1 + x).prod() - 1)["r"].dropna().values
    weekly = df.resample("W").apply(lambda x: (1 + x).prod() - 1)["r"].dropna().values
    fig3 = _chart(
        go.Histogram(x=yearly, name="年", marker_color="darkred", xbins=dict(size=0.05)),
        go.Histogram(x=monthly, name="月", marker_color="orange", xbins=dict(size=0.02)),
        go.Histogram(x=weekly, name="周", marker_color="gold", xbins=dict(size=0.01)),
        barmode="group",
    )

    # view 4: 月换手率分布
    turn_df = pd.DataFrame({"d": dates, "t": bt["turnover"]}).set_index("d")
    monthly_turn = turn_df.resample("ME").sum()["t"].dropna().values
    fig4 = _chart(go.Histogram(x=monthly_turn, name="月换手率", marker_color="purple"))

    # view 5: 日仓位分布
    fig5 = _chart(go.Histogram(x=bt["position_pct"], name="日仓位", marker_color="seagreen"))

    # view 6: 年收益表
    yt = _resample_table(daily_s, daily_p, dates, "YE")
    fig6 = go.Figure(go.Table(
        header=_tbl_header(["年份"] + list(yt.columns), align="left"),
        cells=_tbl_cells(
            [yt.index.tolist()] +
            [[f"{v*100:.2f}%" if abs(v) < 1 else f"{v:.4f}"
              if isinstance(v, float) else v
              for v in yt[c]] for c in yt.columns],
            align="left",
            height=_fit_cell_h(_TAG2_H, n_rows=len(yt.index)),
        ),
    ))
    fig6.update_layout(height=_TAG2_H, margin=_MARGIN_TBL, autosize=True)

    # view 7: 月收益表 (长表, 行多时 plotly 自动滚动)
    mt = _resample_table(daily_s, daily_p, dates, "ME")
    fig7 = go.Figure(go.Table(
        header=_tbl_header(["月份"] + list(mt.columns), align="left"),
        cells=_tbl_cells(
            [mt.index.tolist()] +
            [[f"{v*100:.2f}%" if abs(v) < 1 else f"{v:.4f}"
              if isinstance(v, float) else v
              for v in mt[c]] for c in mt.columns],
            align="left",
            height=_fit_cell_h(_TAG2_H, n_rows=len(mt.index)),
        ),
    ))
    fig7.update_layout(height=_TAG2_H, margin=_MARGIN_TBL, autosize=True)

    # view 8: 交易收益分布
    if len(bt["trades_open_px"]) > 0:
        rets = bt["trades_close_px"] / bt["trades_open_px"] - 1.0
        fig8 = _chart(go.Histogram(x=rets, name="交易收益", nbinsx=80, marker_color="teal"))
    else:
        fig8 = _chart(go.Scatter(x=[], y=[], name="无交易"))

    return [
        ("收益曲线", fig1),
        ("回撤曲线", fig2),
        ("年/月/周收益分布", fig3),
        ("月换手率分布", fig4),
        ("日仓位分布", fig5),
        ("年收益表", fig6),
        ("月收益表", fig7),
        ("交易收益分布", fig8),
    ]


# ============================================================================
# TAG 3: 每天持仓 / 交易记录 (table 切换)
# ============================================================================

def fig_tag3(bt, codes, dates_all, name_map) -> list[tuple[str, go.Figure]]:
    # view 1: 每天持仓 — 最近优先
    off = bt["holdings_offsets"]
    rows = []
    n_d_bt = len(bt["dates"])
    for i in range(n_d_bt - 1, -1, -1):
        lo, hi = int(off[i]), int(off[i + 1])
        if hi == lo:
            continue
        date_str = bt["dates"][i].strftime("%Y-%m-%d")
        for k in range(lo, hi):
            a = int(bt["holdings_codes"][k])
            w = float(bt["holdings_weights"][k])
            rows.append((date_str, _label(codes[a], name_map), w))
    if rows:
        df = pd.DataFrame(rows, columns=["日期", "标的", "权重"])
        df = df.head(2000)
        fig1 = go.Figure(go.Table(
            header=_tbl_header(list(df.columns), align="left"),
            cells=_tbl_cells(
                [df["日期"], df["标的"], [f"{x*100:.2f}%" for x in df["权重"]]],
                align="left",
                height=_fit_cell_h(_TAG3_H, n_rows=len(df)),
            ),
        ))
    else:
        fig1 = go.Figure(go.Table(header=dict(values=["empty"]),
                                  cells=dict(values=[[]])))
    fig1.update_layout(height=_TAG3_H, margin=_MARGIN_TBL, autosize=True)

    # view 2: 交易记录
    if len(bt["trades_inst"]) > 0:
        order = np.argsort(-bt["trades_close_d"])
        rows = []
        for k in order[:2000]:
            a = int(bt["trades_inst"][k])
            od = int(bt["trades_open_d"][k])
            cd = int(bt["trades_close_d"][k])
            op = float(bt["trades_open_px"][k])
            cp = float(bt["trades_close_px"][k])
            rows.append((
                _label(codes[a], name_map),
                str(dates_all[od]),
                str(dates_all[cd]),
                cd - od,
                f"{op:.2f} → {cp:.2f}",
                f"{(cp / op - 1) * 100:.2f}%",
            ))
        df = pd.DataFrame(rows, columns=["标的", "开仓日", "平仓日",
                                         "持仓天数", "价格", "收益"])
        fig2 = go.Figure(go.Table(
            header=_tbl_header(list(df.columns), align="left"),
            cells=_tbl_cells(
                [df[c] for c in df.columns],
                align="left",
                height=_fit_cell_h(_TAG3_H, n_rows=len(df)),
            ),
        ))
    else:
        fig2 = go.Figure(go.Table(header=dict(values=["empty"]),
                                  cells=dict(values=[[]])))
    fig2.update_layout(height=_TAG3_H, margin=_MARGIN_TBL, autosize=True)

    return [
        ("每天持仓 (最近优先)", fig1),
        ("交易记录 (最近优先)", fig2),
    ]


# ============================================================================
# TAG 4: 排名分析 / 因子相关性
# ============================================================================

def _rolling_mean(x: np.ndarray, w: int) -> np.ndarray:
    s = pd.Series(x, dtype=float)
    return s.rolling(window=w, min_periods=max(1, w // 4)).mean().values


def fig_tag4(an, meta) -> list[tuple[str, go.Figure]]:
    factor_names = meta["factor_names"]
    n_factor = len(factor_names)
    dates = an["dates"]
    qret = an["quantile_ret"]
    Q = qret.shape[0]
    pool_ret = an["pool_ret"]

    # view 1: 分层年收益对比 (柱状)
    df_q = pd.DataFrame(qret.T, index=dates,
                        columns=[f"Q{q+1}" for q in range(Q)])
    df_q["pool"] = pool_ret
    yearly_q = df_q.resample("YE").apply(
        lambda x: (1 + x).prod() ** (TRADING_DAYS / max(1, len(x))) - 1)
    yearly_avg = yearly_q.mean(axis=0)
    fig1 = go.Figure(go.Bar(x=yearly_avg.index, y=yearly_avg.values,
                            marker_color=["steelblue"] * Q + ["darkred"],
                            name="年化均值"))
    fig1.update_layout(height=_TAG4_H, margin=_MARGIN_PLT,
                       autosize=True, font=dict(size=_FONT_CELL))

    # view 2: 分层累计收益曲线 (绝对)
    # NaN 视作 0 收益 (与 backtest pool_nav 在 dr_n=0 时 dr=0 同口径).
    # 不 nan_to_num 的话, numpy.cumprod 遇 NaN 后续全 NaN → 图断在首个空桶日.
    colors = px_colors(Q + 1)
    fig2 = go.Figure()
    for q in range(Q):
        cum = np.cumprod(1.0 + np.nan_to_num(qret[q], nan=0.0))
        fig2.add_trace(go.Scatter(x=dates, y=cum, mode="lines",
                                  name=f"Q{q+1}", line=dict(color=colors[q])))
    cum_p = np.cumprod(1.0 + np.nan_to_num(pool_ret, nan=0.0))
    fig2.add_trace(go.Scatter(x=dates, y=cum_p, mode="lines",
                              name="pool", line=dict(color="black", dash="dash")))
    fig2.update_layout(height=_TAG4_H, margin=_MARGIN_PLT,
                       autosize=True, font=dict(size=_FONT_CELL))

    # view 3: 聚合 factor_score IC (raw + 250d MA)
    ma = _rolling_mean(an["score_ic"], meta["config"]["ic_ma_window"])
    fig3 = go.Figure()
    fig3.add_trace(go.Scatter(x=dates, y=an["score_ic"], mode="lines",
                              name="raw", line=dict(color="lightgray")))
    fig3.add_trace(go.Scatter(x=dates, y=ma, mode="lines",
                              name=f"{meta['config']['ic_ma_window']}日均",
                              line=dict(color="crimson", width=2)))
    fig3.update_layout(height=_TAG4_H, margin=_MARGIN_PLT,
                       autosize=True, font=dict(size=_FONT_CELL))

    # view 4: 单个因子 IC 250 日均 (overlaid)
    colors_f = px_colors(n_factor)
    fig4 = go.Figure()
    for f in range(n_factor):
        ic_ma = _rolling_mean(an["factor_ic"][f], meta["config"]["ic_ma_window"])
        fig4.add_trace(go.Scatter(x=dates, y=ic_ma, mode="lines",
                                  name=factor_names[f], line=dict(color=colors_f[f])))
    fig4.update_layout(height=_TAG4_H, margin=_MARGIN_PLT,
                       autosize=True, font=dict(size=_FONT_CELL))

    # view 5: 因子表格
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
        ir = ic_mean / ic_std * np.sqrt(TRADING_DAYS) if ic_std > 0 else float("nan")
        turn = float(np.nanmean(an["factor_turnover"][f]))
        summary_rows.append((factor_names[f], ic_now, ic_ma_now, ic_mean, ir, turn))
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

    # view 6: 因子相关性矩阵
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
        ("聚合因子IC", fig3),
        ("单因子IC(250均)", fig4),
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
.view-container .view { width: 100%; }
</style>
<script>
function showView(tagIdx, viewIdx) {
  var section = document.querySelectorAll('.tag-section')[tagIdx];
  section.querySelectorAll('.view').forEach(function(v, i) {
    v.style.display = i === viewIdx ? '' : 'none';
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


def main():
    data = _load()
    bt = data["bt"]
    an = data["an"]
    meta = data["meta"]
    codes = data["codes"]
    dates_all = data["dates_all"]
    name_map = data["name_map"]

    tag_views = [
        ("TAG 1: 策略指标 / 交易统计", fig_tag1(bt, an, meta)),
        ("TAG 2: 收益曲线 / 分布 / 表格", fig_tag2(bt, an, meta)),
        ("TAG 3: 每天持仓 / 交易记录", fig_tag3(bt, codes, dates_all, name_map)),
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
        for view_i, (_, fig) in enumerate(views):
            style = '' if view_i == 0 else ' style="display:none"'
            div = fig.to_html(full_html=False, include_plotlyjs=False,
                              div_id=f"tag{tag_i+1}_view{view_i+1}",
                              default_width="100%",
                              config={"responsive": True})
            parts.append(f'<div class="view"{style}>{div}</div>')
        parts.append('</div>')
        parts.append('</div>')
    parts.append(REPORT_FOOT)
    out_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"report -> {out_path}")
    webbrowser.open(out_path.as_uri())


if __name__ == "__main__":
    main()
