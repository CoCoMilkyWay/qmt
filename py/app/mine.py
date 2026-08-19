"""
Simplex lattice 因子权重挖掘

============================================================
整体流程 (按触发顺序):
============================================================

[阶段 1] 全量搜索: 对 lattice 上每个权重算 fitness Y
    触发: 1 次 batch 调用, 一次喂 P 个权重
          P = Σ_j C(n,j)·C(M-1,j-1)·2^j (带符号 lattice)
          n=8, M=10 -> 658048
    kernel: evaluate_monotonicity_csr  (parallel=True, prange over pop)
    每权重开销: 每日 (F 点积 f32 SIMD + 分位 quickselect O(cnt) + G 档区间累加)
               月末 G x G 朴素排名 + Spearman + (rho+1)/2
    ≈ 全部运行时间的 99%

[阶段 2] Top-N 后评估 (三件套, 合计 ~0.1% 耗时, 基本可忽略, N=TOP_N):

    (2a) 粘性+扣费 NAV 复评   kernel: evaluate_batch_csr
         触发: 分块调用 (进度条), pop = N (top N 权重)
         目的: 给出对齐 cpp backtest 实盘口径的多空/多头 NAV
         相对主搜索单点更慢 (多 bitmask 粘性 + 换手统计), 但只 N 次

    (2b) 参数平原敏感度       kernel: _neighbor_stats_csr (prange over centers)
         触发: 分块调用 (进度条), top N 每中心一个 BFS, [1, NEIGHBOR_DISTANCE_MAX] 跳
               一跳 = 某 k_i -1 & 另一 k_j +1 (L1=2), 单步邻居上限 n*(n-1)
               多跳总数随 N 组合增长, 默认 N=3 仍远小于 lattice 规模
         目的: 邻居 Y 均值 vs 中心 Y 的衰减, 判断是否过拟合山尖
         不重跑评估, 只从阶段 1 的 fitness 数组里查表, 开销忽略

    (2c) 最优权重年度档位表   kernel: evaluate_year_group_matrix_csr
         触发: 1 次调用, 仅对 top1 权重
         目的: 打印每年每档 (Q1..QG) 的累计收益 + 该年单调度, 便于直观检查
         单点与主 kernel 同量级, 规模差 10626x

[阶段 3] 组合精选 (把 top-K 当作 K 条子策略, 解一个小型投资组合):
    候选 = 阶段 1 的 top-K (K=TOP_N); 子策略收益流 = 阶段 2a 的
    粘性+扣费多头日收益 (n_pop, D). 穷举全部 C(K, PORTFOLIO_N) 个组合,
    组合内配比 = 逆波动率 (风险平价近似). 三维指标:
        gmNbrY   成员邻域稳定性的几何平均 (参数平原, 抗过拟合)
        Sharpe_p 逆波动率配比后组合的年化夏普 (风险调整收益)
        Orth     det(成员日收益相关阵) ∈ [0,1] — 标准化日收益向量的
                 Gram 内积体积, 1 = 两两正交 (日频收益正交性的直接度量)
    每维对全体组合做截面 pct rank, score = 三维分位相乘 — 分位 ∈ (0,1],
    任一维平庸都会把乘积压下去, 最优解天然趋向三维均衡.
    kernel: _portfolio_metrics_csr (组合 odometer 穷举, 每组合 N² 协方差
    聚合 + N³ Cholesky 行列式, C(200,3)≈1.3e6 秒级; 分块续跑供进度条)

============================================================
Fitness 定义 (阶段 1): 周期内分层单调度的跨周期均值 Y ∈ [0, 1]
============================================================
    累计周期由 FITNESS_PERIOD 控制: "year" | "month" | "week" | "day"
    每日: 按 scores = ranks @ w 升序切 G 档, 档 g 当日等权收益
          累加到 period_group[period(d), g]
    每周期: 对 G 个档的周期内累计收益做 Spearman 秩相关 rho,
            单调度 = (rho + 1) / 2 ∈ [0,1]
            有效过滤: 该周期活跃日 >= MIN_PERIOD_DAYS[FITNESS_PERIOD]
    fitness = 跨周期算术平均
    (周期越短, 惩罚短期单调性崩坏越敏感; 越长越平滑)

    注: "每日切档" 不是复杂化, 而是周期档位收益的标准日频实现
        (截面每天都变, 不能用周期级一次性分档)

============================================================
因子处理 (口径对齐)
============================================================
    直读 cpp 张量的 factor 特征 (cs.cpp factor_pipeline 产物:
    全市场截面 winsor/z/pct_rank ∈ [0,1], 截面均值填充后全 finite).
    score = Σ w_f · factor_f 与 cpp cs_factor_score 完全同口径
    (factor 全 finite 时分母 Σw 为常数, 排序不变); 全非负权重可
    直接填回 config::STRATEGY_FACTOR_WEIGHTS, 负权重 (因子反转)
    需 cpp 侧翻转该因子 pipeline 的 invert 标志后取 |w|.

============================================================
搜索算法 (Simplex lattice / stars-and-bars)
============================================================
    观察: 每日分档只依赖 scores = ranks @ w 的相对排序 (argsort), 所以
        w 与 c*w (任意 c > 0) 产生完全相同的持仓 / 换手 / NAV.
    结论: 有意义的搜索空间是权重的 "方向", 即 L1 球面 { Σ |w_i| = 1 }.
        负权重 = 因子反转 (f 全 finite 时 -w·f 与 w·(1-f) 排序等价),
        无方向先验: 方向不由 cpp pipeline 的 invert 固定, 由搜索自行决定.

    lattice 离散化: w_i = k_i / M, k_i 整数, Σ|k_i| = M.
    stars-and-bars 递归枚举非负点 + 对非零坐标做 2^nnz 符号展开,
    每个点是唯一归一化方向, 无比例冗余.
        点数 = Σ_j C(n,j)·C(M-1,j-1)·2^j; n=8, M=10 -> 658048

    精度: 1/M (M=10 对应步长 0.10).
    调 M 直接权衡分辨率 ↔ 评估耗时.

============================================================
准确性要点
============================================================
    - 数据源: output/tensor/*.npy (cpp feature::dump_tensor, 每 feature 一个
        (n_a, n_d) float32; 需 config::TENSOR_DUMP_ENABLE = true 重新 build)
        + output/meta.json (D 轴日期 / A 轴 codes / factor 名单)
    - 张量已是 build-time PIT (row D 只含 T 当日可见信息), 挖掘端零时间偏移;
        T+1 收益 = daily_return[:, d+1] (决策用 row d, 持有 d→d+1)
    - 分档母集 = POOL_FEATURE ("pool" 排名母集 / "tradable" 策略选股母集)
    - 搜索阶段 (fitness Y 评估): 每日 score 升序等比切分 G 档, 每档等权日收益,
        不做涨跌停粘性、不扣换手费. 这是"因子原始分层能力"指标.
    - Top-N 复评阶段 (粘性+扣费, 对齐 cpp backtest 四条限制):
        status 由 limit_up/limit_dn/susp 派生: 0=停牌/1=跌停/2=正常/3=涨停
        * status != 2 的标的"冻结": 当日持仓状态 = 昨日持仓状态, 不产生换手
            - 涨停持仓不卖 (预期次日超额收益)
            - 跌停持仓不卖 (做不到)
            - 涨停非持仓不买 (做不到)
            - 跌停非持仓不买 (预期次日超额风险)
        * 只有 status == 2 的标的可自由进出 long / short 档
        * 成本: 每日对 long/short 分别计算真实换手率, 乘以 COST_ROUND_TRIP (千2)

============================================================
效率要点
============================================================
    - 零导出零缓存: mmap 直读张量, 只 gather pool 内 cell (~1e5 行) 构 CSR,
      加载亚秒级; 换窗口/换因子/换母集直接重跑, 无 npz 失效问题
    - 搜索点位于带符号整数 lattice (k_i 整数, Σ|k|=M), 规模随 M 温和增长
    - 整数坐标编码 int64 key (基 2M+1), BFS 邻居 key 增量算术 + 二分定位, prange 并行
    - 单 CSR 紧凑布局 (pool 内 factor/ret-valid 标的 + status + period 标注)
    - 单调度 kernel: 每日分位 quickselect 分 G 档 (免全排序免分配) + 区间累加, 无粘性 bitmask
    - 粘性+扣费 kernel 只在 top N 上跑 (N 次调用, 可忽略)
    - 内存 C-order, 手写 dot, fastmath, prange 并行

使用方式:
    python py/app/mine.py   (先以 config::TENSOR_DUMP_ENABLE = true 跑出张量)
"""

