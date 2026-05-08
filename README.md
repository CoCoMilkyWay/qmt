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
- kind: 
  - `filter` (1=排除该 D-A) 
  - `factor` (∈[0,1] 截面 pct rank, NaN=不参与) 
  - `inter` (中间量)

## 数据源 (非张量, 仅作为输入)

按 `visible_date` 落到 `data/YYYY/MM/DD/<itf>.json`. 入张量方式:
- **网格** (D, A): `trade_date` 直接索引, 一交易日一行 × 全市场 ~5000 股
- **事件** sparse: `ann_date / pub_date / actual_date` 索引, 不定期, 经状态机或 PIT-ffill 还原 (D, A)
- **asset** 静态: 无 D 轴, 广播到 (D, A); **axis** 仅生成 D / A 轴, 不入张量

入库时机来源: `doc/tushare/help/数据更新说明.md` + 各 API 自身 doc; tushare 用语 "实时更新" = 随公告/财报到达即入库, 无固定时点; "定期更新" = 周期性批发, 非事件驱动.

`偏移` 列假设: (a) 信号计算前 1 分钟刷一次库, (b) 信号在交易日 T 盘中算. 含义 = D=T 行用 visible_date = T + offset 的数据 (网格直接索引, 事件 PIT 截断 ≤ T + offset). 事件类统一 −1: 多数公告盘后发, 实盘 T 日盘中拿不到 T 当日公告, 截至 T−1 保守.

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

排序: filter → factor → inter; inter 内部按 causal 顺序 (raw → derived); 相关字段就近.

