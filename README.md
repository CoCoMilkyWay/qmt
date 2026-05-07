Motive: 实盘量化交易; 国金证券 QMT 客户端下单, tushare 维护本地财报披露时间线
Insight: 研究/回测在 bq/ 仓库; 此处只负责实盘执行 + tushare 数据本地化

qmt/
├── run.py                           # 统一入口: build (py/main.py) + run (py/mode_*.py)
├── app/
│   ├── gjzqqmt/                     # 国金证券 QMT 客户端 (Linux Wine 跑 Windows 程序)
│   │   ├── run.md                   # Wine 安装 + 启动指南 (XtItClient=主端, XtMiniQmt=API端)
│   │   ├── 国金证券QMT交易端/         # 客户端本体 (bin.x64 是 64 位主程序)
│   │   └── QMT操作说明文档/           # 官方 PDF (操作/Python API/网格/VBA/算法交易)
│   └── api/tushare/                 # tushare Python SDK 副本 (历史保留, 不再用)
├── cpp/                             # C++23 实现 (Clang/Linux, header-only boost + yyjson)
│   ├── projects/main/               # CMake 构建 (DEBUG / PROFILE / ASSERT / PRODUCTION)
│   ├── include/
│   │   ├── config.hpp               # 全局常量 (token, API host, lookback, 拉取窗口)
│   │   ├── misc/                    # 通用工具 (date / fs / logging / progress / timer)
│   │   ├── package/yyjson/          # JSON 库
│   │   └── tushare/                 # tushare 子系统头文件
│   └── src/
│       ├── main.cpp                 # tushare::update(start, today, SPECS, lookback)
│       └── tushare/
│           ├── http.cpp             # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│           ├── spec.cpp             # 7 个 SPECS + RangeStrategy / PerDayStrategy
│           ├── store.cpp            # scan_missing / write_by_visible_date (PK upsert + _empty.json)
│           ├── meta.cpp             # refresh_stock_basic: 全局 meta 全量刷新 (L+D+P+G)
│           └── pipeline.cpp         # scan → plan → fetch → write 主流程
├── data/                            # tushare 落地 (按 visible_date 切日, gitignored)
│   ├── _meta/
│   │   └── stock_basic.json         # 全局 meta: ts_code 全量 (L+D+P+G), 每次 update 覆盖刷新
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
- dtype: 统一 **f8** (8-bit float, 张量并行加速; bool 用 0.0/1.0)
- kind: `filter` (1=排除该 D-A) / `factor` (∈[0,1] 截面 pct rank, NaN=不参与) / `inter` (中间量) / `score` (聚合输出)
- st: `✓` 可由 `/data` 直接算 / `△` 口径需 sanity-check / `✗` 缺源, 需补 `cpp/.../spec.cpp` SPECS

## 数据源 (非张量, 仅作为输入)

- daily 网格: `itf=daily_basic / stk_limit / suspend_d` 落到 (D, A); **`daily_basic.close` 与 `stk_limit.up_limit / down_limit` 已是前复权, 不需 `adj_factor`**
- 事件 sparse: `itf=forecast / disclosure / st / dividend / fina_indicator / income / cashflow` 按 ann_date / pub_date / actual_date 落库, 经状态机或 PIT-ffill → (D, A)
- asset 静态: `_meta/stock_basic.{list_date, market, exchange, industry}`, 无 D 轴, 广播到 (D, A)

## 字段表

排序: filter → factor → inter → score; inter 内部按 causal 顺序 (raw → derived); 相关字段就近.

