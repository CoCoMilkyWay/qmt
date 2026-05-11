Motive: 实盘量化交易; 国金证券 QMT 客户端下单, tushare 维护本地数据库

# 项目结构

```
qmt/
├── run.py                           # 统一入口: build (py/main.py) + run (py/mode_*.py)
├── app/
│   ├── gjzqqmt/                     # 国金证券 QMT 客户端 (Linux Wine 跑 Windows 程序)
│   │   ├── run.md                   # Wine 安装 + 启动指南 (XtItClient=主端, XtMiniQmt=API端)
│   │   ├── 国金证券QMT交易端/       # 客户端本体 (bin.x64 是 64 位主程序)
│   │   └── QMT操作说明文档/         # 官方 PDF (操作/Python API/网格/VBA/算法交易)
│   └── api/tushare/                 # tushare Python SDK 副本 (历史保留, 不再用)
├── cpp/                             # C++23 实现 (Clang/Linux, header-only boost + yyjson)
│   ├── projects/main/               # CMake 构建 (DEBUG / PROFILE / ASSERT / PRODUCTION)
│   ├── include/
│   │   ├── config.hpp               # 全局常量 (token, API host, lookback, 拉取窗口, 去重窗口)
│   │   ├── misc/                    # 通用工具 (date / fs / logging / progress / timer / affinity)
│   │   ├── package/yyjson/          # JSON 库
│   │   ├── tushare/                 # tushare 子系统头文件 (数据入)
│   │   └── feature/                 # feature 子系统头文件 (张量出)
│   └── src/
│       ├── main.cpp                 # tushare::update → feature::build → Tensor T[F][A][D]
│       ├── tushare/
│       │   ├── http.cpp             # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│       │   ├── spec.cpp             # 14 个 SPECS + RangeStrategy / PerDayStrategy
│       │   ├── store.cpp            # scan_missing / write_by_visible_date (PK upsert + _empty.json)
│       │   ├── meta.cpp             # refresh_stock_basic + refresh_index_member_all + 单 itf 去重 (lastupdate 时间戳, 粒度=spec.name)
│       │   └── pipeline.cpp         # scan → plan → fetch → write 主流程 (入口逐 itf 走 lastupdate 去重)
│       └── feature/                 # 4-phase 特征系统; 业务密集化 + 外层 flow 完全 agnostic
│                                    # 单点真理: pit.cpp (itf 维) + feature.cpp (feature 维)
│           ├── axis.cpp             # Phase 0: load_axes (D=SSE∪SZSE 交易日) + load_stock_meta (per-A 静态)
│           ├── feature.cpp          # 【单点真理 feature】每 feature 一个 ts_xxx/cs_xxx compute fn + 末尾 FEATURES[] 表挂载
│           │                        # F 枚举顺序 = FEATURES[] 索引 = 计算顺序 (后段读已写就的 T.ts_row(prior_f, a))
│           ├── tensor.cpp           # Tensor 容器 (统一 [F][A][D] layout, ts_row 连续, gather/scatter cs_row)
│           ├── pit.cpp              # 【单点真理 itf】每 itf 一个 namespace block (prealloc + parse + post_sort) + 末尾 ITFS[] 表挂载
│           ├── load.cpp             # Phase 1 通用 flow: 仅迭代 ITFS[] (prealloc → enumerate → 并行 parse → post_sort), 不出现 itf 名
│           ├── ts.cpp               # Phase 2 通用 flow: per-A 并行, 迭代 FEATURES[] 中 axis==TS 的 compute_ts 调; kernel 在 ts.hpp (ttm4_ytd_compute / state_machine_intervals 模板)
│           ├── cs.cpp               # Phase 3 通用 flow: per-D 并行, 迭代 FEATURES[] 中 axis==CS 的 compute_cs 调; kernel (winsor_mad / z / pct_rank / factor_pipeline) 在 cs.hpp/cpp
│           └── build.cpp            # 编排入口: 串 4 phase + misc::Timer 报段时
├── data/                            # tushare 落地 (按 visible_date 切日, gitignored)
│   ├── _meta/
│   │   ├── stock_basic.json         # 全局 meta: ts_code 全量 (L+D+P+G), 每次 update 覆盖刷新
│   │   ├── index_member_all.json    # 申万 SW2021 行业成分 (is_new=Y), 按 L1 分批合并 (PK=ts_code)
│   │   └── <name>.lastupdate        # 单 itf 去重时间戳 (unix epoch s, name=数据文件名); 上次成功距今 < config::API_DEDUP_WINDOW_SECONDS 跳过
│   └── YYYY/
│       └── MM/
│           ├── _empty.json          # 反向稀疏标记 {itf: [DD,...]} = 拉过且为空
│           └── DD/<itf>.json        # 仅在该天有数据时存在 (PK 唯一, 路径 = visible_date)
│                                    # 三态: file 存在 / 在 _empty / 都不在 = 有数据 / 拉过空 / 未拉
│                                    # itf ∈ {forecast, express, disclosure, report, stock_st, calendar,
│                                    #        dividend, daily_basic, adj_factor, stk_limit, suspend_d,
│                                    #        fina_indicator, income, cashflow,
│                                    #        margin_secs, margin_detail, st(归档,不入张量)}
├── py/                              # 构建/运行模式 (run.py 调用)
│   ├── main.py                      # CMake 配置 + 编译
│   └── mode_{debug,profile,assert,production}.py
└── doc/
    ├── research/                    # 数据研究脚本
    │   ├── analysis.py              # 覆盖率分析 (按财季+发布偏移月统计 D/F/E)
    │   └── analysis.md              # 分析结果
    └── tushare/                     # tushare API 文档
        ├── tushare.md               # 接口索引
        ├── help/                    # 通用 trick (本地化 / HTTP 协议 / 数据库落地)
        ├── basic/                   # 基础信息 (stock_basic / trade_cal / st / bak_basic / ...)
        └── financial/               # 财务报表 (forecast / express / disclosure_date / dividend / ...)
```

