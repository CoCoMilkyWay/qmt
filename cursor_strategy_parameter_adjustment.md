# Strategy parameter adjustment
_Exported on 8/20/2026 at 10:08:34 GMT+8 from Cursor (3.16.17)_

---

**User**

@README.md 

@py/app/mine.py  改造一下， 去掉繁杂的定义， 直接定向到一个策略， 直接继承那个策略的参数（读取那个策略dump的tensor ）， 回测机制也要改， 保证开关舱和CPP一样， 一些影响不大的细节可以适当简化（减少计算量）

我希望算出来的weights， 放到cpp里， 跑出来的NAV的CAGR和sharpe 99%能对上

同时还要能够做到急速计算， 因为我们有几百万个组合需要计算

给方案， 是直接在脚本上改好还是干脆做成C++能有丰厚回报？

---

**Cursor**

我先把 README 和 `mine.py` 完整读一遍，再看 C++ 侧回测与策略定义的实现细节。

现在读 C++ 回测引擎和策略定义，看开关舱的确切口径。

看策略定义、`columns.cpp` 选股口径、config 常量和张量导出格式。

读 config 常量、NAV 指标定义、`pct_rank` 语义和 registry 当前状态。

研究完了，先说结论：**值得做成 C++**，而且回报主要不在速度，在"正确性可以被结构保证"。

## 先说一个必须先摆平的事实：现在 `mine.py` 的口径和 C++ 差两层

不是小偏差，是排序结果本身就不同：

1. **score 口径错**。`mine.py` 的 `flat_ranks` 直接取张量里的 factor 值（`cs.cpp::factor_pipeline` 的产物 = **全市场**截面分位），然后 `ranks @ w`。但 C++ `columns.cpp::cs_score` 是先把池外置 NaN、**在 pool 内重做一次 `pct_rank`** 再加权（那段注释专门解释了为什么必须这么做：小市值池在 mcap 上是左尾偏斜子集，全市场分位在池内被压成窄带，权重比例会失真）。F>1 时 `Σw·f` 与 `Σw·pctrank_pool(f)` 的排序不同 ⇒ 选出来的股票就不是同一批。`mine.py` 顶部注释声称"与 cpp cs_score 完全同口径"，这句是错的。
2. **回测机制错**。`mine.py` 是按 score 切 5 档、多头档等权、多空对冲、按换手率乘千2扣费；C++ 是 `hold_n=10` 的持仓状态机：现金 + 分数股 + `target_per_slot = pv_after/hold_n` + 亏空排序扫现金 + 涨跌停/停牌的开关舱约束 + 退市强平。两者的 CAGR/Sharpe 没有任何理由对得上。
3. fitness 是分层单调度 Y，不是 NAV 的 CAGR/Sharpe。

所以无论走哪条路，评估核心都得重写。既然要重写，就要选一个能被验证的写法。

## 关键发现：这件事可以做到"结构上精确"，而不是"调到 99%"

三个观察让整件事变得便宜且可证明：

**A. pool 内分位与权重无关，可以只算一次。** `pct_rank_pool(f)` 只依赖 pool 和 factor，不依赖 w。所以预计算一张 `PR[d][i][f]`（日 × 池内成员 × 因子），之后每个权重只是一次点积。2339 天 × 2343 只 × 13 因子 × 4B = **285 MB**，全线程只读共享。

**B. 每天只需要前 20 名，不需要全排序。** 读 `backtest.cpp` 的买入循环：它按 rank 序遍历 `cands`，`if (!top_n_set.count(a)) break;` —— `top_n_set` 就是前 10 名，所以循环最晚在第 11 个位置断掉，实际只看 `cands[0..9]`；卖出侧要的 `top_exit_set` 是前 20。**前 20 名的精确顺序就是全部所需**，2343 只里选 20 个用阈值预筛 + 小堆，比全排序快一个量级。