import json
import math
from datetime import date
from pathlib import Path

import numba
import numpy as np
from tqdm.auto import tqdm

# ==================== 配置 ====================

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "output"
TENSOR_DIR = OUT_DIR / "tensor"

START_DATE = "20170101"  # 含; "" = 张量起点
END_DATE = ""            # 含; "" = 张量终点 (末日无 T+1 收益, 自动去掉)
GROUP_NUM = 5  # 分档数
FITNESS_PERIOD = "week"  # 阶段 1 fitness Y 的累计周期: "year" | "month" | "week" | "day"
MIN_PERIOD_DAYS = {       # 各周期内活跃日数下限, 活跃日数 < 下限的周期不计入 fitness
    "year": 120,
    "month": 10,
    "week": 3,
    "day": 1,
}
COST_ROUND_TRIP = 0.002  # 一次换手综合成本 (买 0.0005 + 卖 0.0015)
LATTICE_M = 6  # lattice 阶数: w_i = k_i / M, k_i ∈ Z, Σ|k_i| = M (无方向先验, 恒带符号)
# 负权重 = 因子反转: 排序意义上 -w·f ≡ w·(1-f) (f 全 finite 时差常数),
# 方向不采用 cpp pipeline invert 的先验, 完全由搜索决定.
# 点数 = Σ_j C(n,j)·C(M-1,j-1)·2^j; 8 因子 M=10 -> 658048
TOP_N = 200  # 阶段2：按 fitness 取前 N 条做 NAV 复评、邻居表与打印 (可改)
NEIGHBOR_DISTANCE_MAX = 10  # 阶段 2b 邻居敏感度: 统计 [1, N] 跳内全部 lattice 点的 Y 均值
# 一跳 = 某因子 -1, 另一因子 +1 (L1=2); N=3 -> BFS 最多 3 层
STAR_LEVELS = 10  # 衰减星级分档数: 衰减升序排名分 STAR_LEVELS 档, 最低档 = STAR_LEVELS 星 (最平原)

PORTFOLIO_N = 3       # 阶段 3: 从 top-K 穷举挑 N 条子策略构成组合
TRADING_DAYS = 252    # 年化因子 (Sharpe / 年化收益)

# 不持仓月份 (1..12). 命中月份的交易日, fitness kernel 跳过 (不进 year_group, 不累加 year_days),
# NAV kernel 跳过 (持仓状态冻结, nav 不变, 无交易成本).
SKIP_MONTHS = frozenset({})

# 分档母集: "pool" = 排名母集 (与 factor pct_rank 口径一致) | "tradable" = 策略选股母集
POOL_FEATURE = "tradable"

# 搜索权重的 factor 特征 (张量 F 枚举 CS factor 段, 名单见 meta.json factor_names;
# 顺序即权重维度顺序). 挖到的权重可直接填回 config::STRATEGY_FACTOR_WEIGHTS.
SEARCH_FACTOR_NAMES = [
    "bp_ttm3",
    "ep_ttm12",
    "sp_ttm12",
    "cp_ttm12",
    "close",
    "fmcap",
    "mcap",
    "dy_ttm12",
]

assert len(SEARCH_FACTOR_NAMES) >= 1
assert len(SEARCH_FACTOR_NAMES) == len(set(SEARCH_FACTOR_NAMES))
assert POOL_FEATURE in ("pool", "tradable")
assert 2 <= PORTFOLIO_N <= TOP_N
assert math.comb(TOP_N, PORTFOLIO_N) <= 2e8, \
    f"C({TOP_N},{PORTFOLIO_N})={math.comb(TOP_N, PORTFOLIO_N):.2e} 组合穷举过大, 调小 TOP_N 或 PORTFOLIO_N"

for _m in SKIP_MONTHS:
    assert isinstance(
        _m, int) and 1 <= _m <= 12, f"SKIP_MONTHS 含非法月份 {_m}, 必须是 1..12 整数"

assert FITNESS_PERIOD in MIN_PERIOD_DAYS, \
    f"FITNESS_PERIOD={FITNESS_PERIOD} 无效, 必须是 {list(MIN_PERIOD_DAYS.keys())} 之一"


# ==================== 数据加载 (直读 CPP 张量) ====================