# 因子张量 T[D, A, F]

- `D` = 交易日 (`calendar.json`, exchange=SSE or SZSE)
- `A` = ts_code (`_meta/stock_basic.json`, 含已退市)
- `F` = 下表 feature
- dtype: 统一 **float** (32 比特 float; bool 用 0.0/1.0)
- kind (与「轴」独立, 勿用 kind 推断时序/截面; 以字段表「轴」列与 `FEATURES[]` 为准):
  - `filter` (1=排除该 D-A)
  - `factor` (∈[0,1] NaN=不参与; 当前实现均为截面归一后的连续得分)
  - `inter` (中间量; 含时序与截面两类, 不一刀切)

## 数据源 → 张量 (build-time PIT)

**核心**: 偏移 `offset(itf)` 在 build 阶段一次性消化进 cutoff, 张量 `T[D, A, F]` row D 只含 T 当日信号时点已可知的信息. 下游 (策略/回测/实盘) 直读 row D, 不再处理未来数据.

**落地** (raw, 按 `visible_date` 切日)
- 路径: `data/YYYY/MM/DD/<itf>.json` — 日历日切分 (周末/节假日同样写盘), `visible_date` 来源字段见下表.
- 入库时机源: `doc/tushare/help/数据更新说明.md` + 各 API 自身 doc; tushare 用语 "实时更新" = 随公告到达即落库无固定时点, "定期更新" = 周期性批发非事件驱动.
- 公告披露时段背景: `doc/exchange/公告类别和发布时间.md` (SSE/SZSE 各时段 + 非交易日 13-17 / 12-16 直通).

**cutoff** (build-time, 实盘/回测同一公式)
- 信号时点 ≜ 交易日 T 盘中, 信号计算前 1 分钟刷库; 此后每个 itf 按 `visible_date ≤ T + offset(itf)` 切片写入 row D.
- `offset` 单位 = **日历日**, 含周末/节假日; 取值见下表 `偏移` 列:
  - 入库时点 < 信号时点 (盘前已到位): `0` — T 当日记录可见.
  - 入库时点 ≥ 信号时点 (盘后 / "实时" / "不定期"): `−1` — 截至 T−1 calendar; 自然把 T−1 盘后 + T−1 至 T 之间的周末/节假日公告 (含 SSE/SZSE 非交易日直通时段) 划入 T 行.

**入张量映射** (cutoff 后按 itf 类落 row D)
- **网格** (D=trade_date, A=ts_code): 每条记录唯一 (D, A) 单元. row D 取 `max{ trade_date ≤ T + offset }` (offset=0 → 自身; offset=−1 → 上一交易日, 周末/假日 visible_date 不存在自动跳过).
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见 §字段表 deps (例: `forecast/express/income/cashflow/fina_indicator/disclosure/report` 按 `end_date`, `dividend` 按 `(end_date, div_proc)`). 状态机型 (`profit_st`) 同样按此 cutoff 回放 `visible_date` 升序流.
- **asset 静态**: `_meta/stock_basic` 全量 snapshot 广播到 (D, A); `list_date / delist_date` 决定 (D, A) 行有效, 不走 `visible_date`.
- **axis**: `calendar` 仅生成 D 轴 (`is_open=1` ∩ `exchange ∈ {SSE, SZSE}`); `_meta/stock_basic` 全量 ts_code 生成 A 轴.

**一致性** (build 完成 → 张量 PIT-clean, 下游无未来数据风险)
- **同次去重**: 同 PK 同次响应末条胜 (PK 因 itf 而异, 见 `cpp/src/tushare/spec.cpp`).
- **跨次修正** (replay 安全): 修正写新 day-file, 旧版本留存不被覆盖 → replay 任意 T 按上述 cutoff 自动选当时可见版本.
- **回测 = 实盘**: 同一份 build 代码 + 同一组 offset → 同一份 PIT 张量.
- 已知 best-effort 瑕疵 (不可消除, 接受):
  1. 公告级时间戳缺失 → 同 `visible_date` 内盘前/盘中/盘后无法区分, 统一按盘后保守 → 计入 next-day cutoff (实盘错过 T 当日盘前直通公告, 与回测一致).
  2. `_meta/stock_basic` 仅当前 snapshot → `market` (主板/创业板/...) 历史变更无法回溯, 转板股全期按当前归类.
  3. `suspend_d` tushare 标 "不定期", `offset=0` 假设盘前到位 (停牌公告绝大多数盘前发); 极端情况下 T 盘中刚发布的停牌可能在信号刷库时尚未入 tushare, 想绝对保守可改 `−1`.

