"""读 output/ 下 cpp 写出的 npy/json, 用 plotly 出一个多策略 HTML 报告.

用法:
    python -m py.report          # 生成 output/report.html 并打开浏览器

**本模块不做任何指标计算.** 指标 / 交易统计 / 年月表 / 分层 CAGR / 因子汇总 /
跨策略聚合全部由 cpp 的 report/{metrics,aggregate} 算完落 json, 这里只做
"读 → 组 figure → 拼 HTML". 要加指标请改 cpp, 不要在这里补 numpy — 否则同一个
指标会有两份实现两个口径.

布局: 顶部全局策略选择器 + 原来的 4 个 TAG 板块 (view-buttons 切子视图).
    view 分两类:
      [聚] 跨策略聚合 — 页面生成时一次渲染, 与选择器无关
      [单] 跟随选择器 — 每策略一份 payload 存 <script type="application/json">,
           首次可见时 newPlot, 之后 Plotly.react (惰性渲染, 不同策略互不预热)

    TAG 1: 跨策略指标[聚] | 策略指标[单] | 交易统计[聚] | 策略配置[聚]
    TAG 2: 净值叠加[聚] | 回测曲线[单] | 年度对比[聚] | 年收益表[单]
           | 月收益表[单] | 交易收益分布[单]
    TAG 3: 今日下单台[聚] | 今日持仓[单] | 交易记录[单] | 持仓重叠度[聚]
           | 策略相关性[聚]
    TAG 4: 累积IC对比[聚] | 因子表格[聚] | 累积因子IC[单] | 分层年化[单]
           | 分层累计[单] | 因子相关性[单] | 因子分布(逐年)[单]

数据来源 (全部 cpp 写出):
    output/meta.json                       轴 / per-a 简称与行业 / 策略配置
    output/aggregate/{*.npy, report.json}  跨策略聚合
    output/strategy/<name>/backtest/{*.npy, labels.json, report.json}
    output/strategy/<name>/analysis/{*.npy, report.json}
"""

from __future__ import annotations

import html
import json
import webbrowser
from pathlib import Path

import numpy as np
import plotly.graph_objects as go
import plotly.io as pio
from plotly.subplots import make_subplots

# ============================================================================
# 全局元数据 — 一切 per-a 标签 (简称 / 行业) 都按 a 索引查这里, py 侧不再读
#   parquet, 也不再读 output/tensor/ (那份受 TENSOR_DUMP_ENABLE 门控, 是隐藏耦合)
# ============================================================================

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "output"
AG_DIR = OUT_DIR / "aggregate"

META = json.loads((OUT_DIR / "meta.json").read_text(encoding="utf-8"))
STRATS = META["strategies"]
N_S = len(STRATS)
assert N_S > 0, "meta.json strategies 为空"
S_NAMES = [s["name"] for s in STRATS]

DATES_ALL = np.array(META["dates"])
CODES = np.array(META["codes"])
NAMES_A = np.array(META["names"], dtype=object)
INDUS_A = np.array(META["industries"], dtype=object)
assert len(NAMES_A) == len(CODES) and len(INDUS_A) == len(
    CODES
), "meta.json names/industries 与 codes 长度不一致"

COMBO = "等权组合"

# 全一级行业集合 (从 INDUS_A 的 "一级 -- 二级" 串里取前缀派生, 用于算"过滤行业")
_INDUS1_ALL = sorted({str(x).split(" -- ")[0] for x in INDUS_A})

# 各 TAG 外层 .view-container 高度 (px); TAG 内所有子图共用, 切换对齐
_TAG_H = (200, 480, 620, 480)
_TAG1_H, _TAG2_H, _TAG3_H, _TAG4_H = _TAG_H

_MARGIN_PLT = dict(t=30, b=30, l=50, r=20)

_FONT_CELL = 12
_STRAT_COLOR = "crimson"
_COMBO_COLOR = "black"


def _fmt_ymd(v) -> str:
    s = str(v).replace("-", "")
    assert len(s) == 8, f"bad yyyymmdd: {v!r}"
    return f"{s[:4]}-{s[4:6]}-{s[6:8]}"


def _code6(code) -> str:
    return str(code).split(".")[0]


def _dstr(d_idx) -> list[str]:
    """axes D 索引数组 → "YYYY-MM-DD" 列表 (plotly x 轴直吃日期字符串)."""
    return [_fmt_ymd(DATES_ALL[int(i)]) for i in d_idx]


def _stock_label(a: int) -> str:
    return f"{NAMES_A[a]} ({_code6(CODES[a])})"


def _html_table(
    header: list, cols: list, *, compact: bool = False, cell_bg=None
) -> str:
    """原生 HTML 表 (固定行高 + 外层 overflow 滚动). 不要用 Plotly Table 撑开高度.

    cell_bg: 与 cols 同形状的 style 字符串矩阵 (空串 = 无背景), 用于收益热力着色.
    """
    n = len(cols[0]) if cols else 0
    for c in cols:
        assert len(c) == n, "html table columns length mismatch"
    if cell_bg is not None:
        assert len(cell_bg) == len(cols), "cell_bg 列数与 cols 不一致"
        for c in cell_bg:
            assert len(c) == n, "cell_bg 行数与 cols 不一致"
    th = "".join(f"<th>{html.escape(str(h))}</th>" for h in header)
    trs = []
    for i in range(n):
        tds = []
        for ci, col in enumerate(cols):
            txt = html.escape(str(col[i]))
            style = ""
            if cell_bg and cell_bg[ci][i]:
                style = f' style="{cell_bg[ci][i]}"'
            tds.append(f"<td{style}>{txt}</td>")
        trs.append(f"<tr>{''.join(tds)}</tr>")
    cls = "tbl tbl-compact" if compact else "tbl"
    return (
        f'<table class="{cls}"><thead><tr>{th}</tr></thead>'
        f'<tbody>{"".join(trs)}</tbody></table>'
    )


def _ret_bg(v, thr: float) -> str:
    """收益值 → 背景色 (红赚绿亏, |v|≥thr 饱和, 中间线性渐变).

    返回 "" 表示不着色 (None / "—" / 0 / 非数). 背景用浅色 hsl, 字保持黑.
    """
    if v is None or v == "—":
        return ""
    if isinstance(v, str):
        return ""
    fv = float(v)
    if not np.isfinite(fv) or fv == 0.0:
        return ""
    sat = min(abs(fv) / thr, 1.0)
    hue = 0 if fv > 0 else 120  # 0=红(赚), 120=绿(亏)
    return f"background:hsl({hue},{sat*70:.0f}%,{92-sat*24:.0f}%)"


# backtest/ 下逐日或逐笔的序列, 除 hover / 曲线 / 明细表外不参与任何计算
#   pool 等权净值只在 cpp 内部供 rel_stats/"超额" 用, 不落盘不展示.
_BT_NPY = (
    "strategy_nav",
    "strategy_dd",
    "position_pct",
    "holdings_offsets",
    "holdings_codes",
    "holdings_weights",
    "watch_offsets",
    "watch_codes",
    "watch_scores",
    "watch_rank_chg",
    "watch_hold_w",
    "watch_hold_days",
    "watch_bought",
    "trades_inst",
    "trades_open_d",
    "trades_close_d",
    "trades_open_px",
    "trades_close_px",
    "fills_d",
    "fills_a",
    "fills_side",
    "fills_px",
    "fills_weight",
    "fills_pnl",
)
# analysis/ 下已是"可直接绘"的累积形态 (cum_nav / nan_cumsum 在 cpp 侧做完)
_AN_NPY = (
    "factor_corr",
    "factor_ic_cum",
    "score_ic_cum",
    "quantile_nav",
    "pool_nav_cum",
    "factor_dist_years",
    "factor_dist_grid",
    "factor_dist_density",
)


