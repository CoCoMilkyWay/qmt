# Tensor table processing anomalies
_Exported on 5/18/2026 at 21:25:08 GMT+8 from Cursor (3.2.11)_

---

**User**

@README.md 

这里张量表的处理结果有异常嘛？

@/home/chuyin/.cursor/projects/home-chuyin-work-qmt/terminals/36.txt:7-632 

---

**Cursor**

我来仔细分析终端 36 的统计输出，找异常。先看几个可疑点的代码实现。

我把 `report` 输出（terminal 36）逐 feature 扫了一遍并对照代码核了一下。整体张量算流程跑通，但有几处统计上**值得关注**的点（按严重度排）：

## 1. `risk_warn` max = 3，与 README 不符（**文档/代码不一致**）

```247:247:/home/chuyin/work/qmt/README.md
| filter | risk_warn    | 时序 | itf:cn_stock_status                                                          | `cn_stock_status.st_status` (int8 → float; 0=正常, 1=ST, 2=*ST)
```

但 `pit.cpp` 实际派生 4 态：

```363:367:/home/chuyin/work/qmt/cpp/src/feature/pit.cpp
    int8_t out_st = (st == 1)            ? int8_t{1}
                    : (st == 2)          ? int8_t{2}
                    : (rw != 0)          ? int8_t{3}
                                         : int8_t{0};
```

`risk_warn 2026 max=3` 是这第 4 态（退市整理期）。代码语义是新加的（含 `apply_meta_overlays` 用 `in_delist=1 → 3`），但 README §字段表 risk_warn 行没同步。需要把 README 更新成「0/1/2/3」+ 加入退市整理期说明，否则容易踩坑。

## 2. `up_lim` 大量 `1e6` 哨兵 — 2015–2017 影响面 ≥5%

```296:302:/home/chuyin/work/qmt/cpp/src/feature/pit.cpp
    {
      float up = as_float_or_nan(yyjson_obj_get(item, "upper_limit"));
      pool.limit_price.upper_limit[off] = (up == 0.0f) ? 1e6f : positive_or_inf(up);
    }
    {
      float dn = as_float_or_nan(yyjson_obj_get(item, "lower_limit"));
      pool.limit_price.lower_limit[off] = (dn == 0.0f) ? 0.01f : positive_or_inf(dn);
    }
```

`up_lim` 各年统计：
- 2015 `95% = 1e6`、`mean = 1.452e+05`
- 2016 `95% = 1e6`、`mean = 9.154e+04`
- 2017 `95% = 1e6`、`mean = 7.295e+04`
- 2018+ `max = 1e6` 但 `95% < 300`

说明 **2015–2017 至少 5% 股票的 `upper_limit` 在 BigQuant 落库就是 0**，被哨兵替换；2018+ 是个位数比例（仍然存在）。对应 `dn_lim` 同期 `5% = 0.01`（也是哨兵）。

这不影响 `limit_up`/`limit_dn` 判定（`close_raw < 1e6` 一定不触发涨停，`close_raw > 0.01` 一定不触发跌停），但：
- 业务上意味着 2015–2017 "封板信号" 的覆盖面严重不足
- `up_lim` 进了 `describe` 的统计分布是混着哨兵的，看着很奇怪 — 想清洁分布建议在 describe 处屏蔽 `==1e6f` / `==0.01f`，或哨兵改用 `+inf` (现在 ffill 不兜 +inf，会让停牌中天保哨兵；改前先确认 ffill 链)

## 3. `dy_raw` min 为微负 — **浮点累加漂移**

各年 min 看到 `-1.146e-07`、5%/25% 也出现 `-9.436e-11` 这种量级。代码：

```376:402:/home/chuyin/work/qmt/cpp/src/feature/feature.cpp
    if (!is_finite(e.cash_after_tax) || e.cash_after_tax <= 0.0f)
      continue;
    items.push_back({misc::parse_yyyymmdd(e.ex_date), e.cash_after_tax});
  }
  ...
  while (hi < items.size() && items[hi].ex <= Td) {
    cash_sum += items[hi].cash;
    ++hi;
  }
  while (lo < hi && items[lo].ex <= Tlo) {
    cash_sum -= items[lo].cash;
    ++lo;
  }
```

滚动窗口 `cash_sum += / -=` 浮点累计 drift，当窗口全部弹出时 `cash_sum` 不精确归 0。这种 `~1e-7` 量级的负数下游 `winsor_mad → z → pct_rank` 不受影响，但语义上 dy 不应为负。需要修可在 `out[d] = max(0.0f, (cash_sum * ts) / m)` 兜一下。

## 4. 其余看到的几点（**预期**，不是 bug）

- `pe_raw / ps_raw / pcf_raw / roe_raw / roa_raw` raw 极端值（`max ~ e+04 … e+07`、`min ~ -e+05`）：分母接近 0 的极端，README 已声明 raw 接受这种值，靠 cs `winsor_mad` 截断。`pe_ttm12 / pb_ttm3 …` 经过 `factor_pipeline` 后 5/95 分位形态稳定，没问题。
- `pct_rank` 输出 max ≈ 0.93–0.97 而非 1：`winsor_mad` 把上端 clip 到 `med+3·MAD`，造成 ~5% 高端并列，`pct_rank` 取平均秩后 max < 1。完全是 winsor 的预期行为。
- `profit_st 2026 = 11.66%`、`risk_warn 2026 = 3.23%`：4 月底是 ST 戴帽截止日，状态机的 `off_d = (Y+1, 4, monthend)`，统计跑到 5 月所以还在 on 区间，预期。
- `revenue_st` 2015–2020 全 0：README 限定 `end_date.Y ≥ 2021 ∧ ann_date ≥ 20210101`，预期。
- `dividend_st` 2015–2017 全 0：warmup `axes 起点 + 3y`，预期。
- `is_margin` 12.9% → 64% 的逐年上升：两融标的扩容真实历史。
- `industry_l1 mean` 逐年上升：早期未匹配（0）占比高，月初快照逐渐覆盖，预期。

---

**结论**：核心张量计算流没看出算错；唯一**真异常**是 №1（README 与代码不一致, 需要把 README §字段表 risk_warn 行同步成 4 态）。№2/№3 是数据源/数值稳定性问题，不影响下游因子排序，但如果想让 describe 输出干净需要小修。

要不要我把 №1（README 同步成 4 态）+ №3（`dy_raw` clamp ≥ 0）这两处一起改掉？№2 的 `up_lim` 哨兵动一动就涉及 `ffill` / `limit_up` 判定，建议先确认你想要的哨兵语义再动。

