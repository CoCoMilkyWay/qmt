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
│   │   ├── misc/                    # 通用工具 (date / fs / logging / progress / timer)
│   │   ├── package/yyjson/          # JSON 库
│   │   └── tushare/                 # tushare 子系统头文件
│   └── src/
│       ├── main.cpp                 # tushare::update(start, today, SPECS, lookback)
│       └── tushare/
│           ├── http.cpp             # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│           ├── spec.cpp             # 13 个 SPECS + RangeStrategy / PerDayStrategy
│           ├── store.cpp            # scan_missing / write_by_visible_date (PK upsert + _empty.json)
│           ├── meta.cpp             # refresh_stock_basic + refresh_index_member_all + 单 API 去重 (lastupdate 时间戳)
│           └── pipeline.cpp         # scan → plan → fetch → write 主流程 (入口逐 API 走 lastupdate 去重)
├── data/                            # tushare 落地 (按 visible_date 切日, gitignored)
│   ├── _meta/
│   │   ├── stock_basic.json         # 全局 meta: ts_code 全量 (L+D+P+G), 每次 update 覆盖刷新
│   │   ├── index_member_all.json    # 申万 SW2021 行业成分 (is_new=Y), 按 L1 分批合并 (PK=ts_code)
│   │   └── <api>.lastupdate         # 单 API 去重时间戳 (unix epoch s); 上次成功距今 < config::API_DEDUP_WINDOW_SECONDS 跳过
│   └── YYYY/
│       └── MM/
│           ├── _empty.json          # 反向稀疏标记 {itf: [DD,...]} = 拉过且为空
│           └── DD/<itf>.json        # 仅在该天有数据时存在 (PK 唯一, 路径 = visible_date)
│                                    # 三态: file 存在 / 在 _empty / 都不在 = 有数据 / 拉过空 / 未拉
│                                    # itf ∈ {forecast, express, disclosure, st, calendar, dividend,
│                                    #        daily_basic, adj_factor, stk_limit, suspend_d,
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
- kind:
  - `filter` (1=排除该 D-A)
  - `factor` (∈[0,1] NaN=不参与)
  - `inter` (中间量)

## 数据源 (非张量, 仅作为输入)

- 按 `visible_date` 落到 `data/YYYY/MM/DD/<itf>.json` (日历日, 周末/节假日同样写盘; visible_date 来源字段见表)
- 入库时机源: `doc/tushare/help/数据更新说明.md` + 各 API 自身 doc; tushare 用语 "实时更新" = 随公告到达即落库无固定时点, "定期更新" = 周期性批发非事件驱动. 
- 公告披露时段背景见 `doc/exchange/公告类别和发布时间.md` (SSE/SZSE 各时段 + 非交易日 13-17 / 12-16 直通).

实盘环路: 交易日 T 盘中, 信号计算前 1 分钟刷一次库, 然后算 signal. row D=T 的可见集 ≜ `{ records | visible_date ≤ T + offset(itf) }`, **offset 单位 = 日历日**, 含周末/节假日.

`offset(itf)` 取值原则 (见表 `偏移` 列):
- 入库时点 < 信号时点 (盘前已到位): `0` — T 当日记录可见
- 入库时点 ≥ 信号时点 (盘后) / "实时" / "不定期": `−1` — 截至 T−1 calendar; 自然把 T-1 盘后 + T-1 至 T 之间的周末/节假日公告 (含 SSE/SZSE 非交易日直通时段) 划入 T 行

入张量映射 (按类):
- **网格** (D=trade_date, A=ts_code): 每条记录唯一 (D, A) 单元. row D 取 `max{ trade_date ≤ T + offset }` (offset=0 → 自身; offset=−1 → 上一交易日, 周末/假日 visible_date 不存在自动跳过).
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见 §字段表 deps (例: `forecast/express/income/cashflow/fina_indicator` 按 `end_date`, `dividend` 按 `(end_date, div_proc)`). 状态机型 (`profit_st / risk_warn`) 同样按此 cutoff 回放 visible_date 升序流.
- **asset 静态**: `_meta/stock_basic` 全量 snapshot 广播到 (D, A); `list_date / delist_date` 决定 (D, A) 行有效, 不走 visible_date.
- **axis**: `calendar` 仅生成 D 轴 (`is_open=1` ∩ `exchange ∈ {SSE, SZSE}`); `_meta/stock_basic` 全量 ts_code 生成 A 轴.