| 类    | itf                    | api                  | 入库时机 (tushare)                                     | visible_date              | 偏移 |
| ----- | ---------------------- | -------------------- | ------------------------------------------------------ | ------------------------- | ---- |
| axis  | calendar               | `trade_cal`          | 定期 (新年度排程)                                      | `cal_date`                | —    |
| asset | _meta/stock_basic      | `stock_basic`        | 每次 update 覆盖刷新 (L+D+P+G)                         | —                         | —    |
| asset | _meta/index_member_all | `index_member_all`   | 每次 update 覆盖刷新 (按 SW2021 L1 分批合并, is_new=Y) | —                         | —    |
| 网格  | daily_basic            | `daily_basic`        | **盘后** 交易日 15:00–17:00                            | `trade_date`              | −1   |
| 网格  | stk_limit              | `stk_limit`          | **盘前** 交易日 08:40 左右                             | `trade_date`              | 0    |
| 网格  | adj_factor             | `adj_factor`         | **盘前** 交易日 09:15–09:20                            | `trade_date`              | 0    |
| 网格  | suspend_d              | `suspend_d`          | 不定期(通常**盘前**)                                   | `trade_date`              | 0    |
| 网格  | margin_secs            | `margin_secs`        | **盘前** 交易日 (每天更新两融名单)                     | `trade_date`              | 0    |
| 网格  | margin_detail          | `margin_detail`      | **盘前** 每日 9:00 (T 日入库 trade_date=T−1 明细)      | `trade_date`              | 0    |
| 事件  | forecast               | `forecast_vip`       | 公告实时(通常**盘后**)                                 | `ann_date`                | −1   |
| 事件  | express                | `express_vip`        | 公告实时(通常**盘后**)                                 | `ann_date`                | −1   |
| 事件  | disclosure             | `disclosure_date`    | 公告实时 (披露计划公告/修订)                           | `ann_date`                | −1   |
| 事件  | report                 | `disclosure_date`    | 公告实时 (随财报实际披露)                              | `actual_date`             | −1   |
| 事件  | dividend               | `dividend`           | 公告实时 (预案/通过/实施)                              | `imp_ann_date / ann_date` | −1   |
| 事件  | st                     | `st`                 | **盘前** imp_date 当日                                 | `imp_date`                | 0    |
| 网格  | stock_st               | `stock_st`           | **盘前** 09:20 当日全量快照 (始 20160101)              | `trade_date`              | 0    |
| 事件  | fina_indicator         | `fina_indicator_vip` | 公告实时 (随财报)                                      | `ann_date`                | −1   |
| 事件  | income                 | `income_vip`         | 公告实时 (随财报)                                      | `ann_date`                | −1   |
| 事件  | cashflow               | `cashflow_vip`       | 公告实时 (随财报)                                      | `ann_date`                | −1   |

## 字段表

本节是 feature 的「契约 / 数学定义」(描述"做什么"); 实现镜像在 `cpp/src/feature/feature.cpp` 的 `FEATURES[]` 表 + `impl::ts_*` / `impl::cs_*` (描述"怎么做"). 增减/修改 feature 须同步两处.

排序 (本表, 阅读用): filter → factor → inter; inter 内部按 causal (raw → derived); 相关字段就近. (注: `cpp/include/feature/feature.hpp` 的 `F` 枚举顺序 = 计算顺序, 与本表排序独立; enum 大段仅 TS / CS 两类, 段内按"相似聚集"对仗 — raw 网格 / raw 自算 / raw meta 派生 / derived / filter / pool.) `assumption` 列 `—` = 定义自洽; 形如 `[元]` `[%]` `[ratio]` `[股]` 的方括号前缀标注 inter 输出单位.

`deps` 列约定: `itf:<name>` ≡ 该 itf 经 §入张量统一规则 切到 (D, A); 其它为 inter / filter feature 名.

`轴` 列: `时序` = per A 沿 D 计算 (无截面依赖, A 维可并行); `截面` = per D 沿 A 计算 (有截面依赖, A 维不可并行). `filter` / `factor` / `inter` 三类均可能出现两种轴之一, 仅读本行.

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm4** (高季节性, 4 报告期 ≡ 1 年): tushare 原生 TTM 字段直接取 (pe / ps / dy); YTD 累计字段 (income / cashflow / fina_indicator) 用 helper `ttm4_ytd(X)` 拼接, 自动降级:
  - 完整: `X(t) + X(Y-1, 12) − X(Y-1, t.M)` (t = 最新可见 end_date)
  - 缺去年同期: `X(t) + X(Y-1, 12) × (12−M)/12` (均匀分布近似)
  - 缺去年年报: `X(t) × 12/M` (年化)
  - 年报 M=12: 直接 `X(t)`
- **ttm1** (低季节性, 单期 snapshot ≡ MRQ): 取 tushare 原生 (pb).

