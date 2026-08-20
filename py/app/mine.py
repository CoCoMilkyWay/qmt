"""
因子权重挖掘 — 后处理

计算全在 cpp (src/mine/mine.cpp): 继承 mine::MINE_STRATEGY 那个策略的全部配置,
只搜 weights. 四段式:
    Phase 1  全 lattice 一遍日循环: 滑窗分层 (梳子分 S = b − 2·se) + 顶档滑窗夏普.
             252 日窗 / 21 日步; 档数 = universe_size / hold_n ⇒ 顶档就是策略
             实际持有的那 hold_n 只.
    Phase 2  三个截面分数: u1 = pctrank(梳子均值), u2 = pctrank(夏普均值),
             u3 = pctrank(−敏感性); 敏感性 = 邻域 |Δu2| 均值 / 纯噪声期望
             (1 = 与噪声无异, 0 = 平原, >1 = 真尖峰). 总分 = u1·u2·u3.
    Phase 3  持仓去重: 总分降序流式贪心, 平均逐日持仓重合 ≥ 0.5 视为同一风格.
    Phase 4  最终名单真回测 (backtest/engine.hpp 同一内核; cpp 自检与
             strategy_nav 逐点对账, 不过关直接 assert fail ⇒ 挖出的权重填回
             strategy/def/<name>.hpp 必然复现).

本脚本只做三件事 (零重算 — 邻域敏感性/去重都已在 cpp 精确算完):
    1. 读 output/mine/{k_grid, point_metrics, styles, bt_metrics, windows}.npy
       + meta.json
    2. 按 rank_key() 排序风格名单
    3. 打印表格 + 可直接粘回 cpp 的 weights

前置: cpp 侧 mine::MINE_ENABLE = true 跑一次 (见 cpp/include/mine/spec.hpp)

用法: python py/app/mine.py
"""

import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
MINE_DIR = ROOT / "output" / "mine"

TOP_N = 40  # 打印条数


# ============================================================================
# rank_key — 唯一需要动的地方. 对**风格名单**排序 (真回测指标只有它们才有).
#
# pt 是该风格的全格指标列 (meta::point_metric_names):
#   梳子均值/IR/p10/胜率 · 斜率均值 · R2均值 · 夏普均值/IR/p10/胜率
#   · 分层分 · 夏普分 · 敏感性 · 稳健分 · 总分
# bt 是回测列 (meta::bt_metric_names):
#   年化 / 夏普 / 波动率 / 最大回撤 (≤0) / NAV倍数 / 年换手率 / 创新高最长天数
# 返回 [S] float 数组, 越大越好; -inf = 直接淘汰.
#
# 默认就用 cpp 的总分 (u1·u2·u3) + 回测硬约束. 注意秩统计量对**绝对幅度**免疫,
#   "微弱但极稳"的边缘也能拿高分 — 设定尺度的只有交易成本, 而成本只在真回测里
#   出现. 若榜首全是微弱边缘, 用 pt["斜率均值"] (单位 = 年化价差) 加个下限.
#   per-window 明细在 d["win"], 自定义加权 / 风格聚类都在这里做.
# ============================================================================
MAX_TURNOVER = 60.0  # 年换手率上限 (倍); 超过 = 交易成本假设不可信
MAX_DRAWDOWN = -0.50  # 最大回撤下限; 更深 = 拿不住


def rank_key(pt: dict[str, np.ndarray], bt: dict[str, np.ndarray]) -> np.ndarray:
    y = np.asarray(pt["总分"], dtype=np.float64).copy()
    bad = (
        ~np.isfinite(y)
        | (bt["年换手率"] > MAX_TURNOVER)
        | (bt["最大回撤"] < MAX_DRAWDOWN)
    )
    y[bad] = -np.inf
    return y


# ============================================================================
# 载入
# ============================================================================
def load() -> dict:
    meta = json.loads((MINE_DIR / "meta.json").read_text())
    k_grid = np.load(MINE_DIR / "k_grid.npy")  # [P, n] int8
    ptm = np.load(MINE_DIR / "point_metrics.npy")  # [P, c] f4 (全 lattice)
    styles = np.load(MINE_DIR / "styles.npy")  # [S] int32, 总分降序
    btm = np.load(MINE_DIR / "bt_metrics.npy")  # [S, m] f4
    win = np.load(MINE_DIR / "windows.npy")  # [S, W, 4] f4
    assert k_grid.shape[0] == ptm.shape[0] == meta["n_points"]
    assert k_grid.shape[1] == len(meta["factor_names"])
    assert ptm.shape[1] == len(meta["point_metric_names"])
    assert styles.shape[0] == btm.shape[0] == win.shape[0] == meta["n_styles"]
    assert btm.shape[1] == len(meta["bt_metric_names"])
    assert win.shape[1] == meta["layer"]["n_windows"]
    assert win.shape[2] == len(meta["window_metric_names"])
    idx = styles.astype(np.int64)
    return {
        "meta": meta,
        "k": k_grid,
        "styles": idx,
        # 风格名单视图 (全格列按 styles 取行)
        "pt": {nm: ptm[idx, i] for i, nm in enumerate(meta["point_metric_names"])},
        "bt": {nm: btm[:, i] for i, nm in enumerate(meta["bt_metric_names"])},
        "win": win,  # per-window (梳子分, 斜率, R2, 夏普) — 自定义加权/聚类用
        "factors": meta["factor_names"],
        "factor_cn": meta["factor_cn_names"],
        "M": meta["lattice_m"],
    }