def load_data() -> dict:
    """
    mmap 直读 output/tensor/*.npy (每 feature 一个 (n_a, n_d) float32, a-major)
    + output/meta.json, 在 [START_DATE, END_DATE] 窗口内按 POOL_FEATURE 母集
    gather 出单 CSR 紧凑布局 (日主序):
        ranks  (N, F)  搜索因子截面 pct_rank (张量原值, 全 finite)
        rets   (N,)    T+1 收益 = daily_return[:, d+1]
        insts  (N,)    A 轴全局索引 (粘性 kernel bitmask 用)
        status (N,)    0=停牌 / 1=跌停 / 2=正常 / 3=涨停 (对齐 cpp backtest 冻结口径)
        off    (D+1,)  CSR 段偏移
    """
    meta = json.loads((OUT_DIR / "meta.json").read_text(encoding="utf-8"))
    dates_i = np.asarray(meta["dates"], dtype=np.int64)  # YYYYMMDD 升序
    n_a = len(meta["codes"])
    n_d = len(dates_i)
    for _n in SEARCH_FACTOR_NAMES:
        assert _n in meta["factor_names"], \
            f"搜索因子 {_n} 不在张量 factor 名单 {meta['factor_names']} 中"
    assert TENSOR_DIR.is_dir(), \
        f"{TENSOR_DIR} 不存在 — 置 cpp config::TENSOR_DUMP_ENABLE = true 后重新 build"

    def tensor(name: str) -> np.ndarray:
        path = TENSOR_DIR / f"{name}.npy"
        assert path.exists(), f"缺张量 {path}"
        arr = np.load(path, mmap_mode="r")
        assert arr.shape == (n_a, n_d) and arr.dtype == np.float32, \
            f"{name}: shape={arr.shape} dtype={arr.dtype}, 期望 ({n_a}, {n_d}) float32"
        return arr

    # 窗口 [d_lo, d_hi); 右端至多 n_d-1 (末日无 T+1 收益)
    d_lo = int(np.searchsorted(dates_i, int(START_DATE),
               side="left")) if START_DATE else 0
    d_hi = int(np.searchsorted(dates_i, int(END_DATE),
               side="right")) if END_DATE else n_d
    d_hi = min(d_hi, n_d - 1)
    assert d_lo < d_hi, \
        f"窗口为空: [{START_DATE}, {END_DATE}] vs 张量 [{dates_i[0]}, {dates_i[-1]}]"
    D = d_hi - d_lo
    dates_sel = dates_i[d_lo:d_hi]
    print(f"张量: {TENSOR_DIR} (n_a={n_a}, n_d={n_d}), "
          f"窗口 {dates_sel[0]}..{dates_sel[-1]} ({D} 日)")

    # 母集 ∧ finite(T+1 收益) → 日主序 CSR
    fwd = np.ascontiguousarray(tensor("daily_return")[
                               :, d_lo + 1:d_hi + 1])  # (n_a, D)
    member = (np.asarray(tensor(POOL_FEATURE)[
              :, d_lo:d_hi]) > 0.5) & np.isfinite(fwd)
    day_of_row, a_of_row = np.nonzero(member.T)  # 按 d 升序, 段内按 a 升序
    N = len(a_of_row)
    assert N > 0, "母集为空 — 检查 POOL_FEATURE 与窗口"
    off = np.zeros(D + 1, dtype=np.int32)
    off[1:] = np.cumsum(np.bincount(day_of_row, minlength=D)).astype(np.int32)
    d_abs = d_lo + day_of_row

    flat_rets = fwd[a_of_row, day_of_row]
    flat_insts = a_of_row.astype(np.int32)

    # status: susp 最后写 (停牌语义覆盖涨跌停; POOL_FEATURE="pool" 时 pool_b 已排 susp)
    flat_status = np.full(N, 2, dtype=np.int8)
    flat_status[tensor("limit_dn")[a_of_row, d_abs] > 0.5] = 1
    flat_status[tensor("limit_up")[a_of_row, d_abs] > 0.5] = 3
    flat_status[tensor("susp")[a_of_row, d_abs] > 0.5] = 0

    F = len(SEARCH_FACTOR_NAMES)
    flat_ranks = np.empty((N, F), dtype=np.float32)
    for f, name in enumerate(SEARCH_FACTOR_NAMES):
        flat_ranks[:, f] = tensor(name)[a_of_row, d_abs]
    assert np.isfinite(flat_ranks).all(), \
        "factor 含非 finite — factor_pipeline 截面均值填充后应全 finite"

    # 周期标注
    year_of_day = (dates_sel // 10000).astype(np.int32)
    month_of_day = ((dates_sel // 100) % 100).astype(np.int8)
    unique_years, year_idx = np.unique(year_of_day, return_inverse=True)
    year_idx = year_idx.astype(np.int32)
    skip_arr = np.array(sorted(SKIP_MONTHS), dtype=np.int8)
    active_day = (~np.isin(month_of_day, skip_arr)).astype(np.uint8)

    # 注: SKIP_MONTHS 命中日由 active_day=0 在 kernel 内 continue 掉, period_idx 值不会被访问
    if FITNESS_PERIOD == "year":
        period_idx = year_idx.copy()
        n_periods = len(unique_years)
    elif FITNESS_PERIOD == "month":
        _, period_idx = np.unique(dates_sel // 100, return_inverse=True)
        period_idx = period_idx.astype(np.int32)
        n_periods = int(period_idx.max()) + 1
    elif FITNESS_PERIOD == "week":
        iso_key = np.array(
            [y * 100 + w for y, w in
             (date(int(s) // 10000, (int(s) // 100) % 100, int(s) % 100).isocalendar()[:2]
              for s in dates_sel)],
            dtype=np.int64)
        _, period_idx = np.unique(iso_key, return_inverse=True)
        period_idx = period_idx.astype(np.int32)
        n_periods = int(period_idx.max()) + 1
    else:  # "day" (配置 assert 保证)
        period_idx = np.arange(D, dtype=np.int32)
        n_periods = D

    data = {
        "ranks": flat_ranks,
        "rets": flat_rets,
        "insts": flat_insts,
        "status": flat_status,
        "off": off,
        "year_idx": year_idx,
        "period_idx": period_idx,
        "n_periods": n_periods,
        "min_period_days": MIN_PERIOD_DAYS[FITNESS_PERIOD],
        "active_day": active_day,
        "years": unique_years.tolist(),
        "factor_names": list(SEARCH_FACTOR_NAMES),
        "n_stocks": n_a,
        "max_cnt": int(np.diff(off).max()),  # kernel 内共享, 避免重复扫描
    }
    n_active = int(active_day.sum())
    skip_disp = sorted(SKIP_MONTHS) if SKIP_MONTHS else "无"
    print(f"  母集={POOL_FEATURE}, 样本: {N}, 日均 {N / D:.1f}, "
          f"年数: {len(unique_years)} ({unique_years[0]}..{unique_years[-1]}), "
          f"周期=\"{FITNESS_PERIOD}\" 桶数: {n_periods}")
    print(
        f"  SKIP_MONTHS={skip_disp}, 活跃日数: {n_active}/{D} ({100.0 * n_active / D:.1f}%)")
    return data


# ==================== numba kernel ====================

@numba.njit(parallel=True, cache=True, fastmath=True, boundscheck=False)
def evaluate_batch_csr(
    pop,           # (n_pop, F) float32
    ranks,         # (N, F) float32 C-order
    rets,          # (N,) float32
    insts,         # (N,) int32
    status,        # (N,) int8: 0=停牌, 1=跌停, 2=正常, 3=涨停
    off,           # (D+1,) int32
    active_day,    # (D,) uint8, 0=SKIP_MONTHS 命中日, 持仓冻结 nav 不变
    year_idx,      # (D,) int32, 0..n_years-1
    n_years,       # int
    group_num,     # int
    cost_rt,       # float: 一次换手综合成本 (buy + sell)
    n_stocks,      # int: 全标的数, 用于 bitmask
    max_cnt,       # int, 预计算的最大日样本数
):
    """
    粘性持仓多空评估. 每日:
        universe = 全部 factor/ret-valid 标的 (cnt), gsz = cnt // group_num
        Locked-long  = prev_long ∩ (status != 2) 今日必须持有
        Locked-short = prev_short ∩ (status != 2) 今日必须持有
        Free-long 填满 gsz: 在 status==2 里按得分从高到低补齐
        Free-short 填满 gsz: 在 status==2 里按得分从低到高补齐
        turnover_long = |new_buys| / today_long_cnt  (new_buys 只可能来自 status==2, 合法可交易)
        turnover_short 同理 (new_shorts 也只可能来自 status==2)
        daily_ls   = ret_long - ret_short - (turnover_long + turnover_short) * cost_rt
        daily_long = ret_long - turnover_long * cost_rt
    NAV 按年分桶累乘 (每年起算 1.0):
        cum_nav = Π_y year_nav[y]      (等价全期累乘)
        avg_nav = mean_y year_nav[y]   (各年年末 NAV 算术平均)
    返回: (fitness, daily_long)
        fitness (n_pop, 4):
            [:,0] = 累计多空 NAV
            [:,1] = 累计多头 NAV
            [:,2] = 年均多空 NAV (逐年重置, 年末 NAV 算术平均)
            [:,3] = 年均多头 NAV
        daily_long (n_pop, D): 逐日多头收益 (已扣换手费);
            跳过日 (SKIP_MONTHS / 无样本 / 多头腿空) = NaN — 阶段 3 组合精选的收益流
    """
    n_pop = pop.shape[0]
    F = pop.shape[1]
    D = off.shape[0] - 1
    fitness = np.empty((n_pop, 4), dtype=np.float64)
    daily_long = np.full((n_pop, D), np.nan, dtype=np.float64)

    for p in numba.prange(n_pop):
        w = pop[p]
        scores = np.empty(max_cnt, dtype=np.float32)
        prev_long_mask = np.zeros(n_stocks, dtype=np.uint8)
        prev_short_mask = np.zeros(n_stocks, dtype=np.uint8)
        prev_long_ids = np.empty(max_cnt, dtype=np.int32)
        prev_short_ids = np.empty(max_cnt, dtype=np.int32)
        today_long_local = np.empty(max_cnt, dtype=np.int32)
        today_short_local = np.empty(max_cnt, dtype=np.int32)
        prev_long_cnt = 0
        prev_short_cnt = 0

        year_nav_ls = np.ones(n_years, dtype=np.float64)
        year_nav_l = np.ones(n_years, dtype=np.float64)
        year_seen = np.zeros(n_years, dtype=np.uint8)

        for d in range(D):
            if active_day[d] == 0:
                # SKIP_MONTHS 命中日: 持仓冻结, nav 不变, 无交易成本 (选项 C 语义)
                continue
            lo = off[d]
            cnt = off[d + 1] - lo
            if cnt == 0:
                continue
            gsz = cnt // group_num
            if gsz < 1:
                continue
            y = year_idx[d]
            year_seen[y] = 1

            # 合并扫描: 计算分数 + 识别 locked (两侧)
            today_long_cnt = 0
            today_short_cnt = 0
            for i in range(cnt):
                s = np.float32(0.0)
                for f in range(F):
                    s += ranks[lo + i, f] * w[f]
                scores[i] = s
                st = status[lo + i]
                if st != 2:
                    gid = insts[lo + i]
                    if prev_long_mask[gid] != 0:
                        today_long_local[today_long_cnt] = i
                        today_long_cnt += 1
                    if prev_short_mask[gid] != 0:
                        today_short_local[today_short_cnt] = i
                        today_short_cnt += 1

            # 升序: order[0]=min, order[cnt-1]=max
            order = np.argsort(scores[:cnt])

            # ---------- LONG: 从高分补齐到 gsz ----------
            j = cnt - 1
            while j >= 0 and today_long_cnt < gsz:
                local = order[j]
                if status[lo + local] == 2:
                    today_long_local[today_long_cnt] = local
                    today_long_cnt += 1
                j -= 1

            if today_long_cnt > 0:
                ret_sum = 0.0
                new_buys = 0
                for k in range(today_long_cnt):
                    local = today_long_local[k]
                    ret_sum += rets[lo + local]
                    if prev_long_mask[insts[lo + local]] == 0:
                        new_buys += 1
                ret_long = ret_sum / today_long_cnt
                turnover_long = new_buys / today_long_cnt
                long_active = True
            else:
                ret_long = 0.0
                turnover_long = 0.0
                long_active = False

            # ---------- SHORT: 从低分补齐到 gsz ----------
            j = 0
            while j < cnt and today_short_cnt < gsz:
                local = order[j]
                if status[lo + local] == 2:
                    today_short_local[today_short_cnt] = local
                    today_short_cnt += 1
                j += 1

            if today_short_cnt > 0:
                ret_sum = 0.0
                new_shorts = 0
                for k in range(today_short_cnt):
                    local = today_short_local[k]
                    ret_sum += rets[lo + local]
                    if prev_short_mask[insts[lo + local]] == 0:
                        new_shorts += 1
                ret_short = ret_sum / today_short_cnt
                turnover_short = new_shorts / today_short_cnt
                short_active = True
            else:
                ret_short = 0.0
                turnover_short = 0.0
                short_active = False

            # 更新 prev_long (两套 ids 都写 gid, 便于下一日 O(1) 查 mask)
            if long_active:
                for k in range(prev_long_cnt):
                    prev_long_mask[prev_long_ids[k]] = 0
                for k in range(today_long_cnt):
                    gid = insts[lo + today_long_local[k]]
                    prev_long_mask[gid] = 1
                    prev_long_ids[k] = gid
                prev_long_cnt = today_long_cnt

            if short_active:
                for k in range(prev_short_cnt):
                    prev_short_mask[prev_short_ids[k]] = 0
                for k in range(today_short_cnt):
                    gid = insts[lo + today_short_local[k]]
                    prev_short_mask[gid] = 1
                    prev_short_ids[k] = gid
                prev_short_cnt = today_short_cnt

            if long_active and short_active:
                daily = ret_long - ret_short - \
                    (turnover_long + turnover_short) * cost_rt
                year_nav_ls[y] *= (1.0 + daily)
            elif long_active:
                daily = ret_long - turnover_long * cost_rt
                year_nav_ls[y] *= (1.0 + daily)
            elif short_active:
                daily = -ret_short - turnover_short * cost_rt
                year_nav_ls[y] *= (1.0 + daily)

            if long_active:
                daily = ret_long - turnover_long * cost_rt
                year_nav_l[y] *= (1.0 + daily)
                daily_long[p, d] = daily

        cum_ls = 1.0
        cum_l = 1.0
        sum_ls = 0.0
        sum_l = 0.0
        n_seen = 0
        for y in range(n_years):
            if year_seen[y] != 0:
                cum_ls *= year_nav_ls[y]
                cum_l *= year_nav_l[y]
                sum_ls += year_nav_ls[y]
                sum_l += year_nav_l[y]
                n_seen += 1
        if n_seen > 0:
            avg_ls = sum_ls / n_seen
            avg_l = sum_l / n_seen
        else:
            avg_ls = 1.0
            avg_l = 1.0
        fitness[p, 0] = cum_ls
        fitness[p, 1] = cum_l
        fitness[p, 2] = avg_ls
        fitness[p, 3] = avg_l

    return fitness, daily_long


def evaluate_batch(pop: np.ndarray, data: dict) -> tuple[np.ndarray, np.ndarray]:
    """
    粘性+扣费 多空/多头 NAV 评估. 返回 (fitness, daily_long):
        fitness (n_pop, 4): [:,0]=累计多空, [:,1]=累计多头, [:,2]=年均多空, [:,3]=年均多头
        daily_long (n_pop, D): 逐日多头收益 (扣费; 跳过日 NaN)
    """
    pop = np.ascontiguousarray(pop, dtype=np.float32)
    return evaluate_batch_csr(
        pop,
        data["ranks"], data["rets"], data["insts"], data["status"], data["off"],
        data["active_day"],
        data["year_idx"], len(data["years"]),
        GROUP_NUM, COST_ROUND_TRIP, data["n_stocks"], data["max_cnt"],
    )


@numba.njit(cache=True, fastmath=True, boundscheck=False, inline='always')
def _accum_year_group(
    w,             # (F,) float32, 单个权重
    ranks,         # (N, F) float32 C-order
    rets,          # (N,) float32
    off,           # (D+1,) int32
    year_idx,      # (D,) int32
    active_day,    # (D,) uint8, 0=SKIP_MONTHS 命中日, kernel 跳过
    G,             # int
    scores,        # (max_cnt,) float32, 调用方分配复用 buffer
    idx,           # (max_cnt,) int32, 调用方分配复用 buffer
    seg,           # (8*G+16,) int64, 调用方分配复用 buffer (分段栈)
    year_group,    # (n_years, G) float64, 调用方 zero out
    year_days,     # (n_years,) int32, 调用方 zero out
):
    """
    核心 primitive: 按 w 每日 score→分位分 G 档→按年累加档内等权日收益.
    Spearman / NAV / year matrix 三种用法都复用这个累加结果.
    active_day[d]==0 的日子整日跳过 (不累加, 不计入 year_days).
    档只由分位边界 cnt*g//G 决定, 档内顺序无意义 → 多边界 Hoare quickselect
    O(cnt log G) 替代全排序 argsort O(cnt log cnt), 且零分配 (buffer 复用).
    并列分数跨边界的归属任意, 与不稳定 argsort 语义一致.
    """
    F = w.shape[0]
    D = off.shape[0] - 1
    for d in range(D):
        if active_day[d] == 0:
            continue
        lo = off[d]
        cnt = off[d + 1] - lo
        if cnt < G:
            continue
        for i in range(cnt):
            s = np.float32(0.0)
            for f in range(F):
                s += ranks[lo + i, f] * w[f]
            scores[i] = s
            idx[i] = i
        y = year_idx[d]
        # 分段栈, 每条 (l, r, gl, gr): idx[l:r) 覆盖档 gl..gr (l = cnt*gl//G)
        seg[0] = 0
        seg[1] = cnt
        seg[2] = 0
        seg[3] = G - 1
        sp = 4
        while sp > 0:
            sp -= 4
            l = seg[sp]
            r = seg[sp + 1]
            gl = seg[sp + 2]
            gr = seg[sp + 3]
            if gl == gr:
                sum_ret = 0.0
                for t in range(l, r):
                    sum_ret += rets[lo + idx[t]]
                year_group[y, gl] += sum_ret / (r - l)
                continue
            gm = (gl + gr) // 2 + 1
            b = cnt * gm // G  # 档 gm 的起始位置 (升序第 b 小); cnt>=G 保证 l < b < r
            # Hoare quickselect: 使 scores[idx[l:b)] 全 <= scores[idx[b:r)]
            ll = l
            rr = r - 1
            while ll < rr:
                pv = scores[idx[(ll + rr) >> 1]]
                i2 = ll
                j2 = rr
                while i2 <= j2:
                    while scores[idx[i2]] < pv:
                        i2 += 1
                    while scores[idx[j2]] > pv:
                        j2 -= 1
                    if i2 <= j2:
                        tmp = idx[i2]
                        idx[i2] = idx[j2]
                        idx[j2] = tmp
                        i2 += 1
                        j2 -= 1
                if b <= j2:
                    rr = j2
                elif b >= i2:
                    ll = i2
                else:
                    break
            seg[sp] = l
            seg[sp + 1] = b
            seg[sp + 2] = gl
            seg[sp + 3] = gm - 1
            seg[sp + 4] = b
            seg[sp + 5] = r
            seg[sp + 6] = gm
            seg[sp + 7] = gr
            sp += 8
        year_days[y] += 1


@numba.njit(parallel=True, cache=True, fastmath=True, boundscheck=False)
def evaluate_monotonicity_csr(
    pop,           # (n_pop, F) float32
    ranks,         # (N, F) float32 C-order
    rets,          # (N,) float32
    off,           # (D+1,) int32
    period_idx,    # (D,) int32, 0..n_periods-1 (紧凑周期索引, 粒度由 FITNESS_PERIOD 决定)
    active_day,    # (D,) uint8
    n_periods,     # int
    group_num,     # int
    min_days,      # int, 周期内活跃日数下限, < 此值的周期不计入
    max_cnt,       # int, 预计算的最大日样本数
):
    """
    周期分层单调度 fitness. 调 _accum_year_group (bucket 通用 primitive) 拿到
    period_group 后, 每周期做一次无并列 Spearman: rho = 1 - 6Σd²/(G(G²-1)),
    score_p = (rho+1)/2. fitness = 有效周期 (周期活跃日 >= min_days) 均值 ∈ [0,1].
    """
    n_pop = pop.shape[0]
    G = group_num
    fitness = np.empty(n_pop, dtype=np.float64)
    spearman_denom = float(G * (G * G - 1))  # G>=2 保证 > 0

    for p in numba.prange(n_pop):
        scores = np.empty(max_cnt, dtype=np.float32)
        idx = np.empty(max_cnt, dtype=np.int32)
        seg = np.empty(8 * G + 16, dtype=np.int64)
        period_group = np.zeros((n_periods, G), dtype=np.float64)
        period_days = np.zeros(n_periods, dtype=np.int32)
        _accum_year_group(pop[p], ranks, rets, off, period_idx, active_day, G,
                          scores, idx, seg, period_group, period_days)

        total = 0.0
        n_valid = 0
        for m in range(n_periods):
            if period_days[m] < min_days:
                continue
            ssq = 0.0
            for g in range(G):
                r = 1
                for h in range(G):
                    if period_group[m, h] < period_group[m, g]:
                        r += 1
                    elif period_group[m, h] == period_group[m, g] and h < g:
                        r += 1
                diff = float(r - (g + 1))
                ssq += diff * diff
            rho = 1.0 - 6.0 * ssq / spearman_denom
            total += (rho + 1.0) * 0.5
            n_valid += 1

        if n_valid == 0:
            fitness[p] = 0.0
        else:
            fitness[p] = total / n_valid

    return fitness


def evaluate_monotonicity(pop: np.ndarray, data: dict) -> np.ndarray:
    """周期分层单调度 fitness ∈ [0,1]. 返回 (n_pop,)."""
    pop = np.ascontiguousarray(pop, dtype=np.float32)
    return evaluate_monotonicity_csr(
        pop,
        data["ranks"], data["rets"], data["off"],
        data["period_idx"], data["active_day"],
        data["n_periods"], GROUP_NUM, data["min_period_days"], data["max_cnt"],
    )


@numba.njit(cache=True, fastmath=True, boundscheck=False)
def evaluate_year_group_matrix_csr(
    w,             # (F,) float32, 单个权重
    ranks,         # (N, F) float32 C-order
    rets,          # (N,) float32
    off,           # (D+1,) int32
    year_idx,      # (D,) int32
    active_day,    # (D,) uint8
    n_years,       # int
    group_num,     # int
    max_cnt,       # int
):
    """单个权重 w 的年度档位收益矩阵. 直接包装 _accum_year_group."""
    G = group_num
    scores = np.empty(max_cnt, dtype=np.float32)
    idx = np.empty(max_cnt, dtype=np.int32)
    seg = np.empty(8 * G + 16, dtype=np.int64)
    year_group = np.zeros((n_years, G), dtype=np.float64)
    year_days = np.zeros(n_years, dtype=np.int32)
    _accum_year_group(w, ranks, rets, off, year_idx, active_day, G,
                      scores, idx, seg, year_group, year_days)
    return year_group, year_days


# ==================== 阶段 3: 组合精选 ====================

@numba.njit(cache=True, boundscheck=False)
def _portfolio_metrics_csr(
    stab,     # (K,) float64, 成员稳定性 (NbrY), > 0
    mu,       # (K,) float64, 日均收益
    sig,      # (K,) float64, 日波动 (std), > 0
    cov,      # (K, K) float64, 日收益协方差
    corr,     # (K, K) float64, 日收益相关阵
    N,        # int, 组合成员数
    ann,      # float, 年化因子 sqrt(TRADING_DAYS)
    c,        # (N,) int64, odometer 状态; 调用方初值 [0,1,..,N-1], 跨调用原地推进续跑
    chunk,    # int, 本次最多产出的组合数
    combos,   # (chunk, N) int32, 调用方预分配 (可以是大数组的切片视图), 写 [0, produced)
    mets,     # (chunk, 3) float64, 同上
):
    """
    穷举 C(K, N) 个组合 (字典序 odometer), 每组合算三维原始指标:
        gmNbrY   = gmean(stab)  成员邻域稳定性几何平均
        Sharpe_p = μ_p/σ_p × ann, w = 逆波动率归一, σ_p² = wᵀ Σ w (N² 聚合)
        Orth     = det(成员相关阵) ∈ [0,1] — 标准化日收益向量的 Gram
                   内积体积, 1 = 两两正交; Cholesky 主元连乘, 非正定 → 0
    截面 pct rank / 相乘打分由 Python 侧做 (需全量截面).
    分块续跑: 每次调用最多产出 chunk 个组合就返回 (供调用方展示进度),
    c 原地推进, 下次调用从断点续跑; 计算结果与整跑一次性穷举完全等价.
    返回: (produced 本次实际产出数, done 是否已穷举完毕)
    """
    K = stab.shape[0]
    w = np.empty(N)
    L = np.empty((N, N))
    produced = 0

    while produced < chunk:
        wsum = 0.0
        for a in range(N):
            w[a] = 1.0 / sig[c[a]]
            wsum += w[a]
        for a in range(N):
            w[a] /= wsum

        mu_p = 0.0
        var_p = 0.0
        log_stab = 0.0
        for a in range(N):
            ia = c[a]
            mu_p += w[a] * mu[ia]
            log_stab += np.log(stab[ia])
            for b in range(N):
                var_p += w[a] * w[b] * cov[ia, c[b]]

        # Orth: 相关子阵 Cholesky, det = Π 主元
        for a in range(N):
            for b in range(N):
                L[a, b] = corr[c[a], c[b]]
        det = 1.0
        for a in range(N):
            s = L[a, a]
            for t in range(a):
                s -= L[a, t] * L[a, t]
            if s <= 0.0:
                det = 0.0
                break
            det *= s
            la = np.sqrt(s)
            L[a, a] = la
            for b in range(a + 1, N):
                s2 = L[b, a]
                for t in range(a):
                    s2 -= L[b, t] * L[a, t]
                L[b, a] = s2 / la

        for a in range(N):
            combos[produced, a] = np.int32(c[a])
        mets[produced, 0] = np.exp(log_stab / N)
        mets[produced, 1] = mu_p / np.sqrt(var_p) * ann
        mets[produced, 2] = det
        produced += 1

        # 下一个组合 (字典序)
        a = N - 1
        while a >= 0 and c[a] == K - N + a:
            a -= 1
        if a < 0:
            return produced, True
        c[a] += 1
        for b in range(a + 1, N):
            c[b] = c[b - 1] + 1

    return produced, False


def select_portfolio(top_idx, w_grid, fitness, nbr_means, daily_long, factor_names) -> None:
    """
    阶段 3: 组合精选 — 把 top-K 权重当作 K 条子策略, 解 N 条的小型投资组合.
    收益流 = 阶段 2a 粘性+扣费多头日收益; 有效日 = 全体候选共同 finite 的日子.
    三维原始指标由 _portfolio_metrics_csr 全量算出 (定义见其 docstring),
    此处对每维做全体组合的截面 pct rank, score = 三维分位相乘 —
    任一维平庸都会把乘积压下去, 最优解天然趋向三维均衡.
    """
    K, D = daily_long.shape
    N = PORTFOLIO_N
    n_sets = math.comb(K, N)

    valid = np.isfinite(daily_long).all(axis=0)
    T = int(valid.sum())
    assert T >= 60, f"共同有效交易日太少: {T}"
    R = np.ascontiguousarray(daily_long[:, valid])  # (K, T)

    mu = R.mean(axis=1)
    cov = np.cov(R)  # (K, K), ddof=1
    sig = np.sqrt(np.diag(cov))
    assert (sig > 0).all(), "存在零波动子策略 — 收益流异常"
    stab = np.ascontiguousarray(nbr_means, dtype=np.float64)
    assert np.isfinite(stab).all() and (stab > 0).all(), "NbrY 非法 — 邻居表异常"

    ann = float(np.sqrt(TRADING_DAYS))
    sharpe_each = mu / sig * ann
    corr = cov / np.outer(sig, sig)

    combos = np.empty((n_sets, N), dtype=np.int32)
    mets = np.empty((n_sets, 3), dtype=np.float64)
    c_state = np.arange(N, dtype=np.int64)
    total_produced = 0
    pbar = tqdm(total=n_sets, desc="组合精选 (3)", unit="combo")
    for st, ed in iter_eval_slices(n_sets):
        produced, done = _portfolio_metrics_csr(
            stab, mu, sig, cov, corr, N, ann, c_state, ed - st,
            combos[st:ed], mets[st:ed])
        assert produced == ed - st and done == (ed == n_sets), \
            "组合 odometer 分块续跑状态异常"
        total_produced += produced
        pbar.update(produced)
    pbar.close()
    assert total_produced == n_sets
    assert np.isfinite(mets).all(), "组合指标含非 finite — 收益流异常"

    # 每维截面 pct rank ∈ (0, 1] (升序名次 / 总数; 三维都是越大越好)
    pct = np.empty_like(mets)
    for j in range(3):
        order = np.argsort(mets[:, j], kind="stable")
        r = np.empty(n_sets)
        r[order] = np.arange(1, n_sets + 1, dtype=np.float64)
        pct[:, j] = r / n_sets
    score = pct[:, 0] * pct[:, 1] * pct[:, 2]
    best = int(np.argmax(score))

    print(
        f"\n阶段 3: 组合精选 — top-{K} 穷举 C({K},{N})={n_sets} 个组合, 组合内配比=逆波动率 (风险平价近似)")
    print(f"  收益流: 粘性+扣费多头日收益, 共同有效日 {T}/{D}")
    print(f"  score = 三维截面分位相乘: gmNbrY (稳定) × Sharpe_p (风险调整收益) × Orth (正交)")
    print(f"  Orth = det(成员日收益相关阵) ∈ [0,1] (标准化日收益向量的 Gram 体积, 1=两两正交)")

    mem = combos[best]
    w_rp = 1.0 / sig[mem]
    w_rp /= w_rp.sum()
    pair_corrs = [corr[mem[a], mem[b]]
                  for a in range(N) for b in range(a + 1, N)]
    print(f"\n最优组合 (成员列: #=top 表名次, 配比=逆波动率权重):")
    print(f"score={score[best]:.4f}  "
          f"gmNbrY={mets[best, 0]:.4f} (P{pct[best, 0] * 100:.1f})  "
          f"Sharpe_p={mets[best, 1]:.3f} (P{pct[best, 1] * 100:.1f})  "
          f"Orth={mets[best, 2]:.4f} (P{pct[best, 2] * 100:.1f})  "
          f"平均两两相关={float(np.mean(pair_corrs)):.4f}")
    col_w = [max(len(nm), 5) for nm in factor_names]  # "%+.2f" 权重占 5 字符
    rows = []
    for a in range(N):
        k = int(mem[a])
        gi = int(top_idx[k])
        w_str = ", ".join(f"{v:+.2f}".rjust(cw)
                          for v, cw in zip(w_grid[gi], col_w))
        rows.append(f"      #{k + 1:<4d} 配比 {w_rp[a]:.2f}  Y={fitness[gi]:.4f} NbrY={nbr_means[k]:.4f} "
                    f"Sharpe={sharpe_each[k]:.3f}  [{w_str}]")
    prefix = rows[0].split("[")[0]
    pad = sum(2 if ord(ch) > 0x2E7F else 1 for ch in prefix)  # CJK 占 2 列
    print(" " * pad + "["
          + ", ".join(nm.rjust(cw) for nm, cw in zip(factor_names, col_w)) + "]")
    for row in rows:
        print(row)

    # 组合级统计 (逆波动率配比日收益) + 相关阵
    rp = w_rp @ R[mem]  # (T,)
    nav = np.cumprod(1.0 + rp)
    ann_ret = float(nav[-1] ** (TRADING_DAYS / T) - 1.0)
    ann_vol = float(rp.std(ddof=1) * ann)
    peak = np.maximum.accumulate(nav)
    mdd = float(((nav - peak) / peak).min())
    print(f"\n组合级统计 (逐日再平衡到固定配比, 活跃日口径):")
    print(f"  年化收益={ann_ret * 100:.1f}%  年化波动={ann_vol * 100:.1f}%  "
          f"Sharpe={mets[best, 1]:.3f}  最大回撤={mdd * 100:.1f}%  全期 NAV={float(nav[-1]):.3f}")
    print(f"  成员相关系数矩阵 (顺序同上):")
    for a in range(N):
        row = " ".join(f"{corr[mem[a], mem[b]]:6.3f}" for b in range(N))
        print(f"    {row}")


# ==================== Simplex lattice 搜索 ====================

def generate_simplex_lattice(n_factors: int, m: int) -> np.ndarray:
    """
    M 阶带符号 lattice (L1 球面) 的整数坐标 k, 对应权重 w = k / m:
        { k_i ∈ Z, Σ|k_i| = m } — 负坐标 = 因子反转
        (排序意义上 -w·f ≡ w·(1-f)); 由非负点对非零坐标做 2^nnz
        符号展开得到, 每个带符号点恰好生成一次 (|k| 与符号组合双射).
    stars-and-bars 递归枚举, 返回: (P, n) int32. 浮点权重由调用方自行除 m 得到.
    """
    assert n_factors >= 1 and m >= 1
    points: list[tuple[int, ...]] = []
    k = [0] * n_factors

    def emit() -> None:
        nz = [i for i in range(n_factors) if k[i] != 0]
        for mask in range(1 << len(nz)):
            kk = list(k)
            for b, i in enumerate(nz):
                if (mask >> b) & 1:
                    kk[i] = -kk[i]
            points.append(tuple(kk))

    def rec(i: int, remain: int) -> None:
        if i == n_factors - 1:
            k[i] = remain
            emit()
            return
        for v in range(remain + 1):
            k[i] = v
            rec(i + 1, remain - v)

    rec(0, m)
    return np.asarray(points, dtype=np.int32)


@numba.njit(cache=True, boundscheck=False)
def _check_neighbor_encoding(k_grid, keys, keys_sorted, pw) -> bool:
    """
    一次性 (串行) 校验 lattice key 编码自洽性: 对全体 P 个点的全部单跳邻居,
    验证增量算术得到的 key 都能在 keys_sorted 里精确查到.
    该校验挪到 _neighbor_stats_csr 的 prange 之外单独跑一次 (覆盖全体点,
    比只测 top-K 更严格), 使热路径 kernel 内部无需 assert — prange 循环体
    含 assert 时 numba parfor 会因"多出口"整体放弃并行, 退化为单线程.
    """
    P, n = k_grid.shape
    for v in range(P):
        kv = keys[v]
        for i in range(n):
            ki = k_grid[v, i]
            if ki == 0:
                continue
            base_key = kv - pw[i] if ki > 0 else kv + pw[i]
            for j in range(n):
                if j == i:
                    continue
                kj = k_grid[v, j]
                for dj in (1, -1):
                    if kj > 0 and dj < 0:
                        continue
                    if kj < 0 and dj > 0:
                        continue
                    nk = base_key + dj * pw[j]
                    pos = np.searchsorted(keys_sorted, nk)
                    if pos >= P or keys_sorted[pos] != nk:
                        return False
    return True


@numba.njit(parallel=True, cache=True, boundscheck=False)
def _neighbor_stats_csr(
    k_grid,       # (P, n) int32, lattice 整数坐标
    keys,        # (P,) int64, 每点编码 key = Σ (k_i+M)·(2M+1)^i
    keys_sorted,  # (P,) int64, keys 升序
    idx_sorted,   # (P,) int32, keys_sorted 位置 -> grid 索引
    pw,           # (n,) int64, (2M+1)^i
    centers,      # (C,) int32, 中心点 grid 索引
    max_dist,     # int, BFS 最大跳数
    fitness,      # (P,) float64, 阶段 1 全量 fitness
):
    """
    top-K 邻居敏感度 (阶段 2b), 每中心一个 BFS, prange 并行.
    一跳 = 把 1 单位 |k| 从某非零因子 i 转移到另一因子 j (|k_i|-1, |k_j|+1):
        i 向零收一步 (保号), j 沿自身符号扩一步; j 原为 0 时符号任取.
        均保持 Σ|k| = M, 必然在 lattice 内.
    邻居 key 由当前点 key 增量算术得出, 排序数组二分定位 (无 Python dict).
    返回: (nbr_cnt (C,) int64, nbr_sum (C,) float64)
        = [1, max_dist] 跳内全部邻居 (不含 center) 的个数与 fitness 和.
    """
    P, n = k_grid.shape
    C = centers.shape[0]
    nbr_cnt = np.zeros(C, dtype=np.int64)
    nbr_sum = np.zeros(C, dtype=np.float64)

    for c in numba.prange(C):
        dist = np.full(P, -1, dtype=np.int16)
        queue = np.empty(P, dtype=np.int32)
        v0 = centers[c]
        dist[v0] = 0
        queue[0] = v0
        head = 0
        tail = 1
        acc = 0.0
        m = 0
        while head < tail:
            v = queue[head]
            head += 1
            dv = dist[v]
            if dv >= max_dist:
                continue
            kv = keys[v]
            for i in range(n):
                ki = k_grid[v, i]
                if ki == 0:
                    continue
                base_key = kv - pw[i] if ki > 0 else kv + pw[i]
                for j in range(n):
                    if j == i:
                        continue
                    kj = k_grid[v, j]
                    for dj in (1, -1):
                        if kj > 0 and dj < 0:
                            continue
                        if kj < 0 and dj > 0:
                            continue
                        nk = base_key + dj * pw[j]
                        pos = np.searchsorted(keys_sorted, nk)
                        u = idx_sorted[pos]
                        if dist[u] < 0:
                            dist[u] = dv + 1
                            queue[tail] = u
                            tail += 1
                            acc += fitness[u]
                            m += 1
        nbr_cnt[c] = m
        nbr_sum[c] = acc

    return nbr_cnt, nbr_sum


def iter_eval_slices(total_points: int):
    """
    按总点数自动拆分评估区间, 不使用固定 batch 常量。
    拆分批数 ~= sqrt(total_points), 在进度粒度与评估开销之间做平衡。
    """
    assert total_points >= 1
    n_batches = int(np.sqrt(total_points))
    if n_batches < 1:
        n_batches = 1
    batch_size = (total_points + n_batches - 1) // n_batches
    for st in range(0, total_points, batch_size):
        ed = st + batch_size
        if ed > total_points:
            ed = total_points
        yield st, ed


def run_grid_search(data: dict, top_n: int | None = None) -> tuple[np.ndarray, float, list[str]]:
    """
    两阶段评估:
      1) 全量 lattice 扫 fitness Y = 周期均分层单调度 ∈ [0,1]
      2) Top-K 权重额外算:
         - 邻居平均 Y (L1=2, 参数平原 / 过拟合敏感度)
         - 粘性+扣费口径的全期多头 NAV 和多空 NAV (含逐日多头收益流)
      3) 组合精选: top-K 当子策略, 穷举 N 条组合,
          score = 截面分位(gmNbrY) × 截面分位(Sharpe_p) × 截面分位(Orth)
    返回: (最优权重 (搜索因子维度, 已归一化), 最优 Y, 搜索因子名列表)
    """
    n_years = len(data["years"])
    years = data["years"]

    selected_names = data["factor_names"]
    n_search = len(selected_names)
    print(f"\n搜索因子 ({n_search} 维): {selected_names}")

    k_grid = generate_simplex_lattice(n_search, LATTICE_M)  # (P, n) int32
    w_grid = (k_grid.astype(np.float32) /
              np.float32(LATTICE_M))  # (P, n) float32
    print(
        f"带符号 lattice M={LATTICE_M} (无方向先验, 负权重=因子反转), "
        f"点数={len(w_grid)}, 步长=1/{LATTICE_M}={1.0 / LATTICE_M:.4f}")

    fitness = np.empty(len(w_grid), dtype=np.float64)
    pbar = tqdm(total=len(w_grid),
                desc=f"{FITNESS_PERIOD} 单调度搜索", unit="point")
    for st, ed in iter_eval_slices(len(w_grid)):
        fitness[st:ed] = evaluate_monotonicity(w_grid[st:ed], data)
        pbar.update(ed - st)
    pbar.close()

    n_top = TOP_N if top_n is None else top_n
    assert n_top >= 1
    top_k = min(n_top, len(w_grid))
    top_idx = np.argsort(fitness)[-top_k:][::-1]

    # Top-K 粘性+扣费 NAV (累计 + 年均, 多空 + 多头) + 逐日多头收益流 (阶段 3 用)
    top_w = w_grid[top_idx]
    D = data["active_day"].shape[0]
    top_nav = np.empty((top_k, 4), dtype=np.float64)
    top_daily = np.empty((top_k, D), dtype=np.float64)
    pbar = tqdm(total=top_k, desc="NAV 复评 (2a)", unit="point")
    for st, ed in iter_eval_slices(top_k):
        top_nav[st:ed], top_daily[st:ed] = evaluate_batch(top_w[st:ed], data)
        pbar.update(ed - st)
    pbar.close()

    # 邻居敏感度 (邻居 = [1, NEIGHBOR_DISTANCE_MAX] 跳内全部点):
    # k 编码为 int64 key (基 2M+1, 双射), BFS kernel 对 top-K 中心 prange 并行
    assert 1 <= NEIGHBOR_DISTANCE_MAX <= 32767  # dist 为 int16
    pw = np.int64(2 * LATTICE_M + 1) ** np.arange(n_search, dtype=np.int64)
    keys = (k_grid.astype(np.int64) + LATTICE_M) @ pw
    sort_ord = np.argsort(keys)
    keys_sorted = keys[sort_ord]
    assert _check_neighbor_encoding(k_grid, keys, keys_sorted, pw), \
        "lattice key 编码校验失败 — 邻居增量算术与 keys_sorted 不自洽"
    idx_sorted = sort_ord.astype(np.int32)
    centers = top_idx.astype(np.int32)
    nbr_counts = np.empty(top_k, dtype=np.int64)
    nbr_sums = np.empty(top_k, dtype=np.float64)
    pbar = tqdm(total=top_k, desc="邻居敏感度 (2b)", unit="center")
    for st, ed in iter_eval_slices(top_k):
        nbr_counts[st:ed], nbr_sums[st:ed] = _neighbor_stats_csr(
            k_grid, keys, keys_sorted, idx_sorted,
            pw, centers[st:ed], NEIGHBOR_DISTANCE_MAX, fitness)
        pbar.update(ed - st)
    pbar.close()
    nbr_means = np.where(nbr_counts > 0,
                         nbr_sums / np.maximum(nbr_counts, 1), np.nan)
    decays = fitness[top_idx] - nbr_means

    # 衰减升序 rank -> 1..STAR_LEVELS 星 (最低衰减档 = STAR_LEVELS 星 = 最平原, 最高衰减档 = 1 星 = 最山尖)
    assert STAR_LEVELS >= 1
    order = np.argsort(decays)
    d_rank = np.empty_like(order)
    d_rank[order] = np.arange(top_k)
    star_counts = np.empty(top_k, dtype=np.int32)
    for i in range(top_k):
        b = int(d_rank[i] * STAR_LEVELS / max(top_k, 1))
        if b > STAR_LEVELS - 1:
            b = STAR_LEVELS - 1
        star_counts[i] = STAR_LEVELS - b

    skip_disp = sorted(SKIP_MONTHS) if SKIP_MONTHS else "无"
    pct_per_bin = 100.0 / STAR_LEVELS
    star_w = STAR_LEVELS
    nbr_col_w = max(3, len(str(int(nbr_counts.max()))) if top_k > 0 else 3)
    print(
        f"\nTop {top_k} 结果 (搜索因子顺序: {selected_names}, SKIP_MONTHS={skip_disp}):")
    print("列含义:")
    print("  #        : 在 lattice 内按 Y 降序的名次")
    print(
        f"  Y        : {FITNESS_PERIOD}均分层分档单调度 fitness ∈ [0,1] (1.0 = 每{FITNESS_PERIOD} Q1..QG 都完美单调, 周期活跃日>={MIN_PERIOD_DAYS[FITNESS_PERIOD]} 方计入)")
    print(f"  NbrY     : [1,{NEIGHBOR_DISTANCE_MAX}] 跳内全部邻居权重的 Y 均值 (扰动稳定性)")
    print("  衰减     : Y - NbrY, 越接近 0 越抗过拟合 (山尖 vs 平原)")
    print(
        f"  星       : 衰减在 top-N 内升序 {STAR_LEVELS} 分位 ({STAR_LEVELS}★=衰减最低 {pct_per_bin:.1f}% 最平原, 1★=最高 {pct_per_bin:.1f}% 最山尖)")
    print(f"  N        : [1,{NEIGHBOR_DISTANCE_MAX}] 跳内邻居总个数 (边界点会减少)")
    print(
        f"  多头累计 : 粘性+扣费 (cost_rt={COST_ROUND_TRIP}) 下 top bucket 全期多头 NAV (起点 1.0)")
    print(f"  多头年均 : 逐年重置 NAV, 各年年末 NAV 算术平均 (1.0 = 当年持平)")
    print(f"  多空累计 : 粘性+扣费 下 top-bottom 多空 NAV (起点 1.0)")
    print(f"  多空年均 : 逐年重置多空 NAV, 各年年末算术平均")
    print(f"  权重     : 搜索因子维度上的权重 (顺序同上, Σ|w|=1; 负 = 因子反转, 按 pct_rank 反向)")
    header = f"{'#':<3} {'Y':>7} {'NbrY':>7} {'衰减':>7} {'星':<{star_w}} {'N':>{nbr_col_w}} {'多头累计':>8} {'多头年均':>8} {'多空累计':>8} {'多空年均':>8}  权重"
    print(header)
    print("-" * len(header))
    for rank, gi in enumerate(top_idx, 1):
        i = rank - 1
        w_str = ", ".join(f"{v:+.2f}" for v in w_grid[gi])
        stars = "*" * int(star_counts[i])
        nav_ls_cum = float(top_nav[i, 0])
        nav_l_cum = float(top_nav[i, 1])
        nav_ls_avg = float(top_nav[i, 2])
        nav_l_avg = float(top_nav[i, 3])
        print(
            f"{rank:<3} {fitness[gi]:7.4f} {nbr_means[i]:7.4f} {decays[i]:+7.4f} "
            f"{stars:<{star_w}} {int(nbr_counts[i]):{nbr_col_w}d} "
            f"{nav_l_cum:8.3f} {nav_l_avg:8.3f} {nav_ls_cum:8.3f} {nav_ls_avg:8.3f}  [{w_str}]"
        )

    # 最优权重的年度各档累计收益表 (干净等权, 算术累加)
    best_idx = int(top_idx[0])
    best_weights = w_grid[best_idx].copy()
    best_fitness = float(fitness[best_idx])
    yg, yd = evaluate_year_group_matrix_csr(
        best_weights,
        data["ranks"], data["rets"], data["off"],
        data["year_idx"], data["active_day"],
        n_years, GROUP_NUM, data["max_cnt"],
    )
    print(f"\n最优权重年度各档累计收益 (干净等权, 算术累加, 未扣费; SKIP_MONTHS={skip_disp} 不计入):")
    print(f"  Q1..Q{GROUP_NUM} = 按 score 升序切分的分位档 (Q1=最低分组, Q{GROUP_NUM}=最高分组)")
    print(f"  单元格 = 该档全年所有活跃日 (剔除 SKIP 月) 等权日收益的算术累加")
    hdr = "年份 " + " ".join(f"Q{g + 1:<6}" for g in range(GROUP_NUM)) + "  单调度"
    print(hdr)
    for y in range(n_years):
        if yd[y] < GROUP_NUM:
            continue
        row_vals = yg[y]
        # 单独再算一次该年单调度用于展示
        ranks_sorted = np.argsort(np.argsort(row_vals))  # 0..G-1 秩
        ssq = float(np.sum((ranks_sorted - np.arange(GROUP_NUM)) ** 2))
        rho = 1.0 - 6.0 * ssq / (GROUP_NUM * (GROUP_NUM ** 2 - 1))
        y_score = (rho + 1.0) * 0.5
        cells = " ".join(f"{v:+.4f}" for v in row_vals)
        print(f"{years[y]} {cells}  {y_score:.3f}")

    # 阶段 3: 组合精选 (稳定性 × Sharpe × 分散比, 相乘)
    select_portfolio(top_idx, w_grid, fitness, nbr_means,
                     top_daily, selected_names)

    return best_weights, best_fitness, selected_names


# ==================== 主程序 ====================

def main():
    skip_disp = sorted(SKIP_MONTHS) if SKIP_MONTHS else "无"
    print("=" * 60)
    print("Simplex lattice 因子权重搜索")
    print(
        f"目标: {FITNESS_PERIOD}均分档单调度 Y ∈ [0,1] (Q1..Q{GROUP_NUM}, Spearman (rho+1)/2; 周期活跃日>={MIN_PERIOD_DAYS[FITNESS_PERIOD]} 才计入)")
    print(f"Top{TOP_N} 复评: 粘性+扣费 多头/多空 NAV, cost_rt={COST_ROUND_TRIP}; 邻居 L1=2 敏感度")
    print(f"SKIP_MONTHS={skip_disp} (命中日 fitness 与 NAV 均跳过, 持仓冻结)")
    print(
        f"numba 线程数: {numba.get_num_threads()} (prange 并行: 阶段1 pop / 2a pop / 2b centers)")
    print("=" * 60)

    data = load_data()

    print("预热 JIT...")
    _dummy = np.zeros((1, len(data["factor_names"])), dtype=np.float32)
    _dummy[0, 0] = 1.0
    evaluate_batch(_dummy, data)
    evaluate_monotonicity(_dummy, data)

    best_weights, best_fitness, selected_names = run_grid_search(data)

    print("\n" + "=" * 60)
    print("最终结果")
    print("=" * 60)
    print(f"最优{FITNESS_PERIOD}均分层单调度 Y: {best_fitness:.4f}")
    print(f"\n最优权重 (Σ|w|=1, 负 = 因子反转):")
    for name, w in zip(selected_names, best_weights):
        if w != 0:
            print(f"  {name:20s}: {w:+.4f}")
    if (best_weights < 0).any():
        print("  注: 含负权重, 不能直接填 config::STRATEGY_FACTOR_WEIGHTS (cpp 要求 w > 0);")
        print("      需 cpp 侧为该因子翻转 pipeline invert 标志后取 |w|, 或让 cs_factor_score 支持负权.")
    else:
        print("  (全非负, 可直接填 config::STRATEGY_FACTOR_WEIGHTS)")

    return {
        "selected_factors": selected_names,
        "weights": {name: float(w) for name, w in zip(selected_names, best_weights) if w != 0},
        "mean_monotonicity": float(best_fitness),
    }


if __name__ == "__main__":
    main()