| kind   | feature      | 轴   | deps                                                       | formula                                                                                                                                                                                 | assumption                                                                                                                                                                                                            |
| ------ | ------------ | ---- | ---------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| filter | profit_st    | 时序 | itf:forecast, itf:report                                   | 状态机 (per A):                                                                                                                                                                         | `report.actual_date` 是实际披露事件的 PIT 信号 (visible=actual_date 当日); 未披露的股票 report 不出, 4月底安全网兜底                                                                                                  |
|        |              |      |                                                            | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.last_parent_net < 0` 时                                                                                            |                                                                                                                                                                                                                       |
|        |              |      |                                                            | 按 `forecast.ann_date` 触发, 至 `report.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止 (取较早)                                                                          |                                                                                                                                                                                                                       |
| filter | revenue_st   | 时序 | itf:forecast, itf:report, rev_raw, _meta/stock_basic       | 状态机 (per A) ∧ `meta.market=="主板" ∧ rev_raw < (3e8 if end_date.Y ≥ 2024 else 1e8)`:                                                                                                 | 同 profit_st: 终止用 `report.actual_date`. 主板判定 inline `meta.market[a]` (asset 静态, 全 D 同值)                                                                                                                   |
|        |              |      |                                                            | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101` 时                                                               |                                                                                                                                                                                                                       |
|        |              |      |                                                            | 按 `forecast.ann_date` 触发, 至 `report.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止 (取较早)                                                                          |                                                                                                                                                                                                                       |
| filter | dividend_st  | 时序 | itf:dividend, ni_raw, share_raw, _meta/stock_basic         | `meta.market=="主板" ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_div_tax × share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7`                                                                           | 3y 窗口 = `dividend.end_date.Y ∈ [Y-3, Y-1]` (Y = `dividend.ann_date.Y`); share_raw 取 `dividend.ann_date` 当日快照 (除权后, 实际派发若在大额送转前后会有偏差, 业务接受); 单位均 [元]; 主板判定 inline meta.market[a] |
| filter | trading_st   | 时序 | low_p, low_mc                                              | `rolling_20D(low_p ∨ low_mc).all()`                                                                                                                                                     | —                                                                                                                                                                                                                     |
| filter | risk_warn    | 时序 | itf:stock_st                                               | 直读 stock_st 每日快照 (per A): 0=不在名单, 1=ST (name 不含 '*'), 2=\*ST (name 含 '*'); 下游 tradable 仅用真假, 三态等价 boolean filter                                                  | stock_st 每交易日全量快照 (始 20160101), 无需状态机/回放; 数据起点前 (2015) 一律 0 不影响 2016+ 回测/实盘; name 字段必含 "ST" 子串 (\*ST/SST/退市 等变体), 通过是否含 '*' 区分 ST/\*ST                                |
| filter | new_list     | 时序 | list_age                                                   | `0 ≤ list_age < 60`                                                                                                                                                                     | —                                                                                                                                                                                                                     |
| factor | close        | 截面 | close_raw                                                  | `pct_rank(z(winsor_mad(1 / close_raw)))`                                                                                                                                                | —                                                                                                                                                                                                                     |
| factor | mcap         | 截面 | mcap_raw                                                   | `pct_rank(z(winsor_mad(1 / mcap_raw)))`                                                                                                                                                 | —                                                                                                                                                                                                                     |
| factor | fmcap        | 截面 | fmcap_raw                                                  | `pct_rank(z(winsor_mad(1 / fmcap_raw)))`                                                                                                                                                | —                                                                                                                                                                                                                     |
| factor | pe_ttm4      | 截面 | pe_raw                                                     | `pct_rank(z(winsor_mad(1 / pe_raw)))`                                                                                                                                                   | —                                                                                                                                                                                                                     |
| factor | pb_ttm1      | 截面 | pb_raw                                                     | `pct_rank(z(winsor_mad(1 / pb_raw)))`                                                                                                                                                   | —                                                                                                                                                                                                                     |
| factor | ps_ttm4      | 截面 | ps_raw                                                     | `pct_rank(z(winsor_mad(1 / ps_raw)))`                                                                                                                                                   | —                                                                                                                                                                                                                     |
| factor | pcf_ttm4     | 截面 | pcf_raw                                                    | `pct_rank(z(winsor_mad(1 / pcf_raw)))`                                                                                                                                                  | —                                                                                                                                                                                                                     |
| factor | roe_ttm4     | 截面 | roe_raw                                                    | `pct_rank(z(winsor_mad(roe_raw)))`                                                                                                                                                      | —                                                                                                                                                                                                                     |
| factor | roa_ttm4     | 截面 | roa_raw                                                    | `pct_rank(z(winsor_mad(roa_raw)))`                                                                                                                                                      | —                                                                                                                                                                                                                     |
| factor | dy_ttm4      | 截面 | dy_raw                                                     | `pct_rank(z(winsor_mad(dy_raw)))`                                                                                                                                                       | —                                                                                                                                                                                                                     |
| inter  | close_raw    | 时序 | itf:daily_basic                                            | `daily_basic.close` (前复权)                                                                                                                                                            | [元/股]                                                                                                                                                                                                               |
| inter  | daily_return | 时序 | close_raw                                                  | `close_raw[d] / close_raw[d-1] - 1`                                                                                                                                                     | [ratio]; 前复权链式日收益; `d==0` 或 `close_raw[d-1]` NaN/0 → NaN; 下游 benchmark = pool 内等权 daily_return 均值                                                                                                     |
| inter  | up_lim       | 时序 | itf:stk_limit                                              | `stk_limit.up_limit` (未复权); ≥100000 为无涨停限制哨兵                                                                                                                                 | [元/股]; feature 主动 −1 与 close_raw 对齐 (raw cutoff=0, 在此基础上再偏 −1)                                                                                                                                          |
| inter  | dn_lim       | 时序 | itf:stk_limit                                              | `stk_limit.down_limit` (未复权); ≤0.01 为无跌停限制哨兵                                                                                                                                 | [元/股]; feature 主动 −1 与 close_raw 对齐 (raw cutoff=0, 在此基础上再偏 −1)                                                                                                                                          |
| inter  | susp         | 时序 | itf:suspend_d                                              | `1.0 if itf:suspend_d (D, A) 存在 else 0.0`                                                                                                                                             | [bool]                                                                                                                                                                                                                |
| inter  | mcap_raw     | 时序 | itf:daily_basic                                            | `daily_basic.total_mv × 1e4`                                                                                                                                                            | [元]; 万元 → 元                                                                                                                                                                                                       |
| inter  | fmcap_raw    | 时序 | itf:daily_basic                                            | `daily_basic.circ_mv × 1e4`                                                                                                                                                             | [元]; 万元 → 元                                                                                                                                                                                                       |
| inter  | share_raw    | 时序 | itf:daily_basic                                            | `daily_basic.total_share × 1e4`                                                                                                                                                         | [股]; 万股 → 股                                                                                                                                                                                                       |
| inter  | pe_raw       | 时序 | mcap_raw, itf:income                                       | `mcap_raw / ttm4_ytd(income.n_income_attr_p)`                                                                                                                                           | [ratio]; ttm4 (YTD 拼接, 自动降级); 自己算非 tushare 原生 (支持负 PE 亏损); ni=0 → NaN                                                                                                                                |
| inter  | pb_raw       | 时序 | itf:daily_basic                                            | `daily_basic.pb`                                                                                                                                                                        | [ratio]; ttm1 (tushare 原生 MRQ)                                                                                                                                                                                      |
| inter  | ps_raw       | 时序 | itf:daily_basic                                            | `daily_basic.ps_ttm`                                                                                                                                                                    | [ratio]; ttm4 (tushare 原生)                                                                                                                                                                                          |
| inter  | dy_raw       | 时序 | itf:daily_basic                                            | `daily_basic.dv_ttm`                                                                                                                                                                    | [%]; ttm4 (tushare 原生)                                                                                                                                                                                              |
| inter  | pcf_raw      | 时序 | itf:cashflow, mcap_raw                                     | `mcap_raw / ttm4_ytd(cashflow.n_cashflow_act)`                                                                                                                                          | [ratio]; ttm4 (YTD 拼接, 自动降级); cashflow=0 → NaN                                                                                                                                                                  |
| inter  | roe_raw      | 时序 | itf:fina_indicator                                         | `ttm4_ytd(fina_indicator.roe)`                                                                                                                                                          | [%]; ttm4 (YTD 拼接, 自动降级); 取 `fina_indicator.roe` 不切其他变体; 比率近似 (分母年内忽略)                                                                                                                         |
| inter  | roa_raw      | 时序 | itf:fina_indicator                                         | `ttm4_ytd(fina_indicator.roa)`                                                                                                                                                          | [%]; ttm4 (YTD 拼接, 自动降级); 取 `fina_indicator.roa` 不切其他变体; 比率近似 (分母年内忽略)                                                                                                                         |
| inter  | rev_raw      | 时序 | itf:income                                                 | `ttm4_ytd(income.revenue)`                                                                                                                                                              | [元]; ttm4 (YTD 拼接, 自动降级)                                                                                                                                                                                       |
| inter  | ni_raw       | 时序 | itf:income                                                 | `mean(income.n_income_attr_p where income.end_date.M == 12, last 2 records)`                                                                                                            | [元]; 严格只取 `income.end_date.M == 12` 年报记录, 不混 forecast / express                                                                                                                                            |
| inter  | list_age     | 时序 | _meta/stock_basic                                          | `D − _meta/stock_basic.list_date`                                                                                                                                                       | [日历日]; 正数=已上市天数, 负数=距上市天数; 无 list_date → -1e9 (`new_list` 用 `≥ 0` 排除)                                                                                                                            |
| inter  | delist_age   | 时序 | _meta/stock_basic                                          | `D − _meta/stock_basic.delist_date`                                                                                                                                                     | [日历日]; 正数=已退市天数, 负数=距退市天数; 无 delist_date → 0 (任何 `delist_age > 0` 判定都不命中)                                                                                                                   |
| inter  | is_margin    | 时序 | itf:margin_secs                                            | `1.0 if itf:margin_secs (D, A) 存在 else 0.0`                                                                                                                                           | [bool]; 当日是否为融资融券标的; 与 `susp` 同语义 (稀疏存在=1, 否则=0, 不做 ffill)                                                                                                                                     |
| inter  | mr_bal_raw   | 时序 | itf:margin_detail                                          | `margin_detail.rzye`                                                                                                                                                                    | [元]; 融资余额; per-A grid (post_ffill: 缺席日继承前值, 与 daily_basic 一致); 校验 ≥ 0                                                                                                                                |
| inter  | ms_bal_raw   | 时序 | itf:margin_detail                                          | `margin_detail.rqye`                                                                                                                                                                    | [元]; 融券余额; 同上                                                                                                                                                                                                  |
| inter  | low_p        | 时序 | close_raw                                                  | `close_raw < 1.0`                                                                                                                                                                       | [bool]                                                                                                                                                                                                                |
| inter  | low_mc       | 时序 | mcap_raw, _meta/stock_basic                                | `mcap_raw < (5e8 if meta.market=="主板" else 3e8)`                                                                                                                                      | [bool]; 主板判定 inline meta.market[a]                                                                                                                                                                                |
| inter  | limit_up     | 时序 | close_raw, up_lim                                          | `up_lim < 100000 ∧ close_raw ≥ up_lim − 1e-4`                                                                                                                                           | [bool]; 策略涨停判定; 哨兵排除                                                                                                                                                                                        |
| inter  | limit_dn     | 时序 | close_raw, dn_lim                                          | `dn_lim > 0.01 ∧ close_raw ≤ dn_lim + 1e-4`                                                                                                                                             | [bool]; 策略跌停判定; 哨兵排除                                                                                                                                                                                        |
| inter  | pool_b       | 时序 | susp, is_margin, _meta/stock_basic, _meta/index_member_all | `meta.exchange ∈ POOL_EXCHANGE_WHITELIST ∧ meta.market ∈ POOL_MARKET_WHITELIST ∧ meta.industry_l1 ∈ POOL_INDUSTRY_L1_WHITELIST ∧ ¬susp ∧ (true if POOL_INCLUDE_MARGIN else ¬is_margin)` | [bool]; basic pool, 白名单 + 开关集中在 `config.hpp`                                                                                                                                                                  |
| inter  | pool         | 截面 | pool_b, mcap_raw                                           | `pool_b ∧ rank(mcap_raw asc) ≤ UNIVERSE_SIZE` (per D, within `pool_b`; 默认 UNIVERSE_SIZE = 80)                                                                                         | [bool]; 排名母集 (pct_rank 母集 + nth-smallest 母集)                                                                                                                                                                  |
| inter  | tradable     | 截面 | pool, profit_st, revenue_st, dividend_st,                  | `pool ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)`                                                                                                    | [bool]; 选股母集 (策略实际可买入的 a). pool 不变是为了 factor pct_rank 口径稳定; 下游策略每日 top-K 在 tradable 内挑                                                                                                  |
|        |              |      | trading_st, risk_warn, new_list                            |                                                                                                                                                                                         |                                                                                                                                                                                                                       |

