"""读 output/ 下 cpp 写出的 npy/json, 用 plotly 出一个 HTML 报告.

用法:
    python -m py.report          # 生成 output/report.html 并打开浏览器

布局: 4 个 TAG 板块, 每块用 view-buttons 在子视图间切换.
    TAG 1: 指标卡 (策略 / pool 指数) | 交易统计
    TAG 2: 收益曲线+日仓位曲线 (1 张 3:1 叠图, 含买/卖 marker 与持仓 hover)
           | 回撤曲线 | 收益分布 (年/月/周) | 月换手率分布
           | 年收益表 | 月收益表 | 交易收益分布
    TAG 3: 每天持仓 | 交易记录
    TAG 4: 排名分析 (分层年收益/累计/聚合IC) | 因子相关性 (单因子IC/汇总表/相关阵)

数据全部来自:
    output/meta.json
    output/backtest/*.npy + labels.json   (后者: PIT 名, namechange 切段)
    output/analysis/*.npy
"""
from __future__ import annotations

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
# TAG1: header(22) + 3 cell rows(22 each) + margin(16) = 104, +6 余量
_TAG_H = (110, 480, 620, 480)
_TAG1_H, _TAG2_H, _TAG3_H, _TAG4_H = _TAG_H

_MARGIN_TBL = dict(t=8, b=8, l=8, r=8)
_MARGIN_PLT = dict(t=30, b=30, l=50, r=20)

# 表格仅固定字号与紧凑行高, 其他布局交给 Plotly 自动处理
_FONT_HEADER = 12
_FONT_CELL = 12
_HEADER_H = 22
_CELL_H = 22


def _fit_cell_h(total_h: int, n_rows: int,
               margin: dict = _MARGIN_TBL) -> int:
    """让单元格高度以紧凑值为主, 小屏下不致撑开."""
    assert n_rows > 0, "n_rows must be > 0"
    avail = total_h - margin["t"] - margin["b"] - _HEADER_H
    target = min(_CELL_H, avail // n_rows)
    return max(12, target)


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
        "position_count": np.load(BT_DIR / "position_count.npy"),
        "position_pct": np.load(BT_DIR / "position_pct.npy"),
        "turnover": np.load(BT_DIR / "turnover.npy"),
        "susp_pct": np.load(BT_DIR / "susp_pct.npy"),
        "executable_pct": np.load(BT_DIR / "executable_pct.npy"),
        "holdings_offsets": np.load(BT_DIR / "holdings_offsets.npy"),
        "holdings_codes": np.load(BT_DIR / "holdings_codes.npy"),
        "holdings_weights": np.load(BT_DIR / "holdings_weights.npy"),
        "holdings_names": np.asarray(labels["holdings_names"], dtype=object),
        "trades_inst": np.load(BT_DIR / "trades_inst.npy"),
        "trades_open_d": np.load(BT_DIR / "trades_open_d.npy"),
        "trades_close_d": np.load(BT_DIR / "trades_close_d.npy"),
        "trades_open_px": np.load(BT_DIR / "trades_open_px.npy"),
        "trades_close_px": np.load(BT_DIR / "trades_close_px.npy"),
        "trades_open_names": np.asarray(labels["trades_open_names"], dtype=object),
        "trades_close_names": np.asarray(labels["trades_close_names"], dtype=object),
    }
    # 健全性: labels 长度对齐
    assert len(bt["holdings_names"]) == len(bt["holdings_codes"]), \
        "labels.holdings_names 与 holdings_codes 长度不一致"
    assert len(bt["trades_open_names"]) == len(bt["trades_inst"]), \
        "labels.trades_open_names 与 trades_inst 长度不一致"
    assert len(bt["trades_close_names"]) == len(bt["trades_inst"]), \
        "labels.trades_close_names 与 trades_inst 长度不一致"

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
    """TAG 1 视图1: 策略指标."""
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
                      cell_height: int | None = None,
                      ) -> go.Table:
    """列 = 指标, 行 = 实体."""
    n_rows = len(row_labels)

    cols = [row_labels]
    for metric in col_headers:
        cols.append([_fmt_v(row.get(metric, "—"), metric) for row in row_data])

    ch = _fit_cell_h(_TAG1_H, n_rows=n_rows) if cell_height is None else cell_height

    return go.Table(
        header=_tbl_header(["指标"] + col_headers, align="center"),
        cells=_tbl_cells(cols, align="center", height=ch),
    )


