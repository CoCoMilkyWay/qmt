# Tushare 数据验证方案

**目的**: 不替换 tushare; 用 bigquant DAI 作旁路校对源, 对 `data/YYYY/MM/DD/<itf>.json` 历史落地做字段值/集合一致性审计.

**背景**: 见 [../compare.md](../compare.md) (字段映射表) 与 [../bigquant/probe.py](../bigquant/probe.py) (接口探活, 27/28 通过, `cn_stock_valuation` 无权限).

## 工作流 (每个脚本统一形态)

```
1. 选定一组 (date, ts_code) 抽样
2. 加载 tushare 落地: data/YYYY/MM/DD/<itf>.json
3. bigquant: `bq dai query "<SQL>" --filters '{"date":[D,D]}'`
4. 字段对照, 输出 (a) 集合差异 (b) 字段值差异 (c) 前 N 条样例
5. 任意硬冲突 → assert 失败
```

## 通用约定

- **抽样范围** (全期, 默认): `["2015-01-01", "2025-12-31"]`
  - 第一轮只做 **P1 + P2**; P3 (事件流 + PIT) 标 TODO, 框架跑通后再做
- **抽样股票**: 全市场 (tushare ∪ bigquant 自然集合)
- **失败口径**: assertion, 任何不可解释差异 fail (越早 fail 越好)
- **字段值容差**: 浮点 `|a − b| < eps × max(1, |a|)`, `eps = 1e-6` 默认; 单位换算字段 (万 → 元) 用 `1e-4` 适当放宽
- **缺权限表**: `cn_stock_valuation` (PE/PB/PS/DY 估值簇 + 总市值/流通市值 + turnover_rate_f + volume_ratio) → daily_basic 该部分**不验**
- **bq 调用**: `bq dai query <SQL> --filters '{"date":[F,T]}' --limit 0 -o /tmp/x.csv` → `pd.read_csv` (parquet 需 pyarrow, csv 最稳)

## 数据源映射 + 脚本计划

排序: P1 (网格, 一对一直接比) → P2 (asset/meta 静态) → P3 (事件流 + PIT)

### P1 简单 (网格 itf, PK=(date, ts_code), 字段一一对应)

| # | itf (tushare)    | bigquant table                       | 关键字段映射                                                                                                                                                                                                                                                                | 脚本                                |
| - | ---------------- | ------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------- |
| 1 | `calendar`       | `trading_days`                       | tushare `cal_date,exchange,is_open` (exchange=SSE/SZSE, is_open=1) ↔ bigquant `date` where `market_code='CN'`                                                                                                                                                              | `verify_calendar.py`                |
| 2 | `stk_limit`      | `cn_stock_limit_price`               | `up_limit ↔ upper_limit`, `down_limit ↔ lower_limit` (均未复权)                                                                                                                                                                                                              | `verify_stk_limit.py`               |
| 3 | `suspend_d`      | `cn_stock_suspend`                   | (ts_code, trade_date) 集合一致性 (字段 suspend_type/suspend_timing vs suspend_period/suspend_reason 不直接可比, 仅校验存在性)                                                                                                                                                  | `verify_suspend_d.py`               |
| 4 | `margin_secs`    | `cn_stock_static_data.crd_buy_flag`  | tushare 单日全集合 vs bigquant `crd_buy_flag=1` 子集 (两融名单 = 可融资买入, 等价 tushare margin_secs)                                                                                                                                                                       | `verify_margin_secs.py`             |
| 5 | `margin_detail`  | `cn_stock_margin_trading_detail`     | `rzye ↔ financing_balance`, `rqye ↔ securities_lending_balance`, `rzmre ↔ financing_purchase`, `rzche ↔ financing_repayment` (重点核 rzye + rqye)                                                                                                                          | `verify_margin_detail.py`           |
| 6 | `stock_st`       | `cn_stock_status`                    | tushare `name` 含 '\*' 区分 ST(1)/\*ST(2) ↔ bigquant `st_status` TINYINT 0/1/2 (直接 boolean)                                                                                                                                                                              | `verify_stock_st.py`                |
| 7 | `adj_factor`     | `cn_stock_real_bar1d.adjust_factor`  | 两边均累计因子但基期不同 → 不比绝对值, 比**相邻日比值** `adj[d]/adj[d-1]` 应严格相等 (除权事件位置 + 幅度对齐)                                                                                                                                                                | `verify_adj_factor.py`              |

### P2 中等 (asset/meta 静态 + daily_basic 拆字段)

