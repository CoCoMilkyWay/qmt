"""
因子权重挖掘 — 后处理

计算全在 cpp (src/mine/mine.cpp): 继承 mine::MINE_STRATEGY 那个策略的
pool / filters / hold_n / exit_ratio / 回测窗口, 只搜 weights; 每个权重方向都
走 backtest/engine.hpp 的**同一个回测内核**, 所以这里读到的指标就是把该权重填回
strategy/def/<name>.hpp 之后 cpp 会跑出来的指标 (cpp 侧启动时已用目标策略自己的
weights 与 backtest::run 的 strategy_nav 逐点对账, 不过关直接 assert fail).

本脚本只做三件事 (零重算):
    1. 读 output/mine/{k_grid.npy, metrics.npy, meta.json}
    2. 按 fitness() 排序取 top-N
    3. 算邻域平原度 (抗过拟合) + 打印可直接粘回 cpp 的 weights

前置: cpp 侧 mine::MINE_ENABLE = true 跑一次 (见 cpp/include/mine/spec.hpp)

用法: python py/app/mine.py
"""

import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
MINE_DIR = ROOT / "output" / "mine"

TOP_N = 40  # 打印条数
PLATEAU_HOPS = 1  # 邻域半径 (跳数); 1 跳 = L1 距离 2 = "把一个单位从某因子挪到另一因子"


# ============================================================================
# fitness — 唯一需要动的地方.
#
# m 是 dict[str, np.ndarray], key = meta.json::metric_names:
#   年化 / 夏普 / 波动率 / 最大回撤 (≤0) / NAV倍数 / 年换手率 / 创新高最长天数
# 返回 [P] float 数组, 越大越好; -inf = 直接淘汰.
#
# 默认口径故意保守 (夏普 + 硬约束), 真正的取舍留给使用者:
#   例如加"年化与夏普的几何平均"、按换手罚分、要求最大回撤优于基线、
#   或先用硬约束筛出一个可行域再按邻域平原度排序.
# ============================================================================
MAX_TURNOVER = 60.0  # 年换手率上限 (倍); 超过 = 交易成本假设不可信
MAX_DRAWDOWN = -0.50  # 最大回撤下限; 更深 = 拿不住


def fitness(m: dict[str, np.ndarray]) -> np.ndarray:
    y = np.asarray(m["夏普"], dtype=np.float64).copy()
    bad = (
        ~np.isfinite(y)
        | (m["年换手率"] > MAX_TURNOVER)
        | (m["最大回撤"] < MAX_DRAWDOWN)
    )
    y[bad] = -np.inf
    return y


# ============================================================================
# 载入
# ============================================================================
def load() -> dict:
    meta = json.loads((MINE_DIR / "meta.json").read_text())
    k_grid = np.load(MINE_DIR / "k_grid.npy")  # [P, n] int8
    metrics = np.load(MINE_DIR / "metrics.npy")  # [P, m] float32
    names = meta["metric_names"]
    assert k_grid.shape[0] == metrics.shape[0] == meta["n_points"]
    assert k_grid.shape[1] == len(meta["factor_names"])
    assert metrics.shape[1] == len(names)
    return {
        "meta": meta,
        "k": k_grid,
        "m": {nm: metrics[:, i] for i, nm in enumerate(names)},
        "factors": meta["factor_names"],
        "factor_cn": meta["factor_cn_names"],
        "M": meta["lattice_m"],
    }


# ============================================================================
# 邻域平原度
#   lattice 上 k 的 1 跳邻居 = 把一个单位从坐标 i 挪到坐标 j (Σ|k| 不变):
#     |k_i| 减 1 (朝 0 走), |k_j| 加 1 (背离 0 走; k_j = 0 时正负各一个)
#   k → base-(2M+1) 的 int64 key, 排序后 searchsorted 定位 ⇒ 纯查表, 不重算回测.
#   平原度 = 邻域内 fitness 的均值 / 中心 fitness (越接近 1 越平坦, 越小越像山尖).
# ============================================================================
def _keys(k: np.ndarray, m: int) -> np.ndarray:
    base = 2 * m + 1
    pw = (base ** np.arange(k.shape[1], dtype=np.int64)).astype(np.int64)
    return ((k.astype(np.int64) + m) * pw).sum(axis=1)