**C. `Σ|w|` 归一化可以绕过，但要绕对。** 除以正常数不改顺序，但 float32 除法可能把两个不同的 acc 合并成相等值，而 C++ 那边此时会用"a 升序"破序 ⇒ 极小概率错位。做法：先用未归一的 acc 选出前 40，只对这 40 个做 `acc/wsum`（和 C++ 同一个除法），再按 `(score desc, a asc)` 重排取前 20。代价可忽略，结果和 C++ 逐位一致。

## 方案

**第 1 步：把 `backtest.cpp` 的决策+记账内核抽出来（这是"丰厚回报"的来源）**

现在那 400 行把 NAV 状态机和十几路报表埋线（fills/trades/watch/names/report.json）缠在一起。抽成 `backtest/engine.hpp`：内核用 Recorder policy 参数化，`FullRecorder` 给 `backtest::run` 出报表，`NullRecorder` 给挖掘用（钩子全部编译期消掉，零开销）。内核只吃：窗口、市场状态、以及"每日前 K 名候选"。

这一步做完的验收闸门：五个策略的 `output/strategy/*/backtest/strategy_nav.npy` **逐位不变**。不满足就是重构写错了。

**第 2 步：新增 `cpp/src/mine/`**

- `mine/spec.hpp`：目标策略名 + 搜索因子清单 + lattice 阶 M。pool/filters/`bt_start_date`/`hold_n`/`exit_ratio`/`universe_size` 全部从那个 `StrategySpec` 继承，不再重复定义（对应你说的"去掉繁杂的定义"）。
- 预计算一次：per-day pool CSR（成员 `a` 升序，天然满足 a-升序破序）+ `PR` 矩阵（复用 `feature::pct_rank`，和 `cs_score` 同一份代码）+ `last_close` / status（susp/limit_up/limit_dn/delist 压成一个 uint8，`n_a × n_d_bt` = 13.6 MB）。
- 带符号 simplex lattice 枚举（照搬 `generate_simplex_lattice` 的逻辑）。
- 72 线程分片：点积 → 前 20 → 内核跑 NAV → `report::nav_stats`（CAGR/Sharpe/回撤和报表同一个函数）→ 落 `output/mine/{k_grid.npy, metrics.npy}`。
- 挂在 `main.cpp` 的 `config::MINE_ENABLE` 后面，和 Phase 2.5/2.6 一个形状。

关键好处：不依赖 3.8 GB 张量 dump（在进程内直接读 Tensor），而且 `pool` 列现在根本没导出（`describe.cpp` 里 `if (col == pool) continue`），Python 侧只能拿 `rank>0` 反推。

**第 3 步：`py/app/mine.py` 瘦身成后处理器**

删掉 `load_data` / CSR / 分层单调度 kernel / 粘性NAV kernel（约 700 行），只留：读 `output/mine/*.npy` → 邻居 BFS 平原度（只查 fitness 数组，本来就不碰数据）→ 组合精选 → 打印。这些都是毫秒级的，留在 Python 最舒服。

## 可以安全砍掉的（省算力，不影响结果）

- pool 等权影子指数 `ew()` —— 每天 `O(n_a)` 全表扫，是当前回测里最贵的一段，挖掘阶段完全不需要
- 每天 `O(n_a)` 刷 `last_close` → 预计算一次（注意要从 `bt_d_lo` 起做 ffill，和 C++ 的初始全 NaN 语义一致）
- 两趟扫 `n_a` 构 `cands` 全表 + `rank_today`/`rank_win` → 换成前 20 选择
- watch / fills / trades / labels.json / report.json / 退市 `[WARN]` printf

**不能砍**（砍了就别想对上）：分数股 + 买卖成本 + `target_per_slot` + 亏空排序扫现金；涨停不卖/跌停不卖/涨停不买/跌停不买；停牌挡卖出；退市强平；`(score desc, a asc)` 破序；pool 内重排分位。

## 性能预估（72 核实测规格）

窗口 2339 天，池均 2343 只：

| 搜索因子 | M | lattice 点数 | 预估耗时 |
|---|---|---|---|
| 8 | 10 | 658 K | ~25 s |
| 8 | 12 | 2.19 M | ~75 s |
| 13 | 8 | 6.07 M | ~4 min |