| kind   | feature     | deps                                                                | formula                                                                                                                                     | st  |
| ------ | ----------- | ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- | --- |
| filter | profit_st   | forecast_e, disclosure_e                                            | 状态机: forecast(end_date 12月, type∈{首亏,续亏}, last_parent_net<0) 触发 ann_date; 至 disclosure.actual_date 或 (year+1, 4, monthend) 终止 | ✓   |
| filter | revenue_st  | profit_st, rev_raw, mb                                              | profit_st 同区间 ∩ `rev_raw < (3e8 if mb AND year≥2024 else 1e8)`; 仅 `report_year≥2021 AND ann_date≥20210101`                              | △   |
| filter | dividend_st | dividend_e, ni_raw, mb                                              | 仅 mb; `3y_sum(cash_div_tax × total_share) < 0.30 × ni_raw` AND `3y_sum < 5e7`                                                              | △   |
| filter | trading_st  | low_p, low_mc                                                       | rolling 20D: `(low_p OR low_mc).all_over_window(20)`                                                                                        | ✓   |
| filter | risk_warn   | st_e                                                                | 状态机: 按 pub_date 升序回放, st_tpye 含"撤销/摘帽"下线, 否则上线                                                                           | △   |
| filter | new_list    | list_age                                                            | `list_age < 60` (日历日)                                                                                                                    | ✓   |
| factor | close       | close_raw, trade                                                    | per D in trade: `pct_rank(z(winsor_mad(1/close_raw)))` (越小越优 → 取倒数)                                                                  | ✓   |
| factor | mcap        | mcap_raw, trade                                                     | 同 close (1/mcap_raw)                                                                                                                       | ✓   |
| factor | fmcap       | fmcap_raw, trade                                                    | 同 close (1/fmcap_raw)                                                                                                                      | ✓   |
| factor | pe          | pe_raw, trade                                                       | 同 close (1/pe_raw)                                                                                                                         | △   |
| factor | pb          | pb_raw, trade                                                       | 同 close (1/pb_raw)                                                                                                                         | △   |
| factor | ps          | ps_raw, trade                                                       | 同 close (1/ps_raw)                                                                                                                         | △   |
| factor | pcf         | pcf_raw, trade                                                      | 同 close (1/pcf_raw)                                                                                                                        | △   |
| factor | roe         | roe_raw, trade                                                      | per D in trade: `pct_rank(z(winsor_mad(roe_raw)))` (越大越优, identity)                                                                     | △   |
| factor | roa         | roa_raw, trade                                                      | 同 roe (identity)                                                                                                                           | △   |
| factor | dy          | dy_raw, trade                                                       | 同 roe (identity)                                                                                                                           | △   |
| inter  | close_raw   | itf:daily_basic                                                     | `close` (前复权)                                                                                                                            | ✓   |
| inter  | up_lim      | itf:stk_limit                                                       | `up_limit` (前复权)                                                                                                                         | ✓   |
| inter  | dn_lim      | itf:stk_limit                                                       | `down_limit` (前复权)                                                                                                                       | ✓   |
| inter  | susp        | itf:suspend_d                                                       | (D, A) 行存在 → 1                                                                                                                           | ✓   |
| inter  | mcap_raw    | itf:daily_basic                                                     | `total_mv × 1e4` (元)                                                                                                                       | ✓   |
| inter  | fmcap_raw   | itf:daily_basic                                                     | `circ_mv × 1e4` (元)                                                                                                                        | ✓   |
| inter  | pe_raw      | itf:daily_basic                                                     | `pe_ttm`                                                                                                                                    | △   |
| inter  | pb_raw      | itf:daily_basic                                                     | `pb`                                                                                                                                        | △   |
| inter  | ps_raw      | itf:daily_basic                                                     | `ps_ttm`                                                                                                                                    | △   |
| inter  | dy_raw      | itf:daily_basic                                                     | `dv_ttm` (% TTM 股息率)                                                                                                                     | △   |
| inter  | pcf_raw     | itf:cashflow                                                        | `mcap_raw / rolling_sum(cashflow.n_cashflow_act, 4Q)`                                                                                       | △   |
| inter  | roe_raw     | itf:fina_indicator                                                  | `roe_yearly` 按 ann_date 升序 ffill 到 D (PIT)                                                                                              | △   |
| inter  | roa_raw     | itf:fina_indicator                                                  | `roa_yearly` 同上                                                                                                                           | △   |
| inter  | rev_raw     | itf:income                                                          | `rolling_sum(revenue, 4Q)` ffill 到 D                                                                                                       | △   |
| inter  | ni_raw      | itf:income                                                          | 近 2 年年报 (end_date 12月) `n_income_attr_p` 平均, ffill 到 D                                                                              | △   |
| inter  | mb          | _meta/stock_basic                                                   | `market == '主板'` (asset 静态广播)                                                                                                         | ✓   |
| inter  | list_age    | _meta/stock_basic                                                   | `D − list_date` (日历日)                                                                                                                    | ✓   |
| inter  | low_p       | close_raw                                                           | `close_raw < 1.0`                                                                                                                           | ✓   |
| inter  | low_mc      | mcap_raw, mb                                                        | `mcap_raw < (5e8 if mb else 3e8)`                                                                                                           | ✓   |
| inter  | limit_up    | close_raw, up_lim                                                   | `close_raw ≥ up_lim − 1e-4` (策略涨停判定)                                                                                                  | ✓   |
| inter  | limit_dn    | close_raw, dn_lim                                                   | `close_raw ≤ dn_lim + 1e-4` (策略跌停判定)                                                                                                  | ✓   |
| inter  | pool_b      | mb, susp + (exchange)                                               | `exchange ∈ {SSE, SZSE} AND mb AND NOT susp` (basic pool, 当前 strategy 仅主板)                                                             | ✓   |
| inter  | pool        | pool_b, mcap_raw                                                    | `pool_b AND row_number(over D order by mcap_raw asc) ≤ UNIVERSE_SIZE` (默认 80)                                                             | ✓   |
| inter  | block       | profit_st, revenue_st, dividend_st, trading_st, risk_warn, new_list | 任一 filter = 1 → 1                                                                                                                         | △   |
| inter  | trade       | pool, block                                                         | `pool AND NOT block`                                                                                                                        | ✓   |
| score  | score       | 全部 factor + FACTOR_WEIGHTS                                        | per D in trade: `Σ_f w_f · factor_f / Σ_f w_f · 1{factor_f≠NaN}` (可用因子权重归一)                                                         | ✓   |

## 口径警告

1. 单位: `daily_basic.total_mv / circ_mv` 万元, ×1e4 转元
2. 股息率: `dy_raw = daily_basic.dv_ttm` (TTM 现金分红 / 最新价) vs bigquant `dividend_yield_ratio` — 横截面 sanity-check
3. TTM ROE/ROA: `roe_raw / roa_raw` 用 `fina_indicator.roe_yearly / roa_yearly` (tushare 自身 TTM 化), 与 bigquant `roe_avg_ttm` (4Q 滚动平均) 不一定逐值对齐
4. 行业: `stock_basic.industry` 是 tushare 自有口径, ≠ 申万 2021. `filter.SW2021_ALL_INDUSTRIES` 过滤逻辑无 tushare 等价品, 需重新设计
5. PIT: `daily_basic` D 日盘后 15-17 入库, `stk_limit` D 日盘前 8:40 入库 — 严格 PIT 要求 D-1 日 `daily_basic` 进入 D 日决策; 当前 strategy 用 D 日 `daily_basic` 配 D 日 close 撮合 (引擎口径自洽). 实盘需重新校
