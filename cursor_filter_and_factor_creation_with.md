# Filter and factor creation with current data
_Exported on 5/7/2026 at 17:51:00 GMT+8 from Cursor (3.2.11)_

---

**User**

@README.md 
@app/strategy/strategy.py 

这里的filter和factor因子， 可以用现在的 /data 做出来吗

---

**Cursor**

我先核对一下关键的数据存在情况（disclosure / st 数据是否真的有，stock_basic 是否含 market 字段），然后再给结论。

## 结论：部分可以，部分缺口

`/data` 下的 7+ 个 tushare 接口（forecast / express / disclosure / st / dividend / daily_basic / adj_factor / stk_limit / suspend_d / fina_indicator + meta/stock_basic）能覆盖**大部分**，但有明确缺口。

### 因子 (factor)

| 因子 | bigquant 原数据源 | 用 `/data` |
|---|---|---|
| `pe_ttm` | `pe_ttm` | ✅ `daily_basic.pe_ttm` |
| `pb` | `pb` | ✅ `daily_basic.pb` |
| `ps_ttm` | `ps_ttm` | ✅ `daily_basic.ps_ttm` |
| `dividend_yield` | `dividend_yield_ratio` | ✅ `daily_basic.dv_ttm`（口径需对一下） |
| `total_market_cap` | `total_market_cap` | ✅ `daily_basic.total_mv`（单位万元，需 ×1e4） |
| `float_market_cap` | `float_market_cap` | ✅ `daily_basic.circ_mv` |
| `close` | `close`（前复权） | ✅ `daily_basic.close` + `adj_factor` 自算前复权 |
| `roe_ttm` | `roe_avg_ttm` | ⚠️ `fina_indicator` 只给季度 `roe/q_roe`，要自己滚 4Q TTM 平均 |
| `roa_ttm` | `roa_avg_ttm` | ⚠️ 同上 |
| `pcf_ttm` | `pcf_net_ttm` | ❌ 需 `cashflow_vip`，**SPECS 没有** |

注：`FACTOR_WEIGHTS` 当前默认只有 `total_market_cap=0.6` + `close=0.4` 非零 → **`/data` 已完全够用**。

### 过滤 (filter)

| 过滤因子 | 用 `/data` |
|---|---|
| `new_listing` | ✅ `_meta/stock_basic.list_date` |
| `trading_st` | ✅ `daily_basic.close` + `total_mv` + `stock_basic.market` 板块判定 |
| `profit_st` | ✅ `forecast.json` + `disclosure.json`（2015 起齐全） |
| `risk_warning` | ⚠️ `/data` 的 `st`（pub_date+st_tpye 事件流）≠ bigquant `is_risk_warning`（状态量）；要写状态机重建 |
| `revenue_st` | ❌ 缺 **TTM 营收**（要 `income_vip`，**SPECS 没有**） |
| `dividend_st` | ❌ 缺 **归母净利润绝对值** `n_income_attr_p`（`fina_indicator` 只给比率不给绝对值，要 `income_vip`，**SPECS 没有**） |

### 缺口/需确认

要 100% 对齐 `strategy.py`，**SPECS 还需要补**：
1. `income`/`income_vip` → 修 `revenue_st` + `dividend_st`
2. `cashflow`/`cashflow_vip` → 修 `pcf_ttm`（如要启用权重）

另外 4 个口径问题需要人确认：
- `daily_basic.total_mv` 万元 vs bigquant 元（×1e4）
- `daily_basic.close` 不复权 vs `cn_stock_prefactors.close` 前复权（必须配 `adj_factor`）
- `daily_basic.dv_ttm` vs `dividend_yield_ratio` 定义（横截面 sanity check）
- `filter.SW2021_ALL_INDUSTRIES` 申万 2021 → tushare 不带这个分类，只有 `stock_basic.industry`（tushare 自有口径），`get_basic_pool_sql` 里这段要重写或舍弃

---

接下来你想怎么走？
1. 先把"完全可做"的部分（`new_listing` / `trading_st` / `profit_st` + 7 个 daily_basic 因子）改成读 `/data` 的离线版？
2. 还是先把 `income_vip` / `cashflow_vip` 加进 SPECS 补齐数据？
3. 还是先确认上面 4 个口径问题再动？上一条回复已经给出完整结论和 4 个待确认口径，停在向你提的三选一问题上。请直接告诉我选 1/2/3 哪条路径（或其他想法），我再继续推进。