| # | itf (tushare)              | bigquant table                              | 关键字段映射                                                                                                                                                                                                                                              | 脚本                              |
| - | -------------------------- | ------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------- |
| 8 | `daily_basic.close`        | `cn_stock_real_bar1d.close`                 | 实测两边均**未复权** (tushare daily_basic 2024-12-30 平安银行 close=11.95 ↔ bigquant probe[7] 2024-12-31 pre_close=11.95 验证). 直接 `close ↔ close` 浮点比. 另: `turnover_rate ↔ turn`                                                                       | `verify_daily_basic_close.py`     |
| 9 | `daily_basic.total_share`  | `cn_stock_shares.total_shares`              | `total_share` (万股) × 1e4 ↔ `total_shares` (股)                                                                                                                                                                                                          | `verify_daily_basic_shares.py`    |
|   | `daily_basic.float_share`  | `cn_stock_shares.a_float_shares`            | `float_share` (万股) × 1e4 ↔ `a_float_shares` (股)                                                                                                                                                                                                        | (同上脚本)                         |
|   | `daily_basic.free_share`   | `cn_stock_shares.free_float_shares`         | `free_share` (万股) × 1e4 ↔ `free_float_shares` (股)                                                                                                                                                                                                      | (同上脚本)                         |
|   | `daily_basic.{pe,pe_ttm,pb,ps,ps_ttm,dv_ratio,dv_ttm,total_mv,circ_mv,turnover_rate,turnover_rate_f,volume_ratio}` | (无权限 `cn_stock_valuation`) | **跳过** (license 不开放)                                                                                                                                                                                                                               | —                                  |
| 10 | `_meta/stock_basic`        | `cn_stock_basic_info`                       | ts_code 全集 ∩/∪; `list_date / delist_date` 一致性; `market` (主板/创业板/科创板) ↔ `list_sector` (1/2/3 数值码) 映射                                                                                                                                       | `verify_stock_basic.py`           |
| 11 | `_meta/index_member_all`   | `cn_stock_industry_component`               | bigquant 该表**多分类标准混合** (一股多行, 含中信/自然行业等多 source, l1_code 数字码 1xx-9xx, 与 tushare 申万 SW2021 `801780.SI` 体系完全不同) → **跳过**, 标 TODO                                                                                          | —                                  |
| 12 | `_meta/namechange`         | `cn_stock_name_change`                      | tushare 嵌套 `{ts_code: [{name, start_date}, ...]}` ↔ bigquant 长表 `(instrument, start_date, end_date, name)` → 按 ts_code 比较名称段序列                                                                                                                | `verify_namechange.py`            |

### P3 复杂 (事件 itf + PIT 财报, 字段名映射 + 单位映射)

| #  | itf (tushare)     | bigquant table                                 | 关键字段映射                                                                                                                                                                                                                                                                                                                                                | 脚本                          |
| -- | ----------------- | ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------- |
| 13 | `forecast`        | `cn_stock_profit_estimate`                     | tushare `(ts_code, ann_date, end_date, type, p_change_min/max, last_parent_net)` ↔ bigquant `(instrument, date=ann_date, end_date, fore_type [TINYINT], pct_range_min/max, ...)`. **type ↔ fore_type** 需建枚举映射 (首亏/续亏/预增/扭亏/...)                                                                                                              | `verify_forecast.py`          |
| 14 | `express`         | (无直接对应)                                    | bigquant 仅 `cn_stock_profit_exceed_*` (超预期评估口径, 非快报本体) → **跳过**, 仅记录 gap                                                                                                                                                                                                                                                                  | —                              |
| 15 | `disclosure`      | (无直接对应)                                    | bigquant 仅 `cn_stock_financial_changedate` (实际披露日, 无事先 ann_date 计划) → **跳过 disclosure (计划)**                                                                                                                                                                                                                                                 | —                              |
| 16 | `report`          | `cn_stock_financial_changedate`                | tushare `(ts_code, end_date, actual_date)` ↔ bigquant `(instrument, report_date, changedate)`. 单位: bigquant `statement_type` ∈ {balance_sheet, income, cash_flow} → 同一 (ts_code, end_date, actual_date) 有 3 行 (合并即可)                                                                                                                              | `verify_report.py`            |
| 17 | `dividend`        | `cn_stock_dividend`                            | **仅比 div_proc='实施' 一阶段** (bigquant 无 预案/通过). tushare `(ts_code, end_date, ann_date, imp_ann_date, ex_date, cash_div, cash_div_tax, stk_div)` ↔ bigquant `(instrument, report_date, publish_date, ex_date, cash_before_tax, cash_after_tax, bonus_rate + conversed_rate)`. `stk_div = bonus_rate + conversed_rate`                              | `verify_dividend.py`          |
| 18 | `income`          | `cn_stock_financial_income_general_pit`        | tushare `(ts_code, ann_date, end_date, report_type, revenue, n_income_attr_p, n_income)` ↔ bigquant `(instrument, date=ann_date, report_date, change_type, operating_revenue, net_profit_to_parent_shareholders, net_profit)`. report_type=1 (合并) vs change_type (修订追踪) 不同维度 → 比 `change_type=1` (首次) 或仅取最新                              | `verify_income.py`            |
| 19 | `cashflow`        | `cn_stock_financial_cashflow_general_pit`      | tushare `(ts_code, ann_date, end_date, report_type, n_cashflow_act)` ↔ bigquant `(instrument, date, report_date, change_type, net_cffoa)`                                                                                                                                                                                                                  | `verify_cashflow.py`          |
| 20 | `fina_indicator`  | `cn_stock_financial_profitability`             | tushare YTD 累计 `(roe, roa)` 需 `ttm4_ytd` 自拼 ↔ bigquant 直接 `roe_avg_ttm`/`roa_avg_ttm` → **口径不同, 不直接可比**. 备选: 用 tushare YTD 字段自拼 TTM 后 vs bigquant `_ttm` 比. 单脚本但**默认跳过**, 标记                                                                                                                                              | `verify_fina_indicator.py`    |