def _load_strat(i: int) -> dict:
    root = OUT_DIR / "strategy" / S_NAMES[i]
    bt_dir, an_dir = root / "backtest", root / "analysis"
    d_idx = np.load(bt_dir / "dates.npy")
    assert np.array_equal(
        np.load(an_dir / "dates.npy"), d_idx
    ), f"{S_NAMES[i]}: analysis/backtest dates 不一致"
    lab = json.loads((bt_dir / "labels.json").read_text(encoding="utf-8"))

    s: dict = {
        "name": S_NAMES[i],
        "spec": STRATS[i],
        "d_idx": d_idx,
        "x": _dstr(d_idx),
        "bt": json.loads((bt_dir / "report.json").read_text(encoding="utf-8")),
        "an": json.loads((an_dir / "report.json").read_text(encoding="utf-8")),
    }
    for k in _BT_NPY:
        s[k] = np.load(bt_dir / f"{k}.npy")
    for k in _AN_NPY:
        s[k] = np.load(an_dir / f"{k}.npy")
    for k in ("trades_close_names", "watch_names", "fills_names"):
        s[k] = np.asarray(lab[k], dtype=object)

    n_d = len(d_idx)
    assert len(s["strategy_nav"]) == n_d, f"{s['name']}: nav 与 dates 不同长"
    assert len(s["watch_offsets"]) == n_d + 1, f"{s['name']}: watch CSR 长度错"
    assert len(s["watch_names"]) == len(
        s["watch_codes"]
    ), f"{s['name']}: watch_names 与 watch_codes 不同长"
    assert len(s["fills_names"]) == len(
        s["fills_d"]
    ), f"{s['name']}: fills_names 与 fills_d 不同长"
    assert len(s["trades_close_names"]) == len(
        s["trades_inst"]
    ), f"{s['name']}: trades_close_names 与 trades_inst 不同长"
    return s


def _load_ag() -> dict:
    rep = json.loads((AG_DIR / "report.json").read_text(encoding="utf-8"))
    assert rep["strategies"] == S_NAMES, "aggregate 与 meta 的策略顺序不一致"
    nav = np.load(AG_DIR / "strategy_nav.npy")
    assert nav.shape[0] == N_S, "aggregate/strategy_nav 首维应为策略数"
    return {
        "rep": rep,
        "x": _dstr(np.load(AG_DIR / "dates.npy")),
        "strategy_nav": nav,
        "combo_nav": np.load(AG_DIR / "combo_nav.npy"),
        "overlap_count": np.load(AG_DIR / "overlap_count.npy"),
        "corr": np.load(AG_DIR / "corr.npy"),
    }


# ============================================================================
# 数值格式化 — 唯一按"指标名"分派的地方 (cpp 只给数, 单位语义在这里)
# ============================================================================

_PCT_METRICS = {
    "年化",
    "波动率",
    "最大回撤",
    "Alpha",
    "跟踪误差",
    "平均交易收益",
    "正收益平均",
    "负收益平均",
    "交易赢率",
    "持仓停牌股票比例",
    "月赢率",
    "周赢率",
    "日赢率",
    "调仓指令可执行比例",
    "指数跟踪误差",
    "平均持仓仓位",
    "年换手率",
    "策略收益",
    "基准收益",
    "策略最大回撤",
    "基准最大回撤",
    "换手率",
}
_INT_METRICS = {"天数", "创新高最长天数", "命中策略数"}


def _fmt_v(v, key: str = "") -> str:
    if v is None or v == "—":
        return "—"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, np.integer)) and key not in _PCT_METRICS:
        return str(int(v))
    v = float(v)
    if not np.isfinite(v):
        return "nan"
    if key in _INT_METRICS:
        return str(int(round(v)))
    if key in _PCT_METRICS:
        return f"{v*100:.2f}%"
    return f"{v:.4f}"


def _metric_table(
    row_labels: list[str], rows: list[dict], metrics: list[str], corner: str = ""
) -> str:
    """行 = 实体 (策略 / 期次), 列 = 指标; 缺指标填 "—"."""
    cols = [row_labels]
    for m in metrics:
        cols.append([_fmt_v(r.get(m), m) for r in rows])
    return _html_table([corner] + metrics, cols)


def _col_table(tbl: dict, header: list[str]) -> str:
    """cpp 落的列式表 ({"列名": [...]}) → HTML 表, 列序由 header 指定."""
    for h in header:
        assert h in tbl, f"列式表缺列 {h}"
    n = len(tbl[header[0]])
    cols = [[_fmt_v(tbl[h][i], h) for i in range(n)] for h in header]
    return _html_table(header, cols)


# ============================================================================
# TAG 1: 跨策略指标[聚] / 策略指标[单] / 交易统计[聚] / 策略配置[聚]
# ============================================================================

_AG_METRICS = ["天数", "年化", "波动率", "夏普", "最大回撤", "创新高最长天数"]
_AG_REL = ["信息比率", "Beta", "Alpha", "跟踪误差"]

# 交易统计: cpp report.json trade_stats 的 16 项, 顺序即列序
_TRADE_METRICS = [
    "换股次数",
    "平均持有天数",
    "平均持仓股票数",
    "平均持仓仓位",
    "调仓指令可执行比例",
    "持仓停牌股票比例",
    "平均交易收益",
    "正收益平均",
    "负收益平均",
    "年换手率",
    "交易赢率",
    "日赢率",
    "周赢率",
    "月赢率",
    "指数跟踪误差",
    "创新高最长天数",
]


def view_ag_metrics(ag) -> str:
    """跨策略指标: 行 = 各策略 + 等权组合; 相对列 = 各自相对自己 pool 的指标."""
    met = ag["rep"]["metrics"]
    labels = S_NAMES + [COMBO]
    rows = [met[k] for k in labels]
    cols = _AG_METRICS + _AG_REL
    return _metric_table(labels, rows, cols, corner="策略 (基准=自身 pool)")


def view_ag_trade_stats(strats) -> str:
    """交易统计: 原来是单策略 2×9 宽表, 多策略下改成 行=策略 更好比."""
    rows = []
    for s in strats:
        r = dict(s["bt"]["trade_stats"])
        tm = s["spec"]["timing"]
        r["CPU秒"] = tm["backtest_seconds"] + tm["analysis_seconds"]
        rows.append(r)
    return _metric_table(S_NAMES, rows, _TRADE_METRICS + ["CPU秒"], corner="策略")