## 构建流水线 (data → Tensor)

`feature::build()` 串 4 phase 全过程式; 入口 `cpp/src/feature/build.cpp`.

**Phase 切分动机**

| phase  | 数据形态          | 任务粒度             | 并行性             | 主要工作                              |
| ------ | ----------------- | -------------------- | ------------------ | ------------------------------------- |
| 0 axes | 标量级元数据      | 主线程               | 无                 | 一次性确定 D / A / per-A 静态         |
| 1 load | 文件级 raw json   | (day, itf) ≈ 3650×16 | embarrassingly     | 解析 + **PIT cutoff 落到 row D 索引** |
| 2 时序 | 列式 (per-A 全 D) | a ≈ 5500             | embarrassingly (A) | 单调时间序列计算 + 状态机             |
| 3 截面 | 行式 (per-D 全 A) | d ≈ 2750             | embarrassingly (D) | 截面归一 + universe 选取              |

**业务密集化 + agnostic 外层** — 改字段表/计算图不动外层:
- `pit.cpp` (itf 维 单点真理): 每 itf 一组 `{prealloc, parse, post_sort?, post_ffill?}` + 末尾 `ITFS[]` 表挂载.
- `feature.cpp` (feature 维 单点真理): 每 feature 一个 `ts_xxx`/`cs_xxx` compute fn + 末尾 `FEATURES[]` 表挂载.
- 外层 flow (`load.cpp` / `ts.cpp` / `cs.cpp` / `build.cpp`) 仅通过函数指针表迭代调度, 不出现任何具体 itf 名 / feature 名.