def _neighbors(k_center: np.ndarray, m: int) -> np.ndarray:
    """[n_nbr, n] — 中心点的全部 1 跳邻居 (Σ|k| = M 不变)."""
    n = k_center.size
    out = []
    for i in range(n):
        if k_center[i] == 0:
            continue
        for j in range(n):
            if j == i:
                continue
            steps = [1, -1] if k_center[j] == 0 else [np.sign(k_center[j])]
            for s in steps:
                nb = k_center.astype(np.int64).copy()
                nb[i] -= np.sign(nb[i])
                nb[j] += s
                if np.abs(nb).max() > m:
                    continue
                out.append(nb)
    return np.array(out, dtype=np.int64) if out else np.zeros((0, n), np.int64)


def plateau(k: np.ndarray, y: np.ndarray, idx: np.ndarray, m: int):
    keys = _keys(k, m)
    order = np.argsort(keys)
    ks = keys[order]
    ratio = np.zeros(idx.size)
    n_found = np.zeros(idx.size, dtype=np.int64)
    for t, p in enumerate(idx):
        nb = _neighbors(k[p], m)
        for hop in range(PLATEAU_HOPS - 1):
            grown = [nb] + [_neighbors(x, m) for x in nb]
            nb = np.unique(np.vstack(grown), axis=0)
        if nb.shape[0] == 0:
            continue
        nk = _keys(nb.astype(np.int8), m)
        pos = np.searchsorted(ks, nk)
        pos = np.clip(pos, 0, ks.size - 1)
        hit = ks[pos] == nk
        found = order[pos[hit]]
        found = found[found != p]
        n_found[t] = found.size
        if found.size:
            vals = y[found]
            vals = vals[np.isfinite(vals)]
            if vals.size and y[p] != 0:
                ratio[t] = vals.mean() / y[p]
    return ratio, n_found


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
        lines.append(
            f"    {{&feature::def::{factors[i]}_spec, {int(kk)}.0f / {m}}},")
    return "\n".join(lines)


def main():
    d = load()
    meta, k, mm = d["meta"], d["k"], d["m"]
    m_lat = d["M"]
    y = fitness(mm)

    n_ok = int(np.isfinite(y).sum())
    print(
        f"\n策略 {meta['strategy']} | 窗口 {meta['window']['start']}"
        f"..{meta['window']['end']} ({meta['window']['n_days']} 日)"
    )
    print(
        f"lattice M={m_lat}, 因子 {len(d['factors'])} 个, "
        f"{meta['n_points']} 个权重方向, 通过硬约束 {n_ok} 个 "
        f"({100.0 * n_ok / meta['n_points']:.1f}%)"
    )
    print(f"cpp 自检 NAV 最大相对偏差 {meta['selfcheck']['nav_max_rel_diff']:.2e}")
    assert n_ok > 0, "全部权重被硬约束淘汰 — 放宽 MAX_TURNOVER / MAX_DRAWDOWN"

    top = np.argsort(-y)[: min(TOP_N, n_ok)]
    ratio, n_nbr = plateau(k, y, top, m_lat)

    print(f"\n基线 (策略当前 weights): {meta['baseline_weights']}")
    hdr = f"{'#':>3} {'fitness':>8} {'年化':>8} {'夏普':>7} {'回撤':>8} {'换手':>7} {'NAV':>7} {'平原':>6} {'邻居':>5}"
    print("\n" + hdr)
    print("-" * len(hdr))
    for t, p in enumerate(top):
        print(
            f"{t + 1:>3} {y[p]:>8.3f} {mm['年化'][p]:>8.2%} {mm['夏普'][p]:>7.2f} "
            f"{mm['最大回撤'][p]:>8.2%} {mm['年换手率'][p]:>7.1f} "
            f"{mm['NAV倍数'][p]:>7.1f} {ratio[t]:>6.2f} {n_nbr[t]:>5}"
        )

    print("\n" + "=" * 60)
    for t, p in enumerate(top[:3]):
        print(f"# top{t + 1}  fitness={y[p]:.3f} 平原={ratio[t]:.2f}")
        print(cpp_weights(k[p], d["factors"], m_lat))
        print()


if __name__ == "__main__":
    main()