def view_ag_config() -> str:
    """策略配置对比 — 回答"这几个策略到底差在哪", 全部取自 meta.json.

    板块用汉语 (与交易所列口径一致); 行业列显示"过滤行业"(全一级 − 白名单),
    白名单为空 = 不限 = "全部". 过滤集比白名单更短, 一眼看出排了哪些.
    """
    factor_names = META["factor_names"]

    def _wl(v: list, all_label: str) -> str:
        return all_label if not v else " / ".join(str(x) for x in v)

    def _excl_industry(wl: list) -> str:
        if not wl:
            return "全部"
        ws = set(wl)
        excl = [i for i in _INDUS1_ALL if i not in ws]
        return " / ".join(excl) if excl else "—"

    cols_head = [
        "策略",
        "起始日",
        "持仓数",
        "退出倍数",
        "母集排序",
        "母集大小",
        "两融",
        "交易所",
        "板块",
        "过滤行业",
    ]
    base = [[], [], [], [], [], [], [], [], [], []]
    for s in STRATS:
        p = s["pool"]
        vals = [
            s["name"],
            _fmt_ymd(s["start_date"]),
            str(s["hold_n"]),
            f'{s["exit_ratio"]:.1f}',
            f'{p["rank_key"]} {"升" if p["rank_asc"] else "降"}',
            str(p["universe_size"]),
            p["margin_policy"],
            _wl(p["exchange"], "全部"),
            _wl(p["list_sector"], "全部"),
            _excl_industry(p["industry_l1"]),
        ]
        for c, v in zip(base, vals):
            c.append(v)
    # 因子权重: 每因子一列, 未配置留空 ⇒ 一眼看出各策略用了哪些因子
    for f in factor_names:
        base.append([_fmt_v(s["weights"].get(f)) for s in STRATS])
    # filters 放最后一列 (最长)
    base.append([" / ".join(s["filters"]) for s in STRATS])
    return _html_table(cols_head + factor_names + ["过滤器"], base, compact=True)


# ============================================================================
# TAG 2: 净值叠加[聚] / 回测曲线[单] / 年度对比[聚] / 年月表[单] / 分布[单]
# ============================================================================

_PERIOD_COLS = [
    "期次",
    "策略收益",
    "基准收益",
    "策略最大回撤",
    "基准最大回撤",
    "跟踪误差",
    "信息比率",
    "波动率",
    "夏普比率",
]


def _plot(*traces, height: int, **kw) -> go.Figure:
    fig = go.Figure()
    for tr in traces:
        fig.add_trace(tr)
    fig.update_layout(
        height=height,
        margin=_MARGIN_PLT,
        autosize=True,
        font=dict(size=_FONT_CELL),
        **kw,
    )
    return fig


def _end_label(x, y, name: str, color: str) -> go.Scatter:
    return go.Scatter(
        x=[x[-1]],
        y=[float(y[-1])],
        mode="text",
        text=[name],
        textposition="middle right",
        textfont=dict(size=9, color=color),
        legendgroup=name,
        showlegend=False,
        hoverinfo="skip",
        cliponaxis=False,
    )


def view_ag_nav(ag) -> go.Figure:
    """各策略净值叠加 + 等权组合.

    净值已由 cpp 归一到公共窗口首日 = 1.0 ⇒ 起点天然对齐, 前端零处理.
    """
    x = ag["x"]
    colors = px_colors(N_S)
    fig = go.Figure()
    for i, nm in enumerate(S_NAMES):
        fig.add_trace(
            go.Scatter(
                x=x,
                y=ag["strategy_nav"][i],
                mode="lines",
                name=nm,
                legendgroup=nm,
                showlegend=False,
                hoverinfo="skip",
                line=dict(color=colors[i], width=1.5),
            )
        )
        fig.add_trace(_end_label(x, ag["strategy_nav"][i], nm, colors[i]))
    fig.add_trace(
        go.Scatter(
            x=x,
            y=ag["combo_nav"],
            mode="lines",
            name=COMBO,
            legendgroup=COMBO,
            showlegend=False,
            hoverinfo="skip",
            line=dict(color=_COMBO_COLOR, width=2.5),
        )
    )
    fig.add_trace(_end_label(x, ag["combo_nav"], COMBO, _COMBO_COLOR))
    fig.update_yaxes(title_text="累计净值 (公共窗口归一)", fixedrange=True)
    fig.update_layout(
        height=_TAG2_H,
        margin=dict(t=16, b=30, l=50, r=150),
        autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode=False,
        dragmode="zoom",
        showlegend=False,
    )
    return fig


def view_curve(s) -> go.Figure:
    """回测曲线 — 净值 / 回撤 / 仓位 3 子图, shared x.

    净值 trace 关掉 plotly 原生 hover (hoverinfo=none), 由 HOVER_JS 用
    <script> 里那份结构化 blob 现场自绘面板 — 逐日 20 行的 HTML 不再预渲染进
    图里 (那是原来 10MB+ 的唯一来源).
    """
    x = s["x"]
    nav_s = s["strategy_nav"]
    fig = make_subplots(
        rows=3, cols=1, shared_xaxes=True, row_heights=[3, 1, 1], vertical_spacing=0.04
    )

    fig.add_trace(
        go.Scatter(
            x=x,
            y=nav_s / nav_s[0],
            mode="lines",
            name="策略",
            legendgroup="策略",
            line=dict(color=_STRAT_COLOR),
            hoverinfo="none",
        ),
        row=1,
        col=1,
    )
    # 正式调仓日 marker (fills: 因子 pop/补槽 + 退市强平; 不含再平衡加仓)
    d0 = int(s["d_idx"][0])
    i_fill = sorted({int(v) - d0 for v in s["fills_d"]})
    if i_fill:
        fig.add_trace(
            go.Scatter(
                x=[x[i] for i in i_fill],
                y=[float(nav_s[i] / nav_s[0]) for i in i_fill],
                mode="markers",
                name="调仓",
                legendgroup="策略",
                showlegend=False,
                marker=dict(symbol="triangle-down", color="black", size=8),
                hoverinfo="skip",
            ),
            row=1,
            col=1,
        )

    fig.add_trace(
        go.Scatter(
            x=x,
            y=s["strategy_dd"],
            mode="lines",
            name="策略",
            legendgroup="策略",
            showlegend=False,
            line=dict(color=_STRAT_COLOR),
            fill="tozeroy",
            hoverinfo="skip",
        ),
        row=2,
        col=1,
    )

    fig.add_trace(
        go.Scatter(
            x=x,
            y=s["position_pct"],
            mode="lines",
            name="策略",
            legendgroup="策略",
            showlegend=False,
            line=dict(color=_STRAT_COLOR),
            fill="tozeroy",
            fillcolor="rgba(220,20,60,0.18)",
            hoverinfo="skip",
        ),
        row=3,
        col=1,
    )

    # zoom 只支持横向: 三个子图 y 轴全部 fixedrange (纵向由 RENORM_JS 在横向
    # zoom 后对净值图重算 full range).
    fig.update_yaxes(title_text="净值 (归一)", fixedrange=True, row=1, col=1)
    fig.update_yaxes(title_text="回撤", tickformat=".0%", fixedrange=True, row=2, col=1)
    fig.update_yaxes(
        title_text="仓位",
        tickformat=".0%",
        range=[0, 1.02],
        fixedrange=True,
        row=3,
        col=1,
    )
    fig.update_layout(
        height=_TAG2_H,
        margin=_MARGIN_PLT,
        autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode="closest",
        hoverdistance=10,
        legend=dict(orientation="h", y=1.06, x=0),
    )
    return fig


def view_ag_annual(strats) -> str:
    """跨策略年度收益: 行 = 年, 列 = 各策略.

    各策略回测窗口可能不同起点, 缺的年份留空. 收益格按 ±50% 饱和着色 (红赚绿亏).
    """
    per: list[dict[str, float]] = []
    years: list[str] = []
    for s in strats:
        t = s["bt"]["annual"]
        per.append(dict(zip(t["期次"], t["策略收益"])))
        for y in t["期次"]:
            if y not in years:
                years.append(y)
    years.sort()
    cols = [years]
    bg = [[""] * len(years)]  # 年份列不着色
    for m in per:
        cols.append([_fmt_v(m.get(y), "策略收益") for y in years])
        bg.append([_ret_bg(m.get(y), 0.5) for y in years])
    return _html_table(["年份"] + S_NAMES, cols, cell_bg=bg)