# ============================================================================
# 打印
# ============================================================================
def cpp_weights(k_row: np.ndarray, factors: list[str], m: int) -> str:
    """可直接粘进 strategy/def/<name>.hpp 的 weights 数组.

    写成精确的 k/M 分数: 编译期常量折叠与 cpp 挖掘端
    static_cast<float>(k) / static_cast<float>(M) 逐位相同 ⇒ 填回去必然复现.
    顺序与 MINE_FACTORS 一致 ⇒ score 的浮点累加顺序也一致.
    """
    lines = []
    for i, kk in enumerate(k_row):
        if kk == 0:
            continue
        lines.append(f"    {{&feature::def::{factors[i]}_spec, {int(kk)}.0f / {m}}},")
    return "\n".join(lines)


def main():
    d = load()
    meta, k, styles = d["meta"], d["k"], d["styles"]
    m_lat = d["M"]
    pt, bt = d["pt"], d["bt"]
    y = rank_key(pt, bt)

    n_ok = int(np.isfinite(y).sum())
    lc = meta["layer"]
    print(
        f"\n策略 {meta['strategy']} | 窗口 {meta['window']['start']}"
        f"..{meta['window']['end']} ({meta['window']['n_days']} 日)"
    )
    print(
        f"lattice M={m_lat}, 因子 {len(d['factors'])} 个, "
        f"{meta['n_points']} 个权重方向; 分层 {lc['n_bins']} 档 "
        f"× {lc['n_windows']} 窗 (窗 {lc['window_days']} 日 / 步 {lc['step_days']} 日)"
    )
    print(
        f"去重: 总分前 {meta['dedup_cand']} ⇒ {meta['n_styles']} 个风格 "
        f"(逐日重合 ≥ {meta['dedup_overlap']:.2f} 算同风格), 通过硬约束 {n_ok} 个"
    )
    print(f"cpp 自检 NAV 最大相对偏差 {meta['selfcheck']['nav_max_rel_diff']:.2e}")
    assert n_ok > 0, "全部风格被硬约束淘汰 — 放宽 MAX_TURNOVER / MAX_DRAWDOWN"

    bl = meta["baseline_metrics"]
    print(f"\n基线 (策略当前 weights): {meta['baseline_weights']}")
    print(
        f"基线: 梳子均值 {bl['梳子均值']:.4f} ({bl['梳子均值分位']:.1%} 分位) | "
        f"夏普均值 {bl['夏普均值']:.2f} ({bl['夏普均值分位']:.1%} 分位)"
    )

    top = np.argsort(-y)[: min(TOP_N, n_ok)]
    hdr = (
        f"{'#':>3} {'总分':>6} {'分层':>5} {'夏普':>5} {'稳健':>5} {'敏感':>5} "
        f"{'梳均值':>7} {'夏普均':>6} {'年化':>8} {'夏普':>6} {'回撤':>8} {'换手':>6}"
    )
    print("\n" + hdr)
    print("-" * len(hdr))
    for t, p in enumerate(top):
        print(
            f"{t + 1:>3} {pt['总分'][p]:>6.3f} {pt['分层分'][p]:>5.2f} "
            f"{pt['夏普分'][p]:>5.2f} {pt['稳健分'][p]:>5.2f} {pt['敏感性'][p]:>5.2f} "
            f"{pt['梳子均值'][p]:>7.3f} {pt['夏普均值'][p]:>6.2f} "
            f"{bt['年化'][p]:>8.2%} {bt['夏普'][p]:>6.2f} {bt['最大回撤'][p]:>8.2%} "
            f"{bt['年换手率'][p]:>6.1f}"
        )

    print("\n" + "=" * 60)
    for t, p in enumerate(top[:3]):
        print(
            f"# top{t + 1}  总分={pt['总分'][p]:.3f} "
            f"敏感={pt['敏感性'][p]:.2f} 年化={bt['年化'][p]:.2%}"
        )
        print(cpp_weights(k[styles[p]], d["factors"], m_lat))
        print()


if __name__ == "__main__":
    main()