def _wide_metric_table(index: pd.Index,
                      rows: pd.DataFrame,
                      *,
                      align: str = "left") -> go.Table:
    """指标行在左, 期次列在上，适配较宽的年度/月度表.

    目标: 横向布局 + 仅保留标准表头，不出现“指标/数值”二级表头。
    """
    metric_names = list(rows.columns)
    if rows.empty:
        return go.Table(
            header=dict(values=["empty"]),
            cells=dict(values=[[]]),
        )

    def _fmt(v, metric_key: str):
        if not isinstance(v, (int, float, np.integer, np.floating)):
            return str(v)
        if not np.isfinite(v):
            return "nan"
        if metric_key in {"策略收益", "基准收益", "策略最大回撤", "基准最大回撤", "跟踪误差",
                          "信息比率", "波动率", "夏普比率"}:
            return f"{v*100:.2f}%"
        return f"{v:.4f}"

    cols = [metric_names]
    for k in index.astype(str):
        cols.append([_fmt(rows.loc[k, m], m) for m in metric_names])

    header = ["指标"] + list(index.astype(str))
    fill = [["#d9e9fb"] * len(metric_names)]
    for _ in index:
        fill.append(["#f8fbff"] * len(metric_names))

    return go.Table(
        header=_tbl_header(header, align=align),
        cells=_tbl_cells(
            cols,
            align=align,
            height=_fit_cell_h(_TAG2_H, n_rows=len(metric_names)),
            fill_color=fill if len(index) > 0 else None,
        ),
    )