# 年收益 ±50% 饱和, 月收益 ±10% 饱和 (年波动幅度远大于月)
_RET_THR = {"annual": 0.5, "monthly": 0.1}


def view_period(s, key: str) -> str:
    """年/月收益表 — 策略收益 / 基准收益 两列按对应阈值热力着色."""
    tbl = s["bt"][key]
    thr = _RET_THR[key]
    n = len(tbl[_PERIOD_COLS[0]])
    cols, bg = [], []
    for h in _PERIOD_COLS:
        vals = tbl[h]
        cols.append([_fmt_v(v, h) for v in vals])
        if h in ("策略收益", "基准收益"):
            bg.append([_ret_bg(v, thr) for v in vals])
        else:
            bg.append([""] * n)
    return _html_table(_PERIOD_COLS, cols, cell_bg=bg)


def view_trade_dist(s) -> go.Figure:
    op, cp = s["trades_open_px"], s["trades_close_px"]
    if len(op) == 0:
        return _plot(go.Scatter(x=[], y=[], name="无交易"), height=_TAG2_H)
    return _plot(
        go.Histogram(x=cp / op - 1.0, name="交易收益", nbinsx=80, marker_color="teal"),
        height=_TAG2_H,
        xaxis=dict(tickformat=".0%"),
    )


# ============================================================================
# 回测曲线 hover 载荷 — cpp 已把 watch 的持仓状态算成 3 列 npy, 这里只做紧凑编码
# ============================================================================


def _arrow_bucket(chg: float, n_rank: int) -> int:
    """rank 相对 5 日均线的分档: |Δ| ≤ 0.1N 平, > 0.1N 单箭头, > 0.2N 双箭头."""
    if not np.isfinite(chg):
        return 0
    if chg > 0.2 * n_rank:
        return 2
    if chg > 0.1 * n_rank:
        return 1
    if chg < -0.2 * n_rank:
        return -2
    if chg < -0.1 * n_rank:
        return -1
    return 0


def _hover_blob(s) -> dict:
    """回测曲线逐日 hover 的结构化载荷 (JS 侧现场拼面板, 见 HOVER_JS).

    紧凑编码 — JSON 的引号 / 括号在 46k 条目上就是几 MB, 所以压成字符串:
      每日一行 ("\\n" 分隔), 行内条目 "|" 分隔, 字段 "," 分隔.
      watch: 简称, 分数, 箭头档(-2..2), 仓位%, 持有天数, 当日买入(1)
             后三项为空 = 当日未持仓
      fills: 方向(B/S), 简称, code6, 成交价, 仓位%, 收益%
    """
    n = len(s["d_idx"])
    d0 = int(s["d_idx"][0])
    n_rank = int(s["spec"]["hold_n"]) * 2
    woff = s["watch_offsets"]

    rows = []
    for i in range(n):
        ent = []
        for k in range(int(woff[i]), int(woff[i + 1])):
            nm = str(s["watch_names"][k])
            assert "," not in nm and "|" not in nm, f"简称含分隔符: {nm}"
            w = float(s["watch_hold_w"][k])
            held = np.isfinite(w)
            ent.append(
                ",".join(
                    (
                        nm,
                        f'{float(s["watch_scores"][k]):.2f}',
                        str(_arrow_bucket(float(s["watch_rank_chg"][k]), n_rank)),
                        str(int(round(w * 100))) if held else "",
                        str(int(s["watch_hold_days"][k])) if held else "",
                        "1" if int(s["watch_bought"][k]) else "",
                    )
                )
            )
        rows.append("|".join(ent))

    fills: list[list[str]] = [[] for _ in range(n)]
    for k in range(len(s["fills_d"])):
        i = int(s["fills_d"][k]) - d0
        assert 0 <= i < n, "fills_d 越出回测窗口"
        w = float(s["fills_weight"][k])
        pnl = float(s["fills_pnl"][k])
        fills[i].append(
            ",".join(
                (
                    "B" if int(s["fills_side"][k]) > 0 else "S",
                    str(s["fills_names"][k]),
                    _code6(CODES[int(s["fills_a"][k])]),
                    f'{float(s["fills_px"][k]):.2f}',
                    str(int(round(w * 100))) if np.isfinite(w) else "",
                    f"{pnl*100:+.0f}" if np.isfinite(pnl) else "",
                )
            )
        )

    return {
        "pos": [int(round(float(v) * 100)) for v in s["position_pct"]],
        "watch": "\n".join(rows),
        "fills": "\n".join("|".join(f) for f in fills),
    }


# ============================================================================
# TAG 3: 今日下单台[聚] / 今日持仓[单] / 交易记录[单] / 重叠度[聚] / 相关性[聚]
# ============================================================================


def view_ag_desk(ag) -> str:
    """今日多策略下单台 — 末日各策略持仓合并, 按等权组合下的实际敞口降序.

    命中策略数 ≥ 2 的行就是重叠股: 实盘同一只票敞口翻倍, 四份独立报告看不见.
    """
    desk = ag["rep"]["desk"]
    a_col = desk["a"]
    cols = [
        [str(i + 1) for i in range(len(a_col))],
        [_stock_label(int(a)) for a in a_col],
        [str(INDUS_A[int(a)]) for a in a_col],
        [f"{w*100:.2f}%" for w in desk["combo_weight"]],
        [str(int(v)) for v in desk["n_hit"]],
    ]
    for nm in S_NAMES:
        w = desk["weights"][nm]
        cols.append([f"{v*100:.2f}%" if v > 0 else "" for v in w])
    header = ["序号", "股票", "行业分类", "组合权重", "命中策略数"] + S_NAMES
    return _html_table(header, cols, compact=True)


def view_holdings(s) -> str:
    """今日持仓 — cpp 已按权重降序; 简称 / 行业按 a 查 meta.json."""
    h = s["bt"]["holdings"]
    n = len(h["a"])
    cols = [
        [str(i + 1) for i in range(n)],
        [_stock_label(int(a)) for a in h["a"]],
        [str(INDUS_A[int(a)]) for a in h["a"]],
        [_fmt_ymd(DATES_ALL[int(d)]) for d in h["open_d"]],
        [f"{v:.2f}" for v in h["open_px"]],
        [f"{v:.2f}" for v in h["last_px"]],
        [f"{v*100:.2f}%" for v in h["weight"]],
        [f"{v*100:.2f}%" for v in h["ret"]],
    ]
    return _html_table(
        [
            "序号",
            "股票",
            "行业分类",
            "买入日期",
            "买入价格",
            "最近收盘价",
            "当前仓位",
            "累计涨幅",
        ],
        cols,
        compact=True,
    )


def view_trades(s) -> str:
    """交易记录 — 平仓日降序; 简称取平仓当日历史简称 (labels.json)."""
    order = (
        np.argsort(-s["trades_close_d"], kind="stable")
        if len(s["trades_inst"])
        else np.array([], dtype=int)
    )
    seq, stock, ind, od, cd, days, opx, cpx, pnl = ([] for _ in range(9))
    for i, k in enumerate(order, start=1):
        a = int(s["trades_inst"][k])
        d_o, d_c = int(s["trades_open_d"][k]), int(s["trades_close_d"][k])
        p_o, p_c = float(s["trades_open_px"][k]), float(s["trades_close_px"][k])
        seq.append(str(i))
        stock.append(f'{s["trades_close_names"][k]} ({_code6(CODES[a])})')
        ind.append(str(INDUS_A[a]))
        od.append(_fmt_ymd(DATES_ALL[d_o]))
        cd.append(_fmt_ymd(DATES_ALL[d_c]))
        days.append(str(d_c - d_o))
        opx.append(f"{p_o:.2f}")
        cpx.append(f"{p_c:.2f}")
        pnl.append(f"{(p_c / p_o - 1) * 100:.2f}%")
    return _html_table(
        [
            "序号",
            "股票",
            "行业分类",
            "买入日期",
            "卖出日期",
            "持仓天数",
            "买入价格",
            "卖出价格",
            "涨幅",
        ],
        [seq, stock, ind, od, cd, days, opx, cpx, pnl],
        compact=True,
    )