（单点 ≈ 2339 天 × ~3000 cycle ≈ 2–3 ms/核。numba 大致 3–6 倍慢，也能跑，但见下。）

## 为什么不建议留在 numba

速度不是决定性的（6M 点 numba 大概 15–25 分钟，能接受）。决定性的是：留在 Python 就是**第二份实现**，你那 99% 会变成一个需要反复对账追查的目标——而走 C++ 共享内核，"用策略自己的权重跑挖掘器，结果必须逐位等于 `strategy_nav.npy`" 是一句 assert，一次性把问题关掉，以后改回测逻辑两边自动同步。这也是 README 里 单点真理 那套规范本来的要求。

## 需要你定的几件事

> **实现路径**
> - C++ + 抽内核共享（推荐）：先重构 backtest 内核并用 strategy_nav.npy 逐位验证，再写 mine 模块，Python 只做后处理 ✔
> - C++ 但不动 backtest.cpp：mine 里另写一份内核，靠对账 strategy_nav.npy 验证（风险低但两份实现，违背单点真理）
> - 仍留在 mine.py（numba）：修好 pool 内分位 + 重写成 top-N 持仓状态机，接受人工对账追 99%

> **挖掘目标策略（pool/filters/hold_n/exit_ratio/bt_start_date 全部从它继承）**
> - 低价小市值（当前 mine.py 的 STRATEGY，基线）
> - 低净小市值
> - 低值小市值
> - 融资小市值
> - 做成 mine/spec.hpp 里一个可改的策略名常量，先填低价小市值 ✔

> **搜索因子清单（决定 lattice 规模和耗时）**
> - 张量里全部 13 个 factor（mcap/fmcap/close/cp/sp/cffoa/bp/ep/roe/roa/dy/mr_bal/ms_bal）
> - 沿用 mine.py 现在的 8 个（bp/ep/sp/cp/close/fmcap/mcap/dy）
> - 只搜目标策略自己 weights 里的那几个因子（低价小市值只有 3 个，空间很小）
>
> Answer: mine作为一个开关， 如果开了， 就忽视那个策略的因子依赖， 转而用mine定义的因子bucket做search， （其他配置完全继承）

> **排序用的 fitness（挖掘输出会把 CAGR/Sharpe/波动/回撤/换手全部落盘，这里只是决定按谁排序取 top-N）**
> - Sharpe
> - CAGR（年化收益）
> - 两个都算，排序按 Sharpe，表里两列都打印，最终由邻居平原度+组合精选定夺
> - 另外保留分层单调度 Y 作为第三个稳健性指标（要额外算，会明显变慢）
>
> Answer: 这些部分先 leave for implementation， 我有改进的想法不要原样搬

方案定稿了。把你的四个决定落成文件级计划：

## Step 1 — 抽 backtest 内核（唯一有风险的一步）

`cpp/include/backtest/engine.hpp`（新）：把 `backtest.cpp` 主循环里的**决策 + 记账**搬进来，用 Recorder policy 参数化：

- `FullRecorder` — `backtest::run` 用，钩子里做 fills/trades/watch/holdings CSR/names 那一整套
- `NullRecorder` — mine 用，钩子全空，编译期消掉

钩子面：`on_delist_close` / `on_sell` / `on_buy` / `on_rebalance_add` / `on_day_end`。内核输出只有 `strategy_nav` + `turnover`；pool 影子指数（每天 `O(n_a)` 全表扫）留在 `run` 里，不进内核。

验收闸门（你跑）：五个策略的 `output/strategy/*/backtest/strategy_nav.npy` 逐位不变。

## Step 2 — `cpp/{include,src}/mine/`

`mine/spec.hpp`：
```cpp
inline constexpr bool MINE_ENABLE = false;
inline constexpr std::string_view MINE_STRATEGY = "低价小市值";  // pool/filters/hold_n/exit_ratio/bt_start_date 全部从它继承
inline constexpr const feature::FeatureSpec *MINE_FACTORS[] = { /* 搜索用因子 bucket, 完全独立于该策略的 weights */ };
inline constexpr int MINE_LATTICE_M = 10;
```