def _trade_stats_fig(trades: dict) -> go.Figure:
    """交易统计: 单张 Plotly Table (9 列 × 1 header + 3 cell rows).

    上下两组指标拼成一张表, 中段用表头底色作为第二段视觉表头, 无 subplot 间隙.
    """
    trade_metrics = [
        "换股次数", "平均持有天数", "平均持仓股票数", "平均持仓仓位",
        "调仓指令可执行比例", "持仓停牌股票比例",
        "平均交易收益", "正收益平均", "负收益平均",
        "交易赢率", "日赢率", "周赢率", "月赢率",
        "年换手率", "指数跟踪误差", "创新高最长天数",
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


def _build_holdings_hover(bt, codes) -> list[str]:
    """每个 bt 交易日的持仓 hover 文本 (按权重降序; cpp 已排好).

    单格式: "{name} ({code}) {w:.1%}", 多行用 <br> 拼接.
    """
    off = bt["holdings_offsets"]
    hcodes = bt["holdings_codes"]
    hweights = bt["holdings_weights"]
    hnames = bt["holdings_names"]
    n = len(bt["dates"])
    out = [""] * n
    for i in range(n):
        lo, hi = int(off[i]), int(off[i + 1])
        if hi == lo:
            out[i] = "(空仓)"
            continue
        lines = [
            f"{hnames[k]} ({codes[int(hcodes[k])]}) {hweights[k] * 100:.1f}%"
            for k in range(lo, hi)
        ]
        out[i] = "<br>".join(lines)
    return out


def _build_trade_markers(bt, codes, side: str, nav_norm: np.ndarray,
                          y_offset: float):
    """聚合同日多笔为一个 marker. side ∈ {'buy','sell'}.

    返回 (xs, ys, hovers).
    - buy:  x=open_d, name 取 trades_open_names (PIT 开仓日)
    - sell: x=close_d, name 取 trades_close_names (PIT 平仓日)
    - y_offset: marker 相对曲线点的纵向偏移 (买<0 下移, 卖>0 上移)
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
        op = float(px_open[k]); cp = float(px_close[k])
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
        ys.append(float(nav_norm[i_bt]) + y_offset)
        lines = by_d[i_bt]
        head = f"<b>{label} ×{len(lines)}</b>"
        hovers.append(head + "<br>" + "<br>".join(lines))
    return xs, ys, hovers


def fig_tag2(bt, an, meta, codes) -> list[tuple[str, go.Figure]]:
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

    # view 1: 收益曲线 (3) + 日仓位曲线 (1), shared x.
    #   - 收益曲线 hover: 当日持仓 (PIT 名 / code / 权重, 权重降序)
    #   - 同日多笔买/卖 聚合为 1 个 marker (▲买 / ▼卖), hover 列明
    nav_s_norm = nav_s / nav_s[0]
    nav_p_norm = nav_p / nav_p[0]
    holdings_hover = _build_holdings_hover(bt, codes)

    fig1 = make_subplots(
        rows=2, cols=1, shared_xaxes=True,
        row_heights=[3, 1], vertical_spacing=0.04,
    )

    fig1.add_trace(go.Scatter(
        x=dates, y=nav_s_norm, mode="lines", name="策略",
        line=dict(color="crimson"),
        customdata=holdings_hover,
        hovertemplate=("<b>策略</b> %{y:.4f}<br>"
                       "<br><b>持仓</b><br>%{customdata}<extra></extra>"),
    ), row=1, col=1)
    fig1.add_trace(go.Scatter(
        x=dates, y=nav_p_norm, mode="lines", name="pool指数",
        line=dict(color="steelblue"),
        hovertemplate="<b>pool</b> %{y:.4f}<extra></extra>",
    ), row=1, col=1)

    # marker 纵向偏移: 买曲线下 / 卖曲线上, 偏移 ≈ 整段 y range 的 2%.
    y_max = float(max(nav_s_norm.max(), nav_p_norm.max()))
    y_min = float(min(nav_s_norm.min(), nav_p_norm.min()))
    off = (y_max - y_min) * 0.02

    bx, by, bh = _build_trade_markers(bt, codes, "buy",  nav_s_norm, -off)
    sx, sy, sh = _build_trade_markers(bt, codes, "sell", nav_s_norm, +off)
    if bx:
        fig1.add_trace(go.Scatter(
            x=bx, y=by, mode="markers", name="买入",
            marker=dict(symbol="triangle-up", color="green", size=6),
            customdata=bh,
            hovertemplate="%{customdata}<extra></extra>",
        ), row=1, col=1)
    if sx:
        fig1.add_trace(go.Scatter(
            x=sx, y=sy, mode="markers", name="卖出",
            marker=dict(symbol="triangle-down", color="red", size=6),
            customdata=sh,
            hovertemplate="%{customdata}<extra></extra>",
        ), row=1, col=1)

    fig1.add_trace(go.Scatter(
        x=dates, y=bt["position_pct"], mode="lines", name="仓位",
        line=dict(color="seagreen"), fill="tozeroy",
        fillcolor="rgba(46,139,87,0.18)",
        hovertemplate="<b>仓位</b> %{y:.1%}<extra></extra>",
    ), row=2, col=1)

    fig1.update_yaxes(title_text="净值 (归一)", row=1, col=1)
    fig1.update_yaxes(title_text="仓位", tickformat=".0%",
                      range=[0, 1.02], row=2, col=1)
    fig1.update_layout(
        height=_TAG2_H, margin=_MARGIN_PLT, autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode="x unified",
        legend=dict(orientation="h", y=1.06, x=0),
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

    # view 5: 年收益表
    yt = _resample_table(daily_s, daily_p, dates, "YE")
    fig5 = go.Figure(_wide_metric_table(yt.index, yt))
    fig5.update_layout(height=_TAG2_H, margin=_MARGIN_TBL, autosize=True)

    # view 6: 月收益表
    mt = _resample_table(daily_s, daily_p, dates, "ME")
    fig6 = go.Figure(_wide_metric_table(mt.index, mt))
    fig6.update_layout(height=_TAG2_H, margin=_MARGIN_TBL, autosize=True)

    # view 7: 交易收益分布
    if len(bt["trades_open_px"]) > 0:
        rets = bt["trades_close_px"] / bt["trades_open_px"] - 1.0
        fig7 = _chart(go.Histogram(x=rets, name="交易收益", nbinsx=80, marker_color="teal"))
    else:
        fig7 = _chart(go.Scatter(x=[], y=[], name="无交易"))

    return [
        ("收益曲线 + 仓位曲线", fig1),
        ("回撤曲线", fig2),
        ("年/月/周收益分布", fig3),
        ("月换手率分布", fig4),
        ("年收益表", fig5),
        ("月收益表", fig6),
        ("交易收益分布", fig7),
    ]


# ============================================================================
# TAG 3: 每天持仓 / 交易记录 (table 切换)
# ============================================================================

def fig_tag3(bt, codes, dates_all) -> list[tuple[str, go.Figure]]:
    # view 1: 每天持仓 — 最近优先, 段内权重降序 (cpp 已排好), PIT 名直读
    off = bt["holdings_offsets"]
    hcodes = bt["holdings_codes"]
    hweights = bt["holdings_weights"]
    hnames = bt["holdings_names"]
    rows = []
    n_d_bt = len(bt["dates"])
    for i in range(n_d_bt - 1, -1, -1):
        lo, hi = int(off[i]), int(off[i + 1])
        if hi == lo:
            continue
        date_str = bt["dates"][i].strftime("%Y-%m-%d")
        for k in range(lo, hi):
            a = int(hcodes[k])
            rows.append((date_str,
                         f"{hnames[k]} ({codes[a]})",
                         float(hweights[k])))
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

    # view 2: 交易记录 (PIT 名取平仓当日, namechange 后名字会动态切换)
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
                f"{bt['trades_close_names'][k]} ({codes[a]})",
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
.view-container .view {
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  visibility: hidden;
}
.view-container .view.active { visibility: visible; }
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


def main():
    data = _load()
    bt = data["bt"]
    an = data["an"]
    meta = data["meta"]
    codes = data["codes"]
    dates_all = data["dates_all"]

    tag_views = [
        ("TAG 1: 策略指标 / 交易统计", fig_tag1(bt, an, meta)),
        ("TAG 2: 收益曲线 / 分布 / 表格", fig_tag2(bt, an, meta, codes)),
        ("TAG 3: 每天持仓 / 交易记录", fig_tag3(bt, codes, dates_all)),
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
            cls = "view active" if view_i == 0 else "view"
            div = fig.to_html(full_html=False, include_plotlyjs=False,
                              div_id=f"tag{tag_i+1}_view{view_i+1}",
                              default_width="100%",
                              default_height="100%",
                              config={"responsive": True})
            parts.append(f'<div class="{cls}">{div}</div>')
        parts.append('</div>')
        parts.append('</div>')
    parts.append(REPORT_FOOT)
    out_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"report -> {out_path}")
    webbrowser.open(out_path.as_uri())


if __name__ == "__main__":
    main()