def view_ag_overlap(ag) -> go.Figure:
    """持仓重叠度: 每日被 ≥2 策略同时持有的股票数 (多策略实盘的真实集中度)."""
    return _plot(
        go.Scatter(
            x=ag["x"],
            y=ag["overlap_count"],
            mode="lines",
            name="重叠只数",
            line=dict(color="darkorange"),
            fill="tozeroy",
            fillcolor="rgba(255,140,0,0.18)",
            hovertemplate="%{x}<br>重叠 %{y} 只<extra></extra>",
        ),
        height=_TAG3_H,
        yaxis=dict(title_text="被 ≥2 策略同时持有的只数", rangemode="tozero"),
    )


def view_ag_corr(ag) -> go.Figure:
    """策略间日收益相关矩阵 — 分散化价值的定量依据 (相关越低组合越有意义)."""
    fig = go.Figure(
        go.Heatmap(
            z=ag["corr"],
            x=S_NAMES,
            y=S_NAMES,
            colorscale="RdBu",
            zmid=0,
            zmin=-1,
            zmax=1,
            texttemplate="%{z:.2f}",
            textfont=dict(size=11),
            colorbar=dict(title="corr"),
        )
    )
    fig.update_layout(
        height=_TAG3_H,
        margin=_MARGIN_PLT,
        autosize=True,
        font=dict(size=_FONT_CELL),
        xaxis=dict(tickangle=-30),
    )
    return fig


# ============================================================================
# TAG 4: 累积IC对比[聚] / 因子表格[聚] / 累积因子IC[单] / 分层[单] / 相关阵[单]
# ============================================================================

FACTOR_NAMES = META["factor_names"]
N_FACTOR = len(FACTOR_NAMES)
# 因子英文 → 中文名 (cpp main.cpp 写出, 与 factor_names 同序)
FACTOR_CN = dict(zip(FACTOR_NAMES, META["factor_cn_names"]))
assert len(FACTOR_CN) == N_FACTOR, "factor_cn_names 与 factor_names 长度不一致"


def _factor_label(en: str) -> str:
    """因子英文 + 中文名 (表头/表格用), 缺中文名时退化为纯英文."""
    cn = FACTOR_CN.get(en, "")
    return f"{en} {cn}" if cn else en
IC_MA_W = META["config"]["ic_ma_window"]


def view_ag_score_ic(strats) -> go.Figure:
    """各策略聚合 score 的累积 IC 叠加 — 策略间"选股信号有效性"直接对比.

    单因子的累积 IC 是 per-strategy 视图 (每策略 pool 母集不同, 同一因子的
    IC 曲线本就不同, 叠在一起 n_factor × n_strategy 条会糊).
    """
    colors = px_colors(N_S)
    fig = go.Figure()
    for i, s in enumerate(strats):
        y = s["score_ic_cum"]
        fig.add_trace(
            go.Scatter(
                x=s["x"],
                y=y,
                mode="lines",
                name=s["name"],
                legendgroup=s["name"],
                showlegend=False,
                hoverinfo="skip",
                line=dict(color=colors[i], width=1.5),
            )
        )
        fig.add_trace(_end_label(s["x"], y, s["name"], colors[i]))
    fig.update_yaxes(title_text="累积IC", fixedrange=True)
    fig.update_layout(
        height=_TAG4_H,
        margin=dict(t=16, b=30, l=50, r=150),
        autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode=False,
        dragmode="zoom",
        showlegend=False,
    )
    return fig


def view_ag_factors(strats) -> str:
    """因子表格聚合: 行 = 因子, 每策略贡献 (平均IC, 本策略权重) 两列.

    权重留空 = 该策略没配这个因子 ⇒ "谁在用哪个因子、用得好不好" 一屏看完.
    """
    per = []
    for s in strats:
        t = s["an"]["factors"]
        assert t["因子"] == FACTOR_NAMES, f'{s["name"]}: 因子轴与 meta 不一致'
        per.append(t)
    cols = [[_factor_label(nm) for nm in FACTOR_NAMES]]
    header = ["因子"]
    for s, t in zip(strats, per):
        cols.append([_fmt_v(v) for v in t["平均IC"]])
        cols.append([_fmt_v(v) for v in t["权重"]])
        header += [f'{s["name"]}·IC', f'{s["name"]}·权重']
    return _html_table(header, cols, compact=True)


def _used_factors(s) -> list[tuple[int, str, float]]:
    """策略实际用到的因子 (weights w≠0): 返回 [(FACTOR_NAMES 下标, 名, w)], 按 FACTOR_NAMES 顺序."""
    w = s["spec"]["weights"]
    return [(f, nm, w[nm]) for f, nm in enumerate(FACTOR_NAMES)
            if w.get(nm, 0.0) != 0.0]


def view_factor_ic(s) -> go.Figure:
    """单因子累积 IC + 聚合 score 累积 IC (cpp 已算好累积形态).

    只画策略用到的因子 (weights w≠0); 按策略 weights 符号翻转 IC, 使所有曲线呈
    "与策略收益正相关"的向上形态 (展示因子在策略方向上的贡献).
    """
    used = _used_factors(s)
    colors = px_colors(len(used))
    fig = go.Figure()
    for i, (f, nm, wf) in enumerate(used):
        cn = FACTOR_CN.get(nm, nm)   # 图里 label 用中文
        y = s["factor_ic_cum"][f]
        if wf < 0.0:
            y = -y
        fig.add_trace(
            go.Scatter(
                x=s["x"],
                y=y,
                mode="lines",
                name=cn,
                legendgroup=cn,
                showlegend=False,
                hoverinfo="skip",
                line=dict(color=colors[i]),
            )
        )
        fig.add_trace(_end_label(s["x"], y, cn, colors[i]))
    y = s["score_ic_cum"]
    fig.add_trace(
        go.Scatter(
            x=s["x"],
            y=y,
            mode="lines",
            name="聚合",
            legendgroup="聚合",
            showlegend=False,
            hoverinfo="skip",
            line=dict(color=_STRAT_COLOR, width=2),
        )
    )
    fig.add_trace(_end_label(s["x"], y, "聚合", _STRAT_COLOR))
    fig.update_yaxes(title_text="累积IC", fixedrange=True)
    fig.update_layout(
        height=_TAG4_H,
        margin=dict(t=16, b=30, l=50, r=72),
        autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode=False,
        dragmode="zoom",
        showlegend=False,
    )
    return fig