**关键设计选择** (动机性的, 散落点上提):
- **PIT cutoff 在 Phase 1 一次性消化**: `parse` 内 `row = v_idx - itf::CUTOFF` 直接定位行 D, 写完后 `pool[a, d]` 即 "T 当日合法可见数据". Phase 2/3 不再做任何时间偏移 — 杜绝下游漏算 cutoff 导致的未来数据泄漏 (类比: PIT 责任收敛在数据入口, 不下放).
- **F 枚举顺序 = 计算顺序 = 隐式 topo sort**: 调度器 (ts.cpp / cs.cpp) 仅按 `FEATURES[]` 索引顺序串行调; 只要 "新 feature 加在其依赖之后", 后段直接读 `T.ts_row(prior_f, a)` 即可, 无需运行时 topo / 依赖锁.
- **网格无锁 + 事件 per-A 锁**: 网格 itf 因 `(a, v_idx)` slot 唯一 → 完全无锁写; 事件 itf 多对一 emplace, 锁粒度精到 `vector<mutex>(n_a)` (非全局, 非 per-itf), 接近无争用.
- **F 段独立 A*D layout (a-major / d-minor)**: Phase 2 的 `ts_row(f, a)` 是连续 span (cache friendly, 主路径); Phase 3 的 `gather/scatter_cs_row(f, d)` 是 stride-D copy (3 buffer 复用, 一次性付出).