回测/实盘一致性: 同 PK 同次响应末条胜 (PK 因 itf 而异, 见 `cpp/src/tushare/spec.cpp`); 跨次修正写新 day-file, 旧版本留存不被覆盖 → replay 任意 T 按上述 cutoff 自动选当时可见版本. 已知 best-effort 瑕疵 (不可消除, 接受):
1. 公告级时间戳缺失 → 同 visible_date 内盘前/盘中/盘后无法区分, 统一按盘后保守 → 计入 next-day cutoff (实盘错过 T 当日盘前直通公告, 与回测一致).
2. `_meta/stock_basic` 仅当前 snapshot → `market` (主板/创业板/...) 历史变更无法回溯, 转板股全期按当前归类.
3. `suspend_d` tushare 标 "不定期", `offset=0` 假设盘前到位 (停牌公告绝大多数盘前发); 极端情况下 T 盘中刚发布的停牌可能在信号刷库时尚未入 tushare, 想绝对保守可改 −1.

| 类    | itf               | api                  | 入库时机 (tushare)             | visible_date              | 偏移 |
| ----- | ----------------- | -------------------- | ------------------------------ | ------------------------- | ---- |
| axis  | calendar          | `trade_cal`          | 定期 (新年度排程)              | `cal_date`                | —    |
| asset | _meta/stock_basic | `stock_basic`        | 每次 update 覆盖刷新 (L+D+P+G) | —                         | —    |
| 网格  | daily_basic       | `daily_basic`        | **盘后** 交易日 15:00–17:00    | `trade_date`              | −1   |
| 网格  | stk_limit         | `stk_limit`          | **盘前** 交易日 08:40 左右     | `trade_date`              | 0    |
| 网格  | adj_factor        | `adj_factor`         | **盘前** 交易日 09:15–09:20    | `trade_date`              | 0    |
| 网格  | suspend_d         | `suspend_d`          | 不定期 (停牌通常盘前发)        | `trade_date`              | 0    |
| 事件  | forecast          | `forecast_vip`       | 公告实时                       | `ann_date`                | −1   |
| 事件  | express           | `express_vip`        | 公告实时                       | `ann_date`                | −1   |
| 事件  | disclosure        | `disclosure_date`    | 定期 (季前发布披露计划)        | `actual_date`             | −1   |
| 事件  | dividend          | `dividend`           | 公告实时 (预案/通过/实施)      | `imp_ann_date / ann_date` | −1   |
| 事件  | st                | `st`                 | 公告实时 (含 ST/*ST 上下线)    | `pub_date`                | −1   |
| 事件  | fina_indicator    | `fina_indicator_vip` | 公告实时 (随财报)              | `ann_date`                | −1   |
| 事件  | income            | `income_vip`         | 公告实时 (随财报)              | `ann_date`                | −1   |
| 事件  | cashflow          | `cashflow_vip`       | 公告实时 (随财报)              | `ann_date`                | −1   |

## 字段表

排序: filter → factor → inter; inter 内部按 causal (raw → derived); 相关字段就近. `assumption` 列 `—` = 定义自洽; 形如 `[元]` `[%]` `[ratio]` `[股]` 的方括号前缀标注 inter 输出单位.

`deps` 列约定: `itf:<name>` ≡ 该 itf 经 §入张量统一规则 切到 (D, A); 其它为 inter / filter feature 名.

`轴` 列: `时序` = per A 沿 D 计算 (无截面依赖, A 维可并行); `截面` = per D 沿 A 计算 (有截面依赖, A 维不可并行).

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm4** (高季节性, 4 报告期 ≡ 1 年): tushare 原生 TTM 字段直接取 (pe / ps / dy); YTD 累计字段 (income / cashflow / fina_indicator) 用 helper `ttm4_ytd(X) := X(t) + X(Y-1, 12) − X(Y-1, t.M)` 拼接, 其中 t = (Y, M) 为最新可见 `end_date`. 严格成立于流量字段 (revenue, n_cashflow_act); 对比率字段 (roe, roa) 是常用近似 (分子严格, 分母年内变动忽略).
- **ttm1** (低季节性, 单期 snapshot ≡ MRQ): 取 tushare 原生 (pb).

| kind   | feature     | 轴   | deps                                      | formula                                                                                                                   | assumption                                                                                                                                                                            |
| ------ | ----------- | ---- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| filter | profit_st   | 时序 | itf:forecast, itf:disclosure              | 状态机 (per A):                                                                                                           | —                                                                                                                                                                                     |
|        |             |      |                                           | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.last_parent_net < 0` 时                              |                                                                                                                                                                                       |
|        |             |      |                                           | 按 `forecast.ann_date` 触发, 至 `disclosure.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止                 |                                                                                                                                                                                       |
| filter | revenue_st  | 时序 | itf:forecast, itf:disclosure, rev_raw, mb | 状态机 (per A) ∧ `mb ∧ rev_raw < (3e8 if end_date.Y ≥ 2024 else 1e8)`:                                                    | —                                                                                                                                                                                     |
|        |             |      |                                           | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101` 时 |                                                                                                                                                                                       |
|        |             |      |                                           | 按 `forecast.ann_date` 触发, 至 `disclosure.actual_date` 或 `(forecast.end_date.Y + 1, 4, monthend)` 终止                 |                                                                                                                                                                                       |
| filter | dividend_st | 时序 | itf:dividend, ni_raw, share_raw, mb       | `mb ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_div_tax × share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7`                              | 3y 窗口 = `dividend.end_date.Y ∈ [Y-3, Y-1]` (Y = `dividend.ann_date.Y`); share_raw 取 `dividend.ann_date` 当日快照 (除权后, 实际派发若在大额送转前后会有偏差, 业务接受); 单位均 [元] |
| filter | trading_st  | 时序 | low_p, low_mc                             | `rolling_20D(low_p ∨ low_mc).all()`                                                                                       | —                                                                                                                                                                                     |
| filter | risk_warn   | 时序 | itf:st                                    | 状态机 (per A): 按 `st.pub_date` 升序回放 `st.name` (变更后名), 含 "ST" 子串则上线, 不含则下线                            | A 股监管命名规则: ST 状态名必含 "ST" 子串 (含 *ST/SST 等变体), 撤销摘帽后名不含 "ST"                                                                                                  |
| filter | new_list    | 时序 | list_age                                  | `list_age < 60`                                                                                                           | —                                                                                                                                                                                     |
| factor | close       | 截面 | close_raw                                 | `pct_rank(z(winsor_mad(1 / close_raw)))`                                                                                  | —                                                                                                                                                                                     |
| factor | mcap        | 截面 | mcap_raw                                  | `pct_rank(z(winsor_mad(1 / mcap_raw)))`                                                                                   | —                                                                                                                                                                                     |
| factor | fmcap       | 截面 | fmcap_raw                                 | `pct_rank(z(winsor_mad(1 / fmcap_raw)))`                                                                                  | —                                                                                                                                                                                     |
| factor | pe_ttm4     | 截面 | pe_raw                                    | `pct_rank(z(winsor_mad(1 / pe_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | pb_ttm1     | 截面 | pb_raw                                    | `pct_rank(z(winsor_mad(1 / pb_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | ps_ttm4     | 截面 | ps_raw                                    | `pct_rank(z(winsor_mad(1 / ps_raw)))`                                                                                     | —                                                                                                                                                                                     |
| factor | pcf_ttm4    | 截面 | pcf_raw                                   | `pct_rank(z(winsor_mad(1 / pcf_raw)))`                                                                                    | —                                                                                                                                                                                     |
| factor | roe_ttm4    | 截面 | roe_raw                                   | `pct_rank(z(winsor_mad(roe_raw)))`                                                                                        | —                                                                                                                                                                                     |
| factor | roa_ttm4    | 截面 | roa_raw                                   | `pct_rank(z(winsor_mad(roa_raw)))`                                                                                        | —                                                                                                                                                                                     |
| factor | dy_ttm4     | 截面 | dy_raw                                    | `pct_rank(z(winsor_mad(dy_raw)))`                                                                                         | —                                                                                                                                                                                     |
| inter  | close_raw   | 时序 | itf:daily_basic                           | `daily_basic.close` (前复权)                                                                                              | [元/股]                                                                                                                                                                               |
| inter  | up_lim      | 时序 | itf:stk_limit                             | `stk_limit.up_limit` (前复权)                                                                                             | [元/股]                                                                                                                                                                               |
| inter  | dn_lim      | 时序 | itf:stk_limit                             | `stk_limit.down_limit` (前复权)                                                                                           | [元/股]                                                                                                                                                                               |
| inter  | susp        | 时序 | itf:suspend_d                             | `1.0 if itf:suspend_d (D, A) 存在 else 0.0`                                                                               | [bool]                                                                                                                                                                                |
| inter  | mcap_raw    | 时序 | itf:daily_basic                           | `daily_basic.total_mv × 1e4`                                                                                              | [元]; 万元 → 元                                                                                                                                                                       |
| inter  | fmcap_raw   | 时序 | itf:daily_basic                           | `daily_basic.circ_mv × 1e4`                                                                                               | [元]; 万元 → 元                                                                                                                                                                       |
| inter  | share_raw   | 时序 | itf:daily_basic                           | `daily_basic.total_share × 1e4`                                                                                           | [股]; 万股 → 股                                                                                                                                                                       |
| inter  | pe_raw      | 时序 | itf:daily_basic                           | `daily_basic.pe_ttm`                                                                                                      | [ratio]; ttm4 (tushare 原生); 亏损 → NaN                                                                                                                                              |
| inter  | pb_raw      | 时序 | itf:daily_basic                           | `daily_basic.pb`                                                                                                          | [ratio]; ttm1 (tushare 原生 MRQ)                                                                                                                                                      |
| inter  | ps_raw      | 时序 | itf:daily_basic                           | `daily_basic.ps_ttm`                                                                                                      | [ratio]; ttm4 (tushare 原生)                                                                                                                                                          |
| inter  | dy_raw      | 时序 | itf:daily_basic                           | `daily_basic.dv_ttm`                                                                                                      | [%]; ttm4 (tushare 原生)                                                                                                                                                              |
| inter  | pcf_raw     | 时序 | itf:cashflow, mcap_raw                    | `mcap_raw / ttm4_ytd(cashflow.n_cashflow_act)`                                                                            | [ratio]; ttm4 (YTD 拼接); 流量字段严格; 任一期缺 → NaN                                                                                                                                |
| inter  | roe_raw     | 时序 | itf:fina_indicator                        | `ttm4_ytd(fina_indicator.roe)`                                                                                            | [%]; ttm4 (YTD 拼接, 取 `fina_indicator.roe` 不切其他变体 `roe_waa` 加权 / `roe_dt` 扣非 / `roe_yearly` 年化);<br>比率近似 (分母年内忽略);<br>任一期缺 → NaN                          |
| inter  | roa_raw     | 时序 | itf:fina_indicator                        | `ttm4_ytd(fina_indicator.roa)`                                                                                            | [%]; ttm4 (YTD 拼接, 取 `fina_indicator.roa` 不切其他变体 `roa_yearly` 年化 / `roa_dp` 杜邦);<br>比率近似 (分母年内忽略);<br>任一期缺 → NaN                                           |
| inter  | rev_raw     | 时序 | itf:income                                | `ttm4_ytd(income.revenue)`                                                                                                | [元]; ttm4 (YTD 拼接); 流量字段严格; 任一期缺 → NaN                                                                                                                                   |
| inter  | ni_raw      | 时序 | itf:income                                | `mean(income.n_income_attr_p where income.end_date.M == 12, last 2 records)`                                              | [元]; 严格只取 `income.end_date.M == 12` 年报记录, 不混 forecast / express                                                                                                            |
| inter  | mb          | 时序 | _meta/stock_basic                         | `_meta/stock_basic.market == '主板'`                                                                                      | [bool]; asset 静态广播 (D, A)                                                                                                                                                         |
| inter  | list_age    | 时序 | _meta/stock_basic                         | `D − _meta/stock_basic.list_date`                                                                                         | [日历日]                                                                                                                                                                              |
| inter  | low_p       | 时序 | close_raw                                 | `close_raw < 1.0`                                                                                                         | [bool]                                                                                                                                                                                |
| inter  | low_mc      | 时序 | mcap_raw, mb                              | `mcap_raw < (5e8 if mb else 3e8)`                                                                                         | [bool]                                                                                                                                                                                |
| inter  | limit_up    | 时序 | close_raw, up_lim                         | `close_raw ≥ up_lim − 1e-4`                                                                                               | [bool]; 策略涨停判定                                                                                                                                                                  |
| inter  | limit_dn    | 时序 | close_raw, dn_lim                         | `close_raw ≤ dn_lim + 1e-4`                                                                                               | [bool]; 策略跌停判定                                                                                                                                                                  |
| inter  | pool_b      | 时序 | mb, susp, _meta/stock_basic               | `_meta/stock_basic.exchange ∈ {SSE, SZSE} ∧ mb ∧ ¬susp`                                                                   | [bool]; basic pool, 当前 strategy 仅主板                                                                                                                                              |
| inter  | pool        | 截面 | pool_b, mcap_raw                          | `pool_b ∧ rank(mcap_raw asc) ≤ UNIVERSE_SIZE` (per D, within `pool_b`; 默认 UNIVERSE_SIZE = 80)                           | [bool]; 最终 strategy universe                                                                                                                                                        |