def view_factor_table(s) -> str:
    """因子明细 — 只列策略用到的因子 (w≠0); IC 类列按策略权重符号翻转,
    与累积因子IC图同口径 (呈"对策略贡献为正"形态). 权重列保留原值 (含符号),
    换手率不翻转 (本就是正速率)."""
    ft = s["an"]["factors"]
    used = _used_factors(s)
    header = ["因子", "当期IC", "IC均值", "平均IC", "IR", "换手率", "权重"]
    cols = []
    for h in header:
        if h == "因子":
            cols.append([_factor_label(nm) for _, nm, _ in used])
        elif h == "权重":
            cols.append([_fmt_v(wf, "权重") for _, _, wf in used])
        elif h == "换手率":
            cols.append([_fmt_v(ft[h][f], h) for f, _, _ in used])
        else:  # IC 类列: 按策略权重符号翻转
            cols.append([_fmt_v(ft[h][f] * (-1.0 if wf < 0 else 1.0), h)
                         for f, _, wf in used])
    return _html_table(header, cols)


def view_quantile_bar(s) -> go.Figure:
    """分层年化 — 与分层累计图同一条净值的全程 CAGR (cpp 侧算)."""
    q = s["an"]["quantile"]
    n = len(q["分层"]) - 1  # 末项是 pool
    return _plot(
        go.Bar(
            x=q["分层"],
            y=q["年化"],
            marker_color=["steelblue"] * n + ["darkred"],
            name="年化",
        ),
        height=_TAG4_H,
        yaxis=dict(tickformat=".0%"),
    )


def view_quantile_nav(s) -> go.Figure:
    qn = s["quantile_nav"]
    Q = qn.shape[0]
    colors = px_colors(Q + 1)
    fig = go.Figure()
    for q in range(Q):
        nm = f"Q{q+1}"
        fig.add_trace(
            go.Scatter(
                x=s["x"],
                y=qn[q],
                mode="lines",
                name=nm,
                legendgroup=nm,
                showlegend=False,
                hoverinfo="skip",
                line=dict(color=colors[q]),
            )
        )
        fig.add_trace(_end_label(s["x"], qn[q], nm, colors[q]))
    fig.add_trace(
        go.Scatter(
            x=s["x"],
            y=s["pool_nav_cum"],
            mode="lines",
            name="pool",
            legendgroup="pool",
            showlegend=False,
            hoverinfo="skip",
            line=dict(color="black", dash="dash"),
        )
    )
    fig.add_trace(_end_label(s["x"], s["pool_nav_cum"], "pool", "black"))
    fig.update_yaxes(title_text="累计净值", fixedrange=True)
    fig.update_layout(
        height=_TAG4_H,
        margin=dict(t=16, b=30, l=50, r=64),
        autosize=True,
        font=dict(size=_FONT_CELL),
        hovermode=False,
        dragmode="zoom",
        showlegend=False,
    )
    return fig


def view_factor_corr(s) -> go.Figure:
    fig = go.Figure(
        go.Heatmap(
            z=s["factor_corr"],
            x=FACTOR_NAMES,
            y=FACTOR_NAMES,
            colorscale="RdBu",
            zmid=0,
            colorbar=dict(title="corr"),
        )
    )
    fig.update_layout(
        height=_TAG4_H,
        margin=_MARGIN_PLT,
        autosize=True,
        font=dict(size=_FONT_CELL),
        xaxis=dict(tickangle=-45),
    )
    return fig


def px_colors(n: int) -> list[str]:
    """生成 n 个区分色 (HSL 均分)."""
    return [f"hsl({int(i * 360 / max(n, 1))},65%,50%)" for i in range(n)]


def _hsla(hsl: str, alpha: float) -> str:
    """"hsl(h,s%,l%)" -> "hsla(h,s%,l%,alpha)" (px_colors 上叠加透明度)."""
    inner = hsl[hsl.index("(") + 1 : hsl.index(")")]
    return f"hsla({inner},{alpha:.2f})"


_DIST_MIN_ALPHA = 0.12  # 最早年份的透明度下限 (年份越久越透明, 最新年份 alpha=1)


def view_factor_dist(s) -> go.Figure:
    """逐年因子分布 (KDE 曲线) — 只画策略用到的因子 (weights w≠0); 一因子一色,
    年份越久透明度越高 (最新年份实色, 最早年份接近透明), 用于看分布形态和逐年漂移.

    样本 = 该年内每日 pool 成员的截面因子值 (Factor ∈[0,1] pct_rank); cpp 侧
    report::gaussian_kde 已算好曲线 (Scott's rule 带宽), 这里只画不重算 (不用直方图).
    """
    used = _used_factors(s)
    years = [int(y) for y in s["factor_dist_years"]]
    grid = s["factor_dist_grid"]
    dens = s["factor_dist_density"]  # [n_factor, n_year, n_grid]
    n_year = len(years)
    colors = px_colors(len(used))
    fig = go.Figure()
    for i, (f, nm, _wf) in enumerate(used):
        cn = FACTOR_CN.get(nm, nm)
        for yi, yr in enumerate(years):
            frac = yi / max(n_year - 1, 1)  # 0 = 最早年, 1 = 最新年
            alpha = _DIST_MIN_ALPHA + (1.0 - _DIST_MIN_ALPHA) * frac
            fig.add_trace(
                go.Scatter(
                    x=grid,
                    y=dens[f, yi],
                    mode="lines",
                    name=f"{cn} {yr}",
                    legendgroup=nm,
                    showlegend=(yi == n_year - 1),
                    line=dict(color=_hsla(colors[i], alpha)),
                    hovertemplate=(f"{cn} {yr}<br>x=%{{x:.2f}} "
                                   f"密度=%{{y:.3f}}<extra></extra>"),
                )
            )
    fig.update_xaxes(title_text="因子值 (截面 pct_rank)", range=[0, 1],
                     fixedrange=True)
    fig.update_yaxes(title_text="密度", fixedrange=True)
    fig.update_layout(
        height=_TAG4_H,
        margin=dict(t=16, b=40, l=50, r=20),
        autosize=True,
        font=dict(size=_FONT_CELL),
        legend=dict(orientation="h", y=1.1, x=0),
        hovermode="closest",
    )
    return fig


# ============================================================================
# 报告组装
# ============================================================================

REPORT_HEAD = """<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>多策略回测 / 因子分析报告</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei", sans-serif;
       margin: 0; padding: 12px 24px; background: #fafafa; }
h1 { font-size: 20px; margin: 0 0 4px; }
h2 { font-size: 15px; margin: 0 0 10px; color: #444; }
.run-info { font-size: 12px; color: #888; margin-bottom: 12px; }
.strat-bar { position: sticky; top: 0; z-index: 20; display: flex;
             flex-wrap: wrap; align-items: center; gap: 8px;
             padding: 10px 0; margin-bottom: 4px; background: #fafafa;
             border-bottom: 1px solid #e4e4e4; }
.strat-bar .lbl { font-size: 13px; color: #666; }
.strat-btn { padding: 5px 14px; border: 1px solid #d0b0b0; border-radius: 14px;
             background: #fff; cursor: pointer; font-size: 13px;
             white-space: nowrap; }
.strat-btn.active { background: crimson; color: #fff; border-color: crimson; }
.strat-btn:hover:not(.active) { background: #fbeaea; }
.tag-section { margin-top: 32px; padding: 16px; background: #fff;
               border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,.08);
               box-sizing: border-box; }
.view-buttons { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 14px; }
.view-btn { padding: 6px 14px; border: 1px solid #ccc; border-radius: 4px;
            background: #f5f5f5; cursor: pointer; font-size: 14px; white-space: nowrap; }
.view-btn.active { background: steelblue; color: #fff; border-color: steelblue; }
.view-btn:hover:not(.active) { background: #e0e8f0; }
.view-btn.per { border-color: crimson; }
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
/* 回测曲线 hover 面板 (HOVER_JS 自绘, 替代 plotly 原生 hoverlabel) */
.hover-panel {
  position: absolute; z-index: 30; display: none; pointer-events: none;
  background: #fff; border: 1px solid #ddd; border-radius: 3px;
  padding: 4px 6px; font-size: 9px; line-height: 1.35; color: #111;
  font-family: Consolas, "Courier New", monospace; white-space: pre;
  box-shadow: 0 2px 6px rgba(0,0,0,.12);
}
.hover-panel .up { color: green; font-weight: 900; }
.hover-panel .dn { color: red; font-weight: 900; }
.hover-panel .flat { color: #111; font-weight: 900; }
.hover-panel .pad { color: #fff; font-weight: 900; }
.hover-panel .held { color: crimson; font-weight: bold; }
</style>
</head><body>
<h1>多策略回测 / 因子分析报告</h1>
"""