---

**User**

我们首先定义一个大的张量： time(day) * assets * features(factors, intermediates)
在 @README.md  下面定义一个大表，那些factor， 哪些intermediate， 还有计算顺序， 依赖， 类型， 描述如何计算

先把完整的大表定义好，我们再讨论其他的

---

**Cursor**

明白。我先把完整张量字段表写出来，按"raw → derived → window/ttm/event → cs-transform → agg"的依赖顺序，把所有 strategy.py 用到的 feature 都纳入，标注每条的状态（已就绪 / 口径待确认 / 缺数据源）。我直接追加到 `README.md` 末尾。

大表已写入 `README.md` 末尾，分四部分：

1. **字段表**（按计算顺序）— 60+ feature，覆盖 L0 raw → L1 derived → L2 stateful → L3 cs-z → L4 agg；每行 6 列：层级 / 名字 / 类型 / 依赖 / 公式或源 / 状态（✓ / △ / ✗）
2. **数据缺口** — 需要新增的 tushare 接口
3. **口径警告** — 7 条用前必须确认的口径问题
4. **层级 + 状态** 说明

请你看一遍，重点确认：

- L2 那几行 `roe_ttm` / `roa_ttm` 用 `roe_yearly` ffill 还是 `q_roe` 滚 4Q 求和——口径选哪个？
- L1 复权：所有 close-类字段是不是都用 `close * adj_factor / max(adj_factor by A)` 这种"最新值归一"的前复权？还是用别的归一基准？
- 行业过滤（申万 2021 → tushare industry）你打算去掉还是做映射？
- PIT 那条：实盘要不要严格 D-1 日 daily_basic 才能进入 D 日决策？

确认后我再讨论"先实现哪部分 / 怎么落 SQL 或 numpy 算"。

---

**User**

1. 后面要做张量加速和并行， 所以所有特征统一是8bit浮点数
2. daily_basic和stk_limit里的价格其实是复权过的
3. feature名字要最大化的简明和对仗
4. L0-L4的区分太刻意， 不要， 直接简单分类， filter/factor/inter
5. 排列顺序自上而下， 先排列最终的filter和factor，然后再确定依赖，就近原则，但是顺序也要保证causual


---

**Cursor**

明白。我重新组织：去掉 L0-L4 分层、统一 dtype 为 f8、移除 adj_factor 相关、命名对仗化、按 filter→factor→inter 顺序排列。

已重写。主要变化：

- **去掉 L0-L4**，列改为 `kind ∈ {filter, factor, inter, score}`
- **dtype 统一 f8**，移除 dtype 列（避免冗余），口径警告新增第 7 条说明 normalize 方案待定
- **价格已复权**，删掉 `adj_factor`、`close_raw / up_limit_raw / down_limit_raw → close / up_limit / down_limit` 的派生层；`close_raw / up_lim / dn_lim` 直接来自 itf
- **命名对仗化**：
  - filter: `profit_st / revenue_st / dividend_st / trading_st / risk_warn / new_list`
  - factor: `close / mcap / fmcap / pe / pb / ps / pcf / roe / roa / dy`
  - inter raw（与 factor 同根的）一律 `*_raw`：`close_raw / mcap_raw / fmcap_raw / pe_raw / pb_raw / ps_raw / pcf_raw / dy_raw / roe_raw / roa_raw / rev_raw / ni_raw`
  - inter 其他: `up_lim / dn_lim / susp / mb / list_age / low_p / low_mc / limit_up / limit_dn / pool_b / pool / block / trade`
- **顺序**：filter → factor → inter → score；inter 内部 raw → derived → pool/agg，causal 自下而上保证；filter 内部 `revenue_st` 显式依赖 `profit_st` 故排其后

请检查命名和分组。如果 OK 我们再讨论：
1. f8 量化方案（pe/mcap/close 这种动态范围大的字段如何 normalize）
2. PIT 是否要改成 D-1 daily_basic
3. 哪部分先动手实现

上一条已经完成（README 大表重写完毕、变更点列出、最后给出三个待选下一步）。等你确认命名/分组没问题，再告诉我下一步选哪条。