```text
Phase 0 axes  (主线程; axis.cpp + tensor.cpp)
  # 形态: 标量级, 串行. 后续所有 phase 共用的索引基线 — 只跑一次, 无并行收益可言.
  axes ← load_axes()
    D ← scan data/**/calendar.json, 取 (exchange ∈ {SSE,SZSE} ∧ is_open=1) 的 cal_date 升序去重
    A ← read data/_meta/stock_basic.json, 取全量 ts_code (含已退市) 升序
    + 反向索引 date_idx / code_idx, sys_days 缓存 date_days
    floor_date(d) = max{i : dates[i] ≤ d}    # 周末/节假日 visible_date 自动落到上一交易日
  meta ← load_stock_meta(axes)               # per-A 静态: list_date / delist_date / market / exchange / industry_l1
                                             #   industry_l1 ← _meta/index_member_all.json::l1_name (申万 SW2021 一级, PK=ts_code)
  T    ← Tensor(axes)                        # F 段独立 A*D float, NaN 初始化, a-major / d-minor
                                             #   ts_row(f,a) = 连续 D span (Phase 2 主路径)
                                             #   gather/scatter_cs_row(f,d) = stride D copy (Phase 3 入口)

Phase 1 PIT load  (per-(day, itf) 并行; load.cpp 通用 flow + pit.cpp 单点 itf 表)
  # 形态: 文件级, 任务粒度 = (day, itf). 文件之间无依赖 → 任务池抢占式分发.
  # 关键: cutoff 在此 一次性 落到 row 索引, 写后 pool[a, d] 即 "T 当日合法可见数据"; 下游 0 时间偏移.
  # 并发: 网格 itf 写入完全无锁 (slot 唯一); 事件 itf 仅 per-A mutex 锁 emplace, 争用接近 0.

  for itf in pit.cpp::ITFS[]:                # 仅迭代 ITFS[] 表, 不出现具体 itf 名
    itf.prealloc(axes, pool)                 # 网格: 字段 vector A*D NaN/0; 事件: EventStore[A] 空链

  tasks ← enumerate data/YYYY/MM/DD/<itf.file_name>.json over ITFS[]:
    v_idx ← axes.floor_date(file's day = visible_date)
    skip if v_idx < 0                        # visible_date 早于 dates[0], 无 row D 可写
    skip if 网格 itf ∧ file's day ∉ axes.date_idx   # 网格 file's day 须为交易日 (data 自身保证)

  parallel for task in tasks (n_threads = misc::Affinity::core_count()):
    arr ← yyjson_read(task.path)             # 单 day 单 itf 的 record 数组
    task.itf.parse(arr, v_idx, axes, pool, mu_or_null)
                                             # row = v_idx - itf::CUTOFF
                                             #   (CUTOFF=0 → row=v_idx; CUTOFF=-1 → row=v_idx+1; row≥n_d 越界 skip)
                                             # 网格: mu=nullptr; pool.<itf>.<field>[a*n_d + row] 无锁写
                                             # 事件: mu[a] 锁后 pool.<itf>[a].emplace_back(Ev{v=row,…})

  for itf in ITFS[] where itf.post_sort:     # 事件 itf 末段 sort by v 升序 — 给 Phase 2 单调指针扫
    itf.post_sort(pool)

  for itf in ITFS[] where itf.post_ffill:    # 网格 itf per-A forward fill (停牌期间继承前值)
    itf.post_ffill(axes, pool)

Phase 2 时序  (per-A 并行; ts.cpp 通用 flow + feature.cpp 单点 feature 表)
  # 形态: 列式 (per-A 全 D), A 维 embarrassingly parallel; D 内强 causal (滚动/状态机).
  # 不变量: 每 worker 独占一段 ts_row(*, a), 无写冲突; FEATURES[] TS 段顺序 = 计算顺序,
  #         后段直读 T.ts_row(prior_f, a) (例: low_p 读 close_raw; revenue_st 读 rev_raw).
  # NaN 策略: 不人为补 0 — 上市前/退市后/长期停牌的 NaN 自然流到下游, 让数据可用性显式可判.

  parallel for a in [0, n_a) (n_threads = core_count):
    for f in FEATURES[] where axis == TimeSeries:
      f.compute_ts(a, axes, pool, meta, T)   # 写 T.ts_row(F::self, a); 读 PitPool / StockMeta / 前段 T.ts_row

  # FEATURES[] 索引 = F 枚举值 = 计算顺序; enum 大段仅 TS / CS, 段内对仗如下:
  #   raw 网格      (PitPool dense 直读, pool[a, d] 已是 row D 合法数据):
  #     close_raw / mcap_raw(×1e4) / fmcap_raw(×1e4) / share_raw(×1e4) / pb_raw / ps_raw / dy_raw
  #         ← daily_basic[a, d]
  #     up_lim / dn_lim ← stk_limit[a, d-1] (feature 主动 -1 与 close_raw 对齐; 保留哨兵 100000/0.01)
  #     susp        ← suspend_d[a, d]
  #     is_margin   ← margin_secs[a, d] (bool: 当日是否两融标的)
  #     mr_bal_raw  ← margin_detail.rzye[a, d]   (融资余额 [元])
  #     ms_bal_raw  ← margin_detail.rqye[a, d]   (融券余额 [元])
  #     risk_warn   ← stock_st.state[a, d] (uint8: 0=正常 / 1=ST / 2=*ST; 每日快照, 不需回放)
  #   raw 自算      (财报 ttm4 ytd 拼接; ts.hpp::ttm4_ytd_compute 模板, 自动降级):
  #     rev_raw ← ttm4(income.revenue, type=='1')
  #     ni_raw  ← mean(latest 2 distinct income.end_date.M==12 ∧ type=='1' 的 n_income_attr_p)
  #     pe_raw  ← mcap_raw / ttm4(income.n_income_attr_p) (自己算, 支持负 PE)
  #     pcf_raw ← mcap_raw / ttm4(cashflow.n_cashflow_act, type=='1')
  #     roe_raw ← ttm4(fina_indicator.roe);  roa_raw ← ttm4(fina_indicator.roa)
  #   raw meta 派生 (per-A 动态: 每天 +1):
  #     list_age   ← date_days[d] − parse(meta.list_date[a])
  #     delist_age ← date_days[d] − parse(meta.delist_date[a])  # 无 delist_date → 0
  #   derived       (T 内依赖):
  #     daily_return ← close_raw[d]/close_raw[d-1] - 1   # d==0 或 close_raw[d-1] NaN/0 → NaN
  #     low_p        ← close_raw < 1.0
  #     low_mc       ← mcap_raw < (5e8 if meta.market[a]=="主板" else 3e8)   # 主板判定 inline
  #     limit_up / limit_dn ← close_raw vs up_lim / dn_lim (哨兵排除)
  #   filter        (state machine; ts.hpp::state_machine_intervals 模板):
  #     profit_st   ← OR over { forecast 触发 → off=min(report 同 end_date, ceil(Y+1,4,30)) }
  #                   on_d=trigger.v, 区间 [on_d, off_d) 写 1
  #     revenue_st  ← profit_st 同区间, 区间内再叠 (meta.market=="主板" ∧ rev_raw < threshold(end_date.Y))
  #     dividend_st ← 阶梯 forward fill: 每 dividend event 重算 3y_sum (累加历史 events with
  #                     end_date.Y ∈ [ann_y-3, ann_y-1] 的 cash_div_tax × share_raw[event.v]),
  #                     仅主板 (meta.market=="主板"); 区间 [e.v, next.v) 内按 (ni_raw>0 ∧ 3y_sum 双阈) 写 1.
  #                     warmup_d = max(axes 起点+3y, list_date+3y), 之前一律 0 (3y 窗口不完整, 不偏严).
  #     trading_st  ← rolling 20D over (low_p ∨ low_mc).all()     # 单调计数
  #     new_list    ← 0 ≤ list_age < 60
  #   pool (TS):
  #     pool_b   ← (meta.exchange ∈ config::POOL_EXCHANGE_WHITELIST)
  #                ∧ (meta.market ∈ config::POOL_MARKET_WHITELIST)
  #                ∧ (meta.industry_l1 ∈ config::POOL_INDUSTRY_L1_WHITELIST) ∧ ¬susp
  #                ∧ (true if config::POOL_INCLUDE_MARGIN else ¬is_margin)

Phase 3 截面  (per-D 并行; cs.cpp 通用 flow + feature.cpp 单点 feature 表)
  # 形态: 行式 (per-D 全 A), D 维 embarrassingly parallel; A 内强 causal (winsor/z/rank).
  # 入口代价: gather_cs_row 是 stride-D copy (vs Phase 2 ts_row 连续 span); 每 worker
  #          thread-local 3 buffer (length=n_a) 复用, 避免反复分配.
  # 不变量: FEATURES[] CS 段顺序 = 计算顺序 (tradable 在最后, 读 pool + 6 filter).

  parallel for d in [0, n_d) (n_threads = core_count):
    bufs ← thread-local {a, b, c}: 3 × vector<float>(n_a)
    for f in FEATURES[] where axis == CrossSection:
      f.compute_cs(d, axes, T, bufs)         # 写 T.scatter_cs_row(F::self, d, …)

  # CS 业务一览 (实现见 feature.cpp::impl::cs_*):
  #   factor 流水: cs.hpp::factor_pipeline(d, src, dst, invert, T, buf):
  #     buf[A] ← T.gather_cs_row(src, d)
  #     if invert: buf[a] ← 1/buf[a]         # NaN 或 0 → NaN; 价/估值类 invert=true (越小越优), 收益类 invert=false
  #     buf ← winsor_mad(buf, k=3)           # 截断到 [med ± k·MAD]; 全等/<2 finite → 跳过
  #     buf ← z(buf)                         # (x − mean)/std, 跳 NaN; var≤0 → 跳过
  #     buf ← pct_rank(buf)                  # 升序百分位 ∈ [0,1], 同值平均秩, 跳 NaN
  #     T.scatter_cs_row(dst, d) ← buf
  #   close (← close_raw, invert), mcap, fmcap, pe_ttm4, pb_ttm1, ps_ttm4, pcf_ttm4 (全 invert)
  #   roe_ttm4, roa_ttm4, dy_ttm4 (无 invert)
  #   pool: cands ← {a : pool_b[a,d]=1 ∧ is_finite(mcap_raw[a,d])}
  #         pool[a,d] ← 1.0 if a ∈ nth_smallest(cands by mcap_raw, UNIVERSE_SIZE) else 0.0
  #   tradable: pool[a,d] ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)[a,d]
```

