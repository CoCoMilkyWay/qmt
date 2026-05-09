Motive: 实盘量化交易; 国金证券 QMT 客户端下单, tushare 维护本地数据库

# 项目结构

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
│                                    # itf ∈ {forecast, express, disclosure, report, st, calendar,
│                                    #        dividend, daily_basic, adj_factor, stk_limit, suspend_d,
│                                    #        fina_indicator, income, cashflow}
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
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见 §字段表 deps (例: `forecast/express/income/cashflow/fina_indicator/disclosure/report` 按 `end_date`, `dividend` 按 `(end_date, div_proc)`). 状态机型 (`profit_st / risk_warn`) 同样按此 cutoff 回放 `visible_date` 升序流.
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

| 类    | itf               | api                  | 入库时机 (tushare)             | visible_date              | 偏移 |
| ----- | ----------------- | -------------------- | ------------------------------ | ------------------------- | ---- |
| axis  | calendar          | `trade_cal`          | 定期 (新年度排程)              | `cal_date`                | —    |
| asset | _meta/stock_basic | `stock_basic`        | 每次 update 覆盖刷新 (L+D+P+G) | —                         | —    |
| 网格  | daily_basic       | `daily_basic`        | **盘后** 交易日 15:00–17:00    | `trade_date`              | −1   |
| 网格  | stk_limit         | `stk_limit`          | **盘前** 交易日 08:40 左右     | `trade_date`              | 0    |
| 网格  | adj_factor        | `adj_factor`         | **盘前** 交易日 09:15–09:20    | `trade_date`              | 0    |
| 网格  | suspend_d         | `suspend_d`          | 不定期(通常**盘前**)           | `trade_date`              | 0    |
| 事件  | forecast          | `forecast_vip`       | 公告实时(通常**盘后**)         | `ann_date`                | −1   |
| 事件  | express           | `express_vip`        | 公告实时(通常**盘后**)         | `ann_date`                | −1   |
| 事件  | disclosure        | `disclosure_date`    | 公告实时 (披露计划公告/修订)   | `ann_date`                | −1   |
| 事件  | report            | `disclosure_date`    | 公告实时 (随财报实际披露)      | `actual_date`             | −1   |
| 事件  | dividend          | `dividend`           | 公告实时 (预案/通过/实施)      | `imp_ann_date / ann_date` | −1   |
| 事件  | st                | `st`                 | **盘前** imp_date 当日         | `imp_date`                | 0    |
| 事件  | fina_indicator    | `fina_indicator_vip` | 公告实时 (随财报)              | `ann_date`                | −1   |
| 事件  | income            | `income_vip`         | 公告实时 (随财报)              | `ann_date`                | −1   |
| 事件  | cashflow          | `cashflow_vip`       | 公告实时 (随财报)              | `ann_date`                | −1   |

## 字段表

本节是 feature 的「契约 / 数学定义」(描述"做什么"); 实现镜像在 `cpp/src/feature/feature.cpp` 的 `FEATURES[]` 表 + `impl::ts_*` / `impl::cs_*` (描述"怎么做"). 增减/修改 feature 须同步两处.

排序 (本表, 阅读用): filter → factor → inter; inter 内部按 causal (raw → derived); 相关字段就近. (注: `cpp/include/feature/feature.hpp` 的 `F` 枚举顺序 = 计算顺序, 与本表排序独立.) `assumption` 列 `—` = 定义自洽; 形如 `[元]` `[%]` `[ratio]` `[股]` 的方括号前缀标注 inter 输出单位.

`deps` 列约定: `itf:<name>` ≡ 该 itf 经 §入张量统一规则 切到 (D, A); 其它为 inter / filter feature 名.

`轴` 列: `时序` = per A 沿 D 计算 (无截面依赖, A 维可并行); `截面` = per D 沿 A 计算 (有截面依赖, A 维不可并行). `filter` / `factor` / `inter` 三类均可能出现两种轴之一, 仅读本行.

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm4** (高季节性, 4 报告期 ≡ 1 年): tushare 原生 TTM 字段直接取 (pe / ps / dy); YTD 累计字段 (income / cashflow / fina_indicator) 用 helper `ttm4_ytd(X) := X(t) + X(Y-1, 12) − X(Y-1, t.M)` 拼接, 其中 t = (Y, M) 为最新可见 `end_date`. 严格成立于流量字段 (revenue, n_cashflow_act); 对比率字段 (roe, roa) 是常用近似 (分子严格, 分母年内变动忽略).
- **ttm1** (低季节性, 单期 snapshot ≡ MRQ): 取 tushare 原生 (pb).