REPORT_FOOT = "</body></html>\n"

# 横向 zoom 后重新对齐 y (data-renorm="1" 的那些叠加图: 净值 / 分层累计 / 累积IC):
#   - 每条线用窗口起点处自己的 15 日均线做锚: y' = y / ma[i0] - 1
#     起点高于均线 2% → 从 2% 起, 不再把所有线拧到同一点
#   - 纵向按窗口内可见数据重算 full range (仅 yaxis 'y'; TAG2 回撤/仓位不动)
#   - 双击 reset (xaxis.autorange) 恢复原始 y
# 惰性渲染的 [单] 视图每次 newPlot / react 后要重新 bind (基线数组换了一份),
#   所以 bindRenorm 挂到 window 供 APP_JS 调用, 且先清掉上一轮的 relayout 监听.
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
  window.bindRenorm = function (target) {
    var gd = typeof target === 'string'
      ? document.getElementById(target) : target;
    if (!gd) return;
    if (gd.removeAllListeners) gd.removeAllListeners('plotly_relayout');
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
})();
</script>"""

# 回测曲线 hover — cpp 出结构化 blob, 这里现场拼那 20 行面板.
#   原实现把每天的面板 HTML 预渲染进 figure (customdata), 单策略就 10MB+;
#   现在改成 <script> 里一份紧凑编码 + hover 时格式化 ⇒ 与策略数线性但基数极小.
#   箭头分档 / 全宽对齐 / 持仓着色的语义与原版一致 (见 CSS .hover-panel .*).
HOVER_JS = """<script>
(function () {
  var ARROW = {
    '2': '<span class="up">▲▲</span>',
    '1': '<span class="up">▲</span><span class="pad">▲</span>',
    '0': '<span class="flat">▬▬</span>',
    '-1': '<span class="dn">▼</span><span class="pad">▼</span>',
    '-2': '<span class="dn">▼▼</span>'
  };
  function padR(s, n, ch) { while (s.length < n) s += ch; return s; }
  function padL(s, n) { while (s.length < n) s = ' ' + s; return s; }

  window.bindHover = function (gd, blobId) {
    var el = document.getElementById(blobId);
    if (!el) return;
    var blob = JSON.parse(el.textContent);
    var watch = blob.watch.split('\\n');
    var fills = blob.fills.split('\\n');
    var pos = blob.pos;

    if (gd.removeAllListeners) {
      gd.removeAllListeners('plotly_hover');
      gd.removeAllListeners('plotly_unhover');
    }
    var panel = gd.querySelector(':scope > .hover-panel');
    if (!panel) {
      panel = document.createElement('div');
      panel.className = 'hover-panel';
      gd.appendChild(panel);
    }

    function build(i) {
      var out = '仓位 ' + pos[i] + '%';
      var raw = watch[i] || '';
      if (!raw) {
        out += '\\n(空)';
      } else {
        var rows = raw.split('|').map(function (e) { return e.split(','); });
        var nw = 4;
        rows.forEach(function (f) { if (f[0].length > nw) nw = f[0].length; });
        rows.forEach(function (f) {
          var nm = padR(f[0], nw, '\\u3000');
          var score = padL(f[1], 5);
          var body;
          if (f[3] !== '') {
            var extra = ' ' + padL(f[3], 2) + '% ' + padL(f[4], 3) + 'd';
            var head = f[5] === '1'
              ? '<b>' + nm + '</b>'
              : '<span class="held">' + nm + '</span>';
            body = head + '<span class="held">' + score + extra + '</span>';
          } else {
            body = nm + score;
          }
          out += '\\n' + ARROW[f[2]] + ' ' + body;
        });
      }
      var fl = fills[i] || '';
      if (fl) {
        var buys = [], sells = [];
        fl.split('|').forEach(function (e) {
          var f = e.split(',');
          var head = f[1] + ' (' + f[2] + ') @ ' + f[3];
          if (f[0] === 'B') buys.push(head + ' (' + f[4] + '%)');
          else sells.push(head + ' (' + f[5] + '%, ' + f[4] + '%)');
        });
        [['买入', buys], ['卖出', sells]].forEach(function (kv) {
          if (!kv[1].length) return;
          out += '\\n\\n<b>' + kv[0] + ' ×' + kv[1].length + '</b>\\n<b>'
            + kv[1].join('</b>\\n<b>') + '</b>';
        });
      }
      return out;
    }

    gd.on('plotly_hover', function (ev) {
      var p = ev.points && ev.points[0];
      if (!p || p.curveNumber !== 0) return;
      panel.innerHTML = build(p.pointIndex);
      panel.style.display = 'block';
      var r = gd.getBoundingClientRect();
      var x = ev.event.clientX - r.left + 14;
      var y = ev.event.clientY - r.top + 14;
      if (x + panel.offsetWidth > r.width) {
        x = Math.max(0, x - panel.offsetWidth - 28);
      }
      if (y + panel.offsetHeight > r.height) {
        y = Math.max(0, r.height - panel.offsetHeight);
      }
      panel.style.left = x + 'px';
      panel.style.top = y + 'px';
    });
    gd.on('plotly_unhover', function () { panel.style.display = 'none'; });
  };
})();
</script>"""

# 全局策略选择器 + [单] 视图惰性渲染.
#   [单] 视图只在"自己可见且选中策略变了"时才 newPlot / react ⇒ 4 策略 × 11 个
#   跟随视图不会在打开页面时全部预热 (那会几秒白屏).
APP_JS = """<script>
(function () {
  var PLOT_CFG = { responsive: true };
  var CUR = 0;

  function renderPer(host) {
    if (host.dataset.cur === String(CUR)) return;
    var el = document.getElementById('p_' + host.dataset.slot + '_' + CUR);
    if (!el) return;
    var pl = JSON.parse(el.textContent);
    if (host.dataset.kind === 'html') {
      host.innerHTML = pl;
      host.dataset.cur = String(CUR);
      return;
    }
    var fresh = host.dataset.cur === undefined;
    var pr = fresh
      ? Plotly.newPlot(host, pl.data, pl.layout, PLOT_CFG)
      : Plotly.react(host, pl.data, pl.layout);
    host.dataset.cur = String(CUR);
    pr.then(function () {
      if (host.dataset.renorm) window.bindRenorm(host);
      if (host.dataset.hover) window.bindHover(host, 'h_' + CUR);
    });
  }

  window.showStrat = function (i) {
    CUR = i;
    document.querySelectorAll('.strat-btn').forEach(function (b, k) {
      b.classList.toggle('active', k === i);
    });
    document.querySelectorAll('.view.active[data-per]').forEach(renderPer);
  };

  window.showView = function (tagIdx, viewIdx) {
    var section = document.querySelectorAll('.tag-section')[tagIdx];
    var views = section.querySelectorAll('.view');
    views.forEach(function (v, i) {
      v.classList.toggle('active', i === viewIdx);
    });
    section.querySelectorAll('.view-btn').forEach(function (b, i) {
      b.classList.toggle('active', i === viewIdx);
    });
    var v = views[viewIdx];
    if (v.dataset.per) renderPer(v);
    else if (v.dataset.gd) Plotly.Plots.resize(v.dataset.gd);
  };

  document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('.view[data-gd][data-renorm]').forEach(function (v) {
      window.bindRenorm(v.dataset.gd);
    });
    window.showStrat(0);
  });
})();
</script>"""


# ============================================================================
# view 描述 — per=False 页面生成时就渲染好, per=True 每策略一份 payload
# ============================================================================


def _agg(label: str, obj, *, renorm: bool = False) -> dict:
    return dict(label=label, per=False, obj=obj, renorm=renorm, hover=False)


def _per(label: str, build, *, renorm: bool = False, hover: bool = False) -> dict:
    return dict(label=label, per=True, build=build, renorm=renorm, hover=hover)


def _script_json(sid: str, payload: str) -> str:
    """把一份 JSON 文本挂成 <script type="application/json">.

    "</" 转成 "<\\/" (JSON 合法转义) — 否则正文里的 </script> 会提前收尾.
    """
    safe = payload.replace("</", "<\\/")
    return f'<script type="application/json" id="{sid}">{safe}</script>'


def _tags(ag, strats) -> list[tuple[str, list[dict]]]:
    return [
        (
            "TAG 1: 指标 / 交易统计 / 策略配置",
            [
                _agg("跨策略指标", view_ag_metrics(ag)),
                _agg("交易统计", view_ag_trade_stats(strats)),
                _agg("策略配置", view_ag_config()),
            ],
        ),
        (
            "TAG 2: 净值 / 回测曲线 / 收益表",
            [
                _agg("净值叠加", view_ag_nav(ag), renorm=True),
                _per("回测曲线", view_curve, renorm=True, hover=True),
                _agg("年度收益对比", view_ag_annual(strats)),
                _per("年收益表", lambda s: view_period(s, "annual")),
                _per("月收益表", lambda s: view_period(s, "monthly")),
                _per("交易收益分布", view_trade_dist),
            ],
        ),
        (
            "TAG 3: 下单台 / 持仓 / 交易记录 / 分散化",
            [
                _agg("今日下单台", view_ag_desk(ag)),
                _per("今日持仓", view_holdings),
                _per("交易记录", view_trades),
                _agg("持仓重叠度", view_ag_overlap(ag)),
                _agg("策略相关性", view_ag_corr(ag)),
            ],
        ),
        (
            "TAG 4: 因子 / 分层",
            [
                _agg("累积IC对比", view_ag_score_ic(strats), renorm=True),
                _agg("因子表格", view_ag_factors(strats)),
                _per("累积因子IC", view_factor_ic, renorm=True),
                _per(f"因子明细 (IC均值 {IC_MA_W} 日)", view_factor_table),
                _per("分层年化", view_quantile_bar),
                _per("分层累计", view_quantile_nav, renorm=True),
                _per("因子相关性", view_factor_corr),
                _per("因子分布(逐年)", view_factor_dist),
            ],
        ),
    ]


def _run_info() -> str:
    tm = META["timing"]
    return (
        f"数据末日 {_fmt_ymd(DATES_ALL[-1])} · "
        f"A 轴 {len(CODES)} 只 · D 轴 {len(DATES_ALL)} 日 · "
        f"因子 {N_FACTOR} 个 · 策略 {N_S} 个"
        f' · Tensor {tm["tensor_bytes"] / 1024**3:.2f} GB'
        f' · 聚合 {tm["aggregate_seconds"]:.2f} 秒'
    )


def _strat_bar() -> str:
    out = [
        '<div class="strat-bar">' '<span class="lbl">策略 (只影响标 · 的视图)</span>'
    ]
    for i, nm in enumerate(S_NAMES):
        cls = "strat-btn active" if i == 0 else "strat-btn"
        out.append(
            f'<button class="{cls}" onclick="showStrat({i})">'
            f"{html.escape(nm)}</button>"
        )
    out.append("</div>")
    return "".join(out)


def main():
    ag = _load_ag()
    strats = [_load_strat(i) for i in range(N_S)]

    parts = [REPORT_HEAD, f'<div class="run-info">{_run_info()}</div>', _strat_bar()]

    for tag_i, (tag_name, views) in enumerate(_tags(ag, strats)):
        parts.append('<div class="tag-section">')
        parts.append(f"<h2>{tag_name}</h2>")
        parts.append('<div class="view-buttons">')
        for view_i, v in enumerate(views):
            cls = "view-btn" + (" per" if v["per"] else "")
            cls += " active" if view_i == 0 else ""
            parts.append(
                f'<button class="{cls}" '
                f'onclick="showView({tag_i},{view_i})">'
                f'{v["label"]}</button>'
            )
        parts.append("</div>")
        parts.append(f'<div class="view-container" style="height:{_TAG_H[tag_i]}px">')

        payloads: list[str] = []
        for view_i, v in enumerate(views):
            slot = f"{tag_i}_{view_i}"
            attrs = ' data-renorm="1"' if v["renorm"] else ""
            attrs += ' data-hover="1"' if v["hover"] else ""
            active = " active" if view_i == 0 else ""
            if not v["per"]:
                obj = v["obj"]
                if isinstance(obj, str):
                    parts.append(f'<div class="view scroll{active}">' f"{obj}</div>")
                    continue
                gd = f"g_{slot}"
                div = obj.to_html(
                    full_html=False,
                    include_plotlyjs=False,
                    div_id=gd,
                    default_width="100%",
                    default_height="100%",
                    config={"responsive": True},
                )
                parts.append(
                    f'<div class="view{active}" data-gd="{gd}"' f"{attrs}>{div}</div>"
                )
                continue

            # [单] 视图: 宿主 div 空着, payload 逐策略挂 <script>
            objs = [v["build"](s) for s in strats]
            is_html = isinstance(objs[0], str)
            kind = "html" if is_html else "plot"
            scroll = " scroll" if is_html else ""
            parts.append(
                f'<div class="view{scroll}{active}" id="g_{slot}" '
                f'data-per="1" data-kind="{kind}" '
                f'data-slot="{slot}"{attrs}></div>'
            )
            for si, obj in enumerate(objs):
                payloads.append(
                    _script_json(
                        f"p_{slot}_{si}",
                        (
                            json.dumps(obj, ensure_ascii=False)
                            if is_html
                            else pio.to_json(obj)
                        ),
                    )
                )

        parts.append("</div>")
        parts.extend(payloads)
        parts.append("</div>")

    for si, s in enumerate(strats):
        parts.append(
            _script_json(f"h_{si}", json.dumps(_hover_blob(s), ensure_ascii=False))
        )

    parts.append(RENORM_JS)
    parts.append(HOVER_JS)
    parts.append(APP_JS)
    parts.append(REPORT_FOOT)

    out_path = OUT_DIR / "report.html"
    out_path.write_text("\n".join(parts), encoding="utf-8")
    print(f"report -> {out_path}  ({out_path.stat().st_size / 1024**2:.1f} MB)")
    webbrowser.open(out_path.as_uri())


if __name__ == "__main__":
    main()