并发模型规格 (动机/不变量已在各 Phase 头部展开, 此处仅列数据 + 同步点)
- Phase 0: 主线程; 全量 in-memory; 跑一次.
- Phase 1: 任务数 ≈ 3650 day × |ITFS| (≈ 16); 网格 `mu=nullptr`, 事件 `vector<mutex>(n_a)`; 末段 `post_sort` / `post_ffill` 单线程串行.
- Phase 2: 任务数 ≈ n_a (5500); 每 worker 独占 `T.ts_row(*, a)`.
- Phase 3: 任务数 ≈ n_d (2750); 每 worker 独占 `cs_row` 段 + thread-local 3 buffer (length=n_a).
- 同步点: 仅 phase 间硬屏障 (`build.cpp` 顺序 `join` + `misc::Timer` 报段时), phase 内无屏障.

## 增减用法 (改计算图 / 字段表)

新增/修改/删除一个 itf:
1. `cpp/include/feature/pit.hpp`: 加/改 typed `Grid<…>` / `<…>Ev` struct, 在 `PitPool` 加成员
2. `cpp/src/feature/pit.cpp`: 加 `namespace itf_<name> { prealloc, parse, [post_sort] }` 一组 dense block
3. `cpp/src/feature/pit.cpp`: `ITFS[]` 末尾追加一行
   外层 `load.cpp` / `build.cpp` 不动.

新增/修改/删除一个 feature:
1. `cpp/include/feature/feature.hpp`: 在 `F` 枚举对应位置加一行 (位置 = 计算顺序; 后于其依赖)
2. `cpp/src/feature/feature.cpp`: 在 `namespace impl` 加 `ts_<name>` (签名 `TsComputeFn`) 或 `cs_<name>` (签名 `CsComputeFn`)
3. `cpp/src/feature/feature.cpp`: `FEATURES[]` 对应位置加一行 `{name, kind, axis, &impl::ts_xxx | nullptr, &impl::cs_xxx | nullptr}`
   外层 `ts.cpp` / `cs.cpp` / `build.cpp` 不动. 依赖通过 enum 顺序保证 (无需 topo sort).

通用 kernel (跨 feature 共用): `feature/ts.hpp` 暴露 `ttm4_ytd_compute<Ev>` / `state_machine_intervals<TEv>` 模板; `feature/cs.hpp` 暴露 `winsor_mad / z / pct_rank / factor_pipeline`. 大多数新 factor 一行 `factor_pipeline(d, F::xxx_raw, F::xxx, invert, T, b.a)` 即可.