`feature/registry.hpp` 加第三类根：`MINE_ROOTS = MINE_ENABLE ? MINE_FACTORS : {}`（consteval 三元，关掉时不进计算图、不占 Tensor）。这样 mine 的因子 bucket 想放什么就放什么，不受目标策略的因子依赖约束——正是你要的语义。

`mine/mine.cpp` 干四件事：
1. 预计算一次：per-day pool CSR（成员 a 升序）+ `PR[d][i][f]`（复用 `feature::pct_rank`，和 `cs_score` 同一份代码）+ `last_close`/status 矩阵。PR 用 `MINE_FACTORS ∪ 目标策略自己的 weight 因子`，后者只为第 3 条。
2. **自检**：先用目标策略自己的 weights 走一遍 mine 管线，assert NAV 逐位等于 `backtest::run` 的 `strategy_nav`。这条 assert 就是那个"99%"的替代品——过了就是 100%。
3. 带符号 lattice 枚举 + 72 线程扫：点积 → 阈值预筛取前 40 → 只对这 40 个做 `acc/Σ|w|` 除法 → `(score desc, a asc)` 重排取前 20 → 内核跑 NAV → `report::nav_stats`。
4. 落 `output/mine/{k_grid.npy, factor_names.json, metrics.npy}`。metrics **不烘死 fitness**，把 CAGR / Sharpe / 年化波动 / 最大回撤 / 平均换手 / 有效天数逐列摊开，排序和选择逻辑留给你。

挂在 `main.cpp` 里 `MINE_ENABLE` 后面，和 Phase 2.5/2.6 同形状。

## Step 3 — 暂缓

按你说的，`mine.py` 现有的 Y / 邻居平原度 / 组合精选不原样搬。这一步等你给改进后的 fitness 想法，我再把 `mine.py` 削成读 `output/mine/*` 的薄壳。C++ 侧输出保持中立，不预设。

## 一个需要你拍的问题

`strategy/def/diagnostic.hpp`（"对账诊断"）的存在理由就是把掉出计算图的孤儿因子（ep/roe/roa/dy/mr_bal/ms_bal）拉回来算好落张量。`MINE_ROOTS` 一上线，这个职责就被顶替了。

> **MINE_ROOTS 上线后，“对账诊断”策略怎么处理**
> - 原样保留（它还在给 test/compare.py 对账，不动最安全）
> - 从 STRATEGIES[] 删掉，孤儿因子改由 MINE_FACTORS 拉进计算图（少一个假策略，但 MINE_ENABLE=false 时这些因子就不再落张量，会弄坏 compare.py） ✔
> - 先保留，mine 跑通了再回头清

> **MINE_FACTORS 先填什么（后面随时改，只是第一版默认值）**
> - 当前张量里全部 13 个 factor ✔
> - mine.py 现在的 8 个（bp_ttm3/ep_ttm12/sp_ttm12/cp_ttm12/close/fmcap/mcap/dy_ttm12）
> - 先留空数组 + 注释模标，由你自己填

`MINE_FACTORS` 我就不用 `MINE_ENABLE` 门控了——无条件当计算图根。这样 13 个因子永远落张量，`compare.py` 不受影响，`对账诊断` 可以干净删掉，而 `MINE_ENABLE` 只管 lattice 扫描那一段。代价和今天一样（diagnostic 本来就在算这 6 个）。

先把要动的文件读全。

开始实施。先建内核。

Now `engine.cpp`（`load_market`）：

Now改接 `backtest.cpp`。

Now把主循环里的决策段整体换成 `Engine::step` + Recorder：

Now把循环收尾段里对旧状态的引用改到 `eng` / `mk`：

Now把 `MINE_FACTORS` 接进计算图根：

Now删掉「对账诊断」策略，改掉那两个因子的注释：

Now the mining module itself.

Now hook it into `main.cpp` and the build.

Now the Python side becomes a thin post-processor over the C++ output.

方案落地完成：**挖掘内核搬到 C++，Python 只做后处理**。这样"填回 cpp 能对上"不是调参调出来的，而是结构上就是同一份代码。