## 脚本组织

```
doc/verify/
├── plan.md                         # 本文件
├── _common.py                      # 抽样日期常量 / tushare json loader / bq query 封装 / 浮点比对
├── verify_calendar.py              # P1
├── verify_stk_limit.py             # P1
├── verify_suspend_d.py             # P1
├── verify_margin_secs.py           # P1
├── verify_margin_detail.py         # P1
├── verify_stock_st.py              # P1
├── verify_adj_factor.py            # P1
├── verify_daily_basic_close.py     # P2
├── verify_daily_basic_shares.py    # P2
├── verify_stock_basic.py           # P2
├── verify_namechange.py            # P2  (verify_industry 跳过, schema 不兼容)
├── verify_forecast.py              # P3
├── verify_report.py                # P3
├── verify_dividend.py              # P3
├── verify_income.py                # P3
├── verify_cashflow.py              # P3
└── verify_fina_indicator.py        # P3 (标记口径差)
```

## `_common.py` 接口草案

```python
SAMPLE_DATES = ["20200630", "20230630", "20241230"]   # 改这里扩抽样
DATA_ROOT = pathlib.Path("data")

def load_tushare(itf, date_str):
    """读 data/YYYY/MM/DD/<itf>.json, 返回 list[dict]; 文件不存在或 in _empty 返回 []."""

def bq_query(sql, filters=None):
    """subprocess 调 `bq dai query` + --filters, 返回 list[dict] (用 --format json)."""

def cmp_float(a, b, eps=1e-6, scale=1.0):
    """abs(a*scale - b) < eps * max(1, |b|); NaN 一致 OK."""

def diff_report(tag, ts_set, bq_set, sample=10):
    """打印 ∩/∪ 计数 + 各仅一侧的前 sample 条样例."""
```

## 实施顺序

**第一轮**: P1 + P2 (12 个脚本). P3 (事件流/PIT) 框架跑通后再做.

每个脚本独立可跑:

```bash
python doc/verify/verify_<name>.py            # 默认 SAMPLE_DATES
python doc/verify/verify_<name>.py YYYYMMDD   # 单日
```

每脚本输出末尾:

```
=== verify_<name> ===
日期: [20200630, 20230630, 20241230]
检查项:
  - (ts_code, key) 集合: ts=N1, bq=N2, ∩=N3, 仅 ts=N4, 仅 bq=N5
  - 字段 X: 对齐 K1, 不一致 K2 (前 10 样例)
  - 字段 Y: ...
PASS / FAIL  (FAIL 直接 assert)
```

## 开放问题 (待确认)

1. **抽样规模**: 默认 3 日是否足够? 还是直接全期 (会触 bigquant 翻页+流量, ~10-20 分钟/itf)
2. **adj_factor 对齐方式**: 比相邻比值是否足够? 还是要校对每次除权事件的具体日期
3. **dividend `div_proc='实施'` 阶段**: bigquant `cn_stock_dividend.date` 是否就是 tushare `imp_ann_date` (实施公告日)? 待 sample 确认
4. **forecast `fore_type` TINYINT 编码表**: bigquant 该字段是数值码 (1/2/3/...), 需要先单跑一次找出编码 → tushare type 字符串映射
5. **income/cashflow 多 report_type**: tushare PK 含 report_type (1/2/3/4/...), bigquant `_pit` 表无对应概念 (仅 change_type 修订维度) → 验证时是否仅取 report_type=1 (合并)