| kind   | feature     | 轴   | deps                                  | formula                                                                                                                   | assumption                                                                                                                                                                            |
| ------ | ----------- | ---- | ------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| filter | profit_st   | 时序 | itf:forecast, itf:report              | 状态机 (per A):                                                                                                           | `report.actual_date` 是实际披露事件的 PIT 信号 (visible=actual_date 当日); 未披露的股票 report 不出, 4月底安全网兜底                                                                  |
|        |             |      |                                       | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.last_parent_net < 0` 时                              |                                                                                                                                                                                       |
|        |             |      |                                       | 按 `forecast.ann_date` 触发, 至 `report.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止 (取较早)            |                                                                                                                                                                                       |
| filter | revenue_st  | 时序 | itf:forecast, itf:report, rev_raw, mb | 状态机 (per A) ∧ `mb ∧ rev_raw < (3e8 if end_date.Y ≥ 2024 else 1e8)`:                                                    | 同 profit_st: 终止用 `report.actual_date`                                                                                                                                             |
|        |             |      |                                       | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101` 时 |                                                                                                                                                                                       |
|        |             |      |                                       | 按 `forecast.ann_date` 触发, 至 `report.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止 (取较早)            |                                                                                                                                                                                       |
| filter | dividend_st | 时序 | itf:dividend, ni_raw, share_raw, mb   | `mb ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_div_tax × share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7`                              | 3y 窗口 = `dividend.end_date.Y ∈ [Y-3, Y-1]` (Y = `dividend.ann_date.Y`); share_raw 取 `dividend.ann_date` 当日快照 (除权后, 实际派发若在大额送转前后会有偏差, 业务接受); 单位均 [元] |
| filter | trading_st  | 时序 | low_p, low_mc                         | `rolling_20D(low_p ∨ low_mc).all()`                                                                                       | —                                                                                                                                                                                     |
| filter | risk_warn   | 时序 | itf:st                                | 状态机 (per A): 按 `st.imp_date` 升序回放 `st.name` (变更后名), 含 "ST" 子串则上线, 不含则下线                            | A 股监管命名规则: ST 状态名必含 "ST" 子串 (含 *ST/SST 等变体), 撤销摘帽后名不含 "ST"                                                                                                  |
| filter | new_list    | 时序 | list_age                              | `list_age < 60`                                                                                                           | —                                                                                                                                                                                     |
| factor | close       | 截面 | close_raw                             | `pct_rank(z(winsor_mad(1 / close_raw)))`                                                                                  | —                                                                                                                                                                                     |
| factor | mcap        | 截面 | mcap_raw                              | `pct_rank(z(winsor_mad(1 / mcap_raw)))`                                                                                   | —                                                                                                                                                                                     |
| factor | fmcap       | 截面 | fmcap_raw                             | `pct_rank(z(winsor_mad(1 / fmcap_raw)))`                                                                                  | —                                                                                                                                                                                     |
| factor | pe_ttm4     | 截面 | pe_raw                                | `pct_rank(z(winsor_mad(1 / pe_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | pb_ttm1     | 截面 | pb_raw                                | `pct_rank(z(winsor_mad(1 / pb_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | ps_ttm4     | 截面 | ps_raw                                | `pct_rank(z(winsor_mad(1 / ps_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | pcf_ttm4    | 截面 | pcf_raw                               | `pct_rank(z(winsor_mad(1 / pcf_raw)))`                                                                                    | —                                                                                                                                                                                     |
| factor | roe_ttm4    | 截面 | roe_raw                               | `pct_rank(z(winsor_mad(roe_raw)))`                                                                                        | —                                                                                                                                                                                     |
| factor | roa_ttm4    | 截面 | roa_raw                               | `pct_rank(z(winsor_mad(roa_raw)))`                                                                                        | —                                                                                                                                                                                     |
| factor | dy_ttm4     | 截面 | dy_raw                                | `pct_rank(z(winsor_mad(dy_raw)))`                                                                                         | —                                                                                                                                                                                     |
| inter  | close_raw   | 时序 | itf:daily_basic                       | `daily_basic.close` (前复权)                                                                                              | [元/股]                                                                                                                                                                               |
| inter  | up_lim      | 时序 | itf:stk_limit                         | `stk_limit.up_limit` (前复权); ≥100000 → NaN (无涨停限制哨兵)                                                             | [元/股]; offset=-1 与 close_raw 对齐                                                                                                                                                  |
| inter  | dn_lim      | 时序 | itf:stk_limit                         | `stk_limit.down_limit` (前复权); ≤0.01 → NaN (无跌停限制哨兵)                                                             | [元/股]; offset=-1 与 close_raw 对齐                                                                                                                                                  |
| inter  | susp        | 时序 | itf:suspend_d                         | `1.0 if itf:suspend_d (D, A) 存在 else 0.0`                                                                               | [bool]                                                                                                                                                                                |
| inter  | mcap_raw    | 时序 | itf:daily_basic                       | `daily_basic.total_mv × 1e4`                                                                                              | [元]; 万元 → 元                                                                                                                                                                       |
| inter  | fmcap_raw   | 时序 | itf:daily_basic                       | `daily_basic.circ_mv × 1e4`                                                                                               | [元]; 万元 → 元                                                                                                                                                                       |
| inter  | share_raw   | 时序 | itf:daily_basic                       | `daily_basic.total_share × 1e4`                                                                                           | [股]; 万股 → 股                                                                                                                                                                       |
| inter  | pe_raw      | 时序 | itf:daily_basic                       | `daily_basic.pe_ttm`                                                                                                      | [ratio]; ttm4 (tushare 原生); 亏损 → NaN                                                                                                                                              |
| inter  | pb_raw      | 时序 | itf:daily_basic                       | `daily_basic.pb`                                                                                                          | [ratio]; ttm1 (tushare 原生 MRQ)                                                                                                                                                      |
| inter  | ps_raw      | 时序 | itf:daily_basic                       | `daily_basic.ps_ttm`                                                                                                      | [ratio]; ttm4 (tushare 原生)                                                                                                                                                          |
| inter  | dy_raw      | 时序 | itf:daily_basic                       | `daily_basic.dv_ttm`                                                                                                      | [%]; ttm4 (tushare 原生)                                                                                                                                                              |
| inter  | pcf_raw     | 时序 | itf:cashflow, mcap_raw                | `mcap_raw / ttm4_ytd(cashflow.n_cashflow_act)`                                                                            | [ratio]; ttm4 (YTD 拼接); 流量字段严格; 任一期缺 → NaN                                                                                                                                |
| inter  | roe_raw     | 时序 | itf:fina_indicator                    | `ttm4_ytd(fina_indicator.roe)`                                                                                            | [%]; ttm4 (YTD 拼接, 取 `fina_indicator.roe` 不切其他变体 `roe_waa` 加权 / `roe_dt` 扣非 / `roe_yearly` 年化);<br>比率近似 (分母年内忽略);<br>任一期缺 → NaN                          |
| inter  | roa_raw     | 时序 | itf:fina_indicator                    | `ttm4_ytd(fina_indicator.roa)`                                                                                            | [%]; ttm4 (YTD 拼接, 取 `fina_indicator.roa` 不切其他变体 `roa_yearly` 年化 / `roa_dp` 杜邦);<br>比率近似 (分母年内忽略);<br>任一期缺 → NaN                                           |
| inter  | rev_raw     | 时序 | itf:income                            | `ttm4_ytd(income.revenue)`                                                                                                | [元]; ttm4 (YTD 拼接); 流量字段严格; 任一期缺 → NaN                                                                                                                                   |
| inter  | ni_raw      | 时序 | itf:income                            | `mean(income.n_income_attr_p where income.end_date.M == 12, last 2 records)`                                              | [元]; 严格只取 `income.end_date.M == 12` 年报记录, 不混 forecast / express                                                                                                            |
| inter  | mb          | 时序 | _meta/stock_basic                     | `_meta/stock_basic.market == '主板'`                                                                                      | [bool]; asset 静态广播 (D, A)                                                                                                                                                         |
| inter  | list_age    | 时序 | _meta/stock_basic                     | `D − _meta/stock_basic.list_date`                                                                                         | [日历日]                                                                                                                                                                              |
| inter  | low_p       | 时序 | close_raw                             | `close_raw < 1.0`                                                                                                         | [bool]                                                                                                                                                                                |
| inter  | low_mc      | 时序 | mcap_raw, mb                          | `mcap_raw < (5e8 if mb else 3e8)`                                                                                         | [bool]                                                                                                                                                                                |
| inter  | limit_up    | 时序 | close_raw, up_lim                     | `close_raw ≥ up_lim − 1e-4`                                                                                               | [bool]; 策略涨停判定                                                                                                                                                                  |
| inter  | limit_dn    | 时序 | close_raw, dn_lim                     | `close_raw ≤ dn_lim + 1e-4`                                                                                               | [bool]; 策略跌停判定                                                                                                                                                                  |
| inter  | pool_b      | 时序 | mb, susp, _meta/stock_basic           | `_meta/stock_basic.exchange ∈ {SSE, SZSE} ∧ mb ∧ ¬susp`                                                                   | [bool]; basic pool, 当前 strategy 仅主板                                                                                                                                              |
| inter  | pool        | 截面 | pool_b, mcap_raw                      | `pool_b ∧ rank(mcap_raw asc) ≤ UNIVERSE_SIZE` (per D, within `pool_b`; 默认 UNIVERSE_SIZE = 80)                           | [bool]; 最终 strategy universe                                                                                                                                                        |

## 构建流水线 (data → Tensor)

`feature::build()` 串 4 phase 全过程式; 入口 `cpp/src/feature/build.cpp`.

**架构**: 业务密集化到 2 个单点真理文件 — `pit.cpp` (itf 维) + `feature.cpp` (feature 维); 外层 flow (`load.cpp` / `ts.cpp` / `cs.cpp` / `build.cpp`) 完全 agnostic, 仅通过函数指针表 (`ITFS[]` / `FEATURES[]`) 迭代调度. 改字段表/计算图 不动外层.

```text
Phase 0 axes  (主线程; axis.cpp + tensor.cpp)
  axes ← load_axes()
    D ← scan data/**/calendar.json, 取 (exchange ∈ {SSE,SZSE} ∧ is_open=1) 的 cal_date 升序去重
    A ← read data/_meta/stock_basic.json, 取全量 ts_code (含已退市) 升序
    + 反向索引 date_idx / code_idx, sys_days 缓存 date_days
    floor_date(d) = max{i : dates[i] ≤ d}    # 周末/节假日 visible_date 自动落到上一交易日
  meta ← load_stock_meta(axes)               # per-A 静态: list_date / delist_date / market / exchange
  T    ← Tensor(axes)                        # F 段独立 A*D float, NaN 初始化, a-major / d-minor
                                             #   ts_row(f,a) = 连续 D span (Phase 2 主路径)
                                             #   gather/scatter_cs_row(f,d) = stride D copy (Phase 3 入口)

Phase 1 PIT load  (per-(day, itf) 并行; load.cpp 通用 flow + pit.cpp 单点 itf 表)
  for itf in pit.cpp::ITFS[]:                # 仅迭代 ITFS[] 表, 不出现具体 itf 名
    itf.prealloc(axes, pool)                 # 网格: 字段 vector A*D NaN/0; 事件: EventStore[A] 空链

  tasks ← enumerate data/YYYY/MM/DD/<itf.file_name>.json over ITFS[]:
    v_idx ← axes.floor_date(file's day = visible_date)
    skip if v_idx < 0                        # visible_date 早于 dates[0], 无 row D 可写
    skip if 网格 itf ∧ file's day ∉ axes.date_idx   # 网格 file's day 须为交易日 (data 自身保证)

  parallel for task in tasks (n_threads = misc::Affinity::core_count()):
    arr ← yyjson_read(task.path)             # 单 day 单 itf 的 record 数组
    task.itf.parse(arr, v_idx, axes, pool, mu_or_null)
                                             # 网格 itf: mu=nullptr, dense slot 写入无锁
                                             #   per record (a = code_idx[ts_code], v_idx 唯一)
                                             #     → pool.<itf>.<field>[a*n_d + v_idx]
                                             # 事件 itf: mu=vector<mutex>(n_a), 取 mu[a] 后
                                             #   pool.<itf>[a].emplace_back(Ev{v=v_idx,…})

  for itf in ITFS[] where itf.post_sort:     # 事件 itf 末段 sort by v 升序 (Phase 2 走单调指针扫)
    itf.post_sort(pool)

  for itf in ITFS[] where itf.post_ffill:    # 网格 itf per-A forward fill (停牌期间用前值)
    itf.post_ffill(axes, pool)

Phase 2 时序  (per-A 并行; ts.cpp 通用 flow + feature.cpp 单点 feature 表)
  parallel for a in [0, n_a) (n_threads = core_count):
    for f in FEATURES[] where axis == TimeSeries:
      f.compute_ts(a, axes, pool, meta, T)   # 写自己的 T.ts_row(F::self, a)
                                             # 可读 PitPool / StockMeta / 已写就的 T.ts_row(prior_f, a)

  # FEATURES[] 索引 = F 枚举值 = 计算顺序; 业务一览 (具体实现见 feature.cpp::impl::ts_*):
  #   raw          (PitPool 网格抽取, 应用 offset):
  #     close_raw / mcap_raw(×1e4) / fmcap_raw(×1e4) / share_raw(×1e4) / pe_raw / pb_raw / ps_raw / dy_raw
  #         ← daily_basic[a, d-1]    # offset = -1
  #     up_lim / dn_lim ← stk_limit[a,d]; susp ← suspend_d[a,d]   # offset = 0
  #   ttm4         (财报 ytd 拼接; ts.hpp::ttm4_ytd_compute 模板, d_target = v+1, M==12 退化 X(t)):
  #     rev_raw ← ttm4(income.revenue, type=='1')
  #     pcf_raw ← mcap_raw / ttm4(cashflow.n_cashflow_act, type=='1')
  #     roe_raw ← ttm4(fina_indicator.roe);  roa_raw ← ttm4(fina_indicator.roa)
  #     ni_raw  ← mean(latest 2 distinct income.end_date.M==12 ∧ type=='1' 的 n_income_attr_p)
  #   静态广播     (StockMeta):
  #     mb ← (meta.market[a] == "主板") 广播 D;   list_age ← date_days[d] − parse(meta.list_date[a])
  #   衍生 bool    (T 内依赖):
  #     low_p / low_mc / limit_up / limit_dn
  #   state machine (filter; ts.hpp::state_machine_intervals 模板):
  #     profit_st   ← OR over { forecast 触发 → off=min(report 同 end_date, ceil(Y+1,4,30)) }
  #                   on_d=trigger.v+1, 区间 [on_d, off_d) 写 1
  #     revenue_st  ← profit_st 同区间, 区间内再叠 (mb ∧ rev_raw < threshold(end_date.Y))
  #     dividend_st ← 阶梯 forward fill: 每 dividend event 重算 3y_sum (累加历史 events with
  #                     end_date.Y ∈ [ann_y-3, ann_y-1] 的 cash_div_tax × share_raw[event.v+1]),
  #                     区间 [e.v+1, next.v+1) 内按 (mb ∧ ni_raw>0 ∧ 3y_sum 双阈) 写 1
  #     risk_warn   ← v 升序回放 st events: state ← (st_name 含 "ST"), forward fill;
  #                   offset=0, d_target = v (st.imp_date 必为交易日)
  #     trading_st  ← rolling 20D over (low_p ∨ low_mc).all()     # 单调计数
  #   杂项 filter / pool:
  #     new_list ← list_age < 60
  #     pool_b   ← (meta.exchange ∈ {SSE,SZSE}) ∧ mb ∧ ¬susp

Phase 3 截面  (per-D 并行; cs.cpp 通用 flow + feature.cpp 单点 feature 表)
  parallel for d in [0, n_d) (n_threads = core_count):
    bufs ← thread-local {a, b, c}: 3 × vector<float>(n_a), 复用避免反复分配
    for f in FEATURES[] where axis == CrossSection:
      f.compute_cs(d, axes, T, bufs)         # 写自己的 T.scatter_cs_row(F::self, d, …)

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
```

并发模型一览
- Phase 0: 主线程; 全量 in-memory.
- Phase 1: 任务粒度 = (day, itf) (≈ 3650 day × |ITFS|). 网格 itf 因 (a, v_idx) slot 唯一 → 无锁 (`mu=nullptr`); 事件 itf → `vector<mutex>(n_a)` 仅锁 `pool[a].emplace_back`. 末段 `post_sort` 单线程 per-A `sort by v` 串行.
- Phase 2: 任务粒度 = a (≈ 5500). 每 worker 独占 `T.ts_row(*, a)` 段写入, 无写冲突. FEATURES[] 顺序 = F 枚举顺序 = 计算顺序, 后段读前段输出 (例如 `low_p` 读 `close_raw`, `revenue_st` 读 `rev_raw`/`mb`).
- Phase 3: 任务粒度 = d (≈ 2750). 每 worker 独占 cs_row 段; thread-local 3 buffer (length=n_a) 复用. CS 项之间互不依赖, 串行调用仅为简洁.
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