## 改了什么

**回测内核抽出来共享**（新 `cpp/{include,src}/backtest/engine.*`）
`MarketWindow`（窗口内 last_close + 停牌/涨跌停/退市四个标志压成一字节）+ `Engine::step()`，把现金记账、分数股、买卖成本、`target_per_slot` 再平衡现金清扫、涨跌停开关舱约束、停牌挡卖、退市强平全收进去。报表路径和挖掘路径的差别只剩两点：候选序怎么来、`Recorder` 记什么（报表路径用 lambda 组装的 `Recorder`，挖掘路径 `NullRecorder` 编译期消掉）。`backtest.cpp` 已改接到这个内核，唯一语义变化是持仓表从 `unordered_map` 换成插入序稳定的 `vector<Pos>`（浮点累加顺序不再依赖哈希桶，两条路径逐位一致）。

**挖掘模块**（新 `cpp/{include,src}/mine/`）
`mine/spec.hpp` 是唯一挂载点：`MINE_ENABLE`（现在 false）、`MINE_STRATEGY = "低价小市值"`、`MINE_FACTORS`（13 个 factor）、`MINE_LATTICE_M = 8`。目标策略的 pool/filters/hold_n/exit_ratio/窗口全部继承，只搜 weights。

三个关键点让它够快（M=8 是 6,065,410 个权重方向）：
- pool 与权重无关，所以每日每因子的**池内分位预计算一次**（CSR 布局，因子主序），逐权重只剩一次 axpy。
- 内核只读候选前缀，所以每天只需选出 top-`max(hold_n×exit_ratio, 2×hold_n)` 名（定长插入选择，绝大多数候选一次比较即被拒），不做全排序。这个上界是从 `step()` 的买入循环推出来的，截断与传全量候选逐位等价。
- 循环改成**外层日、内层权重**（每批 256 个）。一个日块约 100KB 常驻 L2 跨整批复用，DRAM 流量降到 1/256 —— 不这么做的话内存带宽会成为瓶颈，算力反而不是。

排序口径与 `columns.cpp::cs_score`/`cs_rank` 逐位同构：分位是池内重排的、累加顺序与 `spec.weights` 遍历顺序一致、归一化除 `Σ|w|` 之后才按 `(score 降序, a 升序)` 破序（除法保序但不保严格序，所以留了 `TIE_MARGIN=16` 余量，边界仍并列则 assert）。

**自检**：启动先拿目标策略自己的 weights 走一遍挖掘管线，与刚跑完的 `backtest::run` 的 `strategy_nav` 逐点对账，`assert(最大相对偏差 < 1e-9)`。预期是逐位相同；过不了就说明管线有分歧，直接 fail 而不是留个"99% 差不多"。

**Python 端**（`py/app/mine.py`，1308 行 → 约 190 行）
只读 `output/mine/{k_grid.npy, metrics.npy, meta.json}`，按 `fitness()` 排序、算邻域平原度（lattice 上 1 跳邻居查表，不重算）、打印可直接粘回 `strategy/def/*.hpp` 的 weights —— 写成精确的 `-3.0f / 8` 分数形式，编译期常量折叠与挖掘端的 `(float)k / (float)M` 逐位相同，所以粘回去必然复现。`fitness()` 按你的要求只放了个保守默认（夏普 + 换手/回撤硬约束），标了注释留给你改。

## 需要你做的

`cpp/include/mine/spec.hpp` 把 `MINE_ENABLE` 改 true 后编译运行。粗估单权重 3–5ms，72 核约 5–7 分钟跑完 6.07M 个方向；落盘 `k_grid.npy` 79MB + `metrics.npy` 170MB。第一次跑请先看自检那行的输出（`[mine] 自检: NAV 逐点最大相对偏差 ...`）—— 我没编译过，assert 和编译错误都要靠这一跑暴露。

M 的代价：M=6 → 45 万点（约半分钟），M=10 → 5400 万点（约 1 小时，且 k_grid 会到 700MB）。建议先用 M=6 验证整条链通，再上 M=8。