| kind   | feature     | deps                     | formula                                                                                                                                     | st                                                                           |
| ------ | ----------- | ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| filter | profit_st   | forecast_e, disclosure_e | 状态机: forecast(end_date 12月, type∈{首亏,续亏}, last_parent_net<0) 触发 ann_date; 至 disclosure.actual_date 或 (year+1, 4, monthend) 终止 | ✓                                                                            |
| filter | revenue_st  | profit_st, rev_raw, mb   | profit_st 同区间 ∩ `rev_raw < (3e8 if mb AND year≥2024 else 1e8)`; 仅 `report_year≥2021 AND ann_date≥20210101`                              | 阈值 3e8/1e8 (主板新规) 触发时点对样本; rev_raw 见下                         |
| filter | dividend_st | dividend_e, ni_raw, mb   | 仅 mb; `3y_sum(cash_div_tax × total_share) < 0.30 × ni_raw` AND `3y_sum < 5e7`                                                              | 3y 窗口按公告年度还是财季; total_share 用除权前还是除权后时点                |
| filter | trading_st  | low_p, low_mc            | rolling 20D: `(low_p OR low_mc).all_over_window(20)`                                                                                        | ✓                                                                            |
| filter | risk_warn   | st_e                     | 状态机: 按 pub_date 升序回放, st_tpye 含"撤销/摘帽"下线, 否则上线                                                                           | st_tpye 撤销/摘帽 / *ST 上下线 枚举集合需对样本                              |
| filter | new_list    | list_age                 | `list_age < 60` (日历日)                                                                                                                    | ✓                                                                            |
| factor | close       | close_raw, trade         | per D in trade: `pct_rank(z(winsor_mad(1/close_raw)))` (越小越优 → 取倒数)                                                                  | ✓                                                                            |
| factor | mcap        | mcap_raw, trade          | 同 close (1/mcap_raw)                                                                                                                       | ✓                                                                            |
| factor | fmcap       | fmcap_raw, trade         | 同 close (1/fmcap_raw)                                                                                                                      | ✓                                                                            |
| factor | pe          | pe_raw, trade            | 同 close (1/pe_raw)                                                                                                                         | 见 pe_raw                                                                    |
| factor | pb          | pb_raw, trade            | 同 close (1/pb_raw)                                                                                                                         | 见 pb_raw                                                                    |
| factor | ps          | ps_raw, trade            | 同 close (1/ps_raw)                                                                                                                         | 见 ps_raw                                                                    |
| factor | pcf         | pcf_raw, trade           | 同 close (1/pcf_raw)                                                                                                                        | 见 pcf_raw                                                                   |
| factor | roe         | roe_raw, trade           | per D in trade: `pct_rank(z(winsor_mad(roe_raw)))` (越大越优, identity)                                                                     | 见 roe_raw                                                                   |
| factor | roa         | roa_raw, trade           | 同 roe (identity)                                                                                                                           | 见 roa_raw                                                                   |
| factor | dy          | dy_raw, trade            | 同 roe (identity)                                                                                                                           | 见 dy_raw                                                                    |
| inter  | close_raw   | itf:daily_basic          | `close` (前复权)                                                                                                                            | ✓                                                                            |
| inter  | up_lim      | itf:stk_limit            | `up_limit` (前复权)                                                                                                                         | ✓                                                                            |
| inter  | dn_lim      | itf:stk_limit            | `down_limit` (前复权)                                                                                                                       | ✓                                                                            |
| inter  | susp        | itf:suspend_d            | (D, A) 行存在 → 1                                                                                                                           | ✓                                                                            |
| inter  | mcap_raw    | itf:daily_basic          | `total_mv × 1e4` (元)                                                                                                                       | ✓                                                                            |
| inter  | fmcap_raw   | itf:daily_basic          | `circ_mv × 1e4` (元)                                                                                                                        | ✓                                                                            |
| inter  | pe_raw      | itf:daily_basic          | `pe_ttm`                                                                                                                                    | tushare pe_ttm 负利润是否置 NaN; 横截面对齐 bq                               |
| inter  | pb_raw      | itf:daily_basic          | `pb`                                                                                                                                        | LF 还是 MRQ; 横截面对齐 bq                                                   |
| inter  | ps_raw      | itf:daily_basic          | `ps_ttm`                                                                                                                                    | TTM 窗口对齐 bq                                                              |
| inter  | dy_raw      | itf:daily_basic          | `dv_ttm` (% TTM 股息率)                                                                                                                     | dv_ttm 横截面对齐 bq `dividend_yield_ratio`                                  |
| inter  | pcf_raw     | itf:cashflow             | `mcap_raw / rolling_sum(cashflow.n_cashflow_act, 4Q)`                                                                                       | 字段名 `n_cashflow_act` vs `n_cashflow_op_act`; 4Q 缺一回退策略              |
| inter  | roe_raw     | itf:fina_indicator       | `roe_yearly` 按 ann_date 升序 ffill 到 D (PIT)                                                                                              | `roe_yearly` (tushare 自身年度化) ≠ bq `roe_avg_ttm` (4Q 滚动平均), 不逐值齐 |
| inter  | roa_raw     | itf:fina_indicator       | `roa_yearly` 同上                                                                                                                           | 同 roe_raw                                                                   |
| inter  | rev_raw     | itf:income               | `rolling_sum(revenue, 4Q)` ffill 到 D                                                                                                       | 4Q 缺一回退策略; revenue 含税口径                                            |
| inter  | ni_raw      | itf:income               | 近 2 年年报 (end_date 12月) `n_income_attr_p` 平均, ffill 到 D                                                                              | 仅取 12月年报, 不混 forecast/express                                         |
| inter  | mb          | _meta/stock_basic        | `market == '主板'` (asset 静态广播)                                                                                                         | ✓                                                                            |
| inter  | list_age    | _meta/stock_basic        | `D − list_date` (日历日)                                                                                                                    | ✓                                                                            |
| inter  | low_p       | close_raw                | `close_raw < 1.0`                                                                                                                           | ✓                                                                            |
| inter  | low_mc      | mcap_raw, mb             | `mcap_raw < (5e8 if mb else 3e8)`                                                                                                           | ✓                                                                            |
| inter  | limit_up    | close_raw, up_lim        | `close_raw ≥ up_lim − 1e-4` (策略涨停判定)                                                                                                  | ✓                                                                            |
| inter  | limit_dn    | close_raw, dn_lim        | `close_raw ≤ dn_lim + 1e-4` (策略跌停判定)                                                                                                  | ✓                                                                            |
| inter  | pool_b      | mb, susp + (exchange)    | `exchange ∈ {SSE, SZSE} AND mb AND NOT susp` (basic pool, 当前 strategy 仅主板)                                                             | ✓                                                                            |
| inter  | pool        | pool_b, mcap_raw         | `pool_b AND row_number(over D order by mcap_raw asc) ≤ UNIVERSE_SIZE` (默认 80)                                                             | ✓                                                                            |

## 口径警告

1. 单位: `daily_basic.total_mv / circ_mv` 万元, ×1e4 转元
2. 行业: `stock_basic.industry` 是 tushare 自有口径, ≠ 申万 2021. `filter.SW2021_ALL_INDUSTRIES` 过滤逻辑无 tushare 等价品, 需重新设计
