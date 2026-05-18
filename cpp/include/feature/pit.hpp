#pragma once

#include "package/yyjson/yyjson.h"

#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace feature {

struct Axes; // fwd decl

// ============================================================================
// PIT 中间结构. Phase 1 写入, Phase 2 只读.
//
// 划分:
//   网格 itf (1 record / 交易日 / asset): dense 存. 字段独立向量, length =
//                                          n_a()*n_d() (a-major, d-minor).
//                                          缺席用 NaN (float) / 0 (uint8/int8);
//                                          与 Tensor::ts_row 同 layout, Phase 2
//                                          可零 copy 取 span.
//   事件 itf (per A 时间线):              EventStore<Ev> = vector<vector<Ev>>,
//                                          [a] 外, 按 v 升序的事件链.
//                                          v = visible_d_idx 经 CUTOFF 调整后
//                                          的 row D 索引.
//
// PitPool 是 typed struct (而非泛型 map<name, anything>) — 编译期类型安全,
// itf.replay 直接写 pool.<itf>.<field>; 增减 itf 只需改 PitPool 与 itf 模块的
// dense block (pit.cpp 内).
//
// 数据源 (BigQuant + Tushare 新基建):
//   全部 BigQuant 表实际入库时间都是盘后 17:00 之后, 按 PIT 严格 = -1; 项目按业务可推出性
//   分两类模式 (详见 README §cutoff 表):
//     normal (CUTOFF=-1, 承认滞后): row D=T 取 T-1 day file 数据.
//     hybrid (CUTOFF=0,  伪装盘前): 历史 day file 按 row=v_idx 消化 (假装盘前可见);
//                                   最后一天 (= 实盘当日) day file 还没入库时, 由
//                                   apply_meta_overlays 用 cn_stock_static_data
//                                   (真盘前 09:00) 填充 row=last_d.
//
//   网格:
//     bar1d                 ← cn_stock_real_bar1d    CUTOFF=-1 (normal)
//     shares                ← cn_stock_shares        CUTOFF=-1 (normal)
//     limit_price           ← cn_stock_limit_price   CUTOFF=-1 (normal, 不 overlay)
//     status                ← cn_stock_status        CUTOFF=0  (hybrid, overlay)
//     margin_detail         ← cn_stock_margin_trading_detail  CUTOFF=0
//                              (真盘前 10:00 入库, normal offset=0 不滞后)
//                              (含派生 is_margin = 当日 (D,A) 是否在两融名单)
//   meta overlay (apply_meta_overlays, post_sort 之后 / post_ffill 之前):
//     cn_stock_static_data (Snapshot, 真盘前 09:00, _meta 单文件) → 填充 row=last_d 的
//       status.{suspended, st_status} 两字段. 仅触及 row=last_d 一行,
//       历史天 (T < last_d) 完全不动. (limit_price 已退回 normal -1, 不再 overlay.)
//   事件:
//     industry_component    ← cn_stock_industry_component (sw2021)  CUTOFF=-1 (normal, 月初快照)
//     industry_change       ← cn_stock_industry_change (sw2021 L1)  CUTOFF=-1 (normal)
//     dividend              ← cn_stock_dividend                     CUTOFF=-1 (normal)
//     financial_ttm         ← cn_stock_financial_ttm_shift (shift=0)CUTOFF=-1 (normal)
//     financial_balance     ← cn_stock_financial_balance_general_pitCUTOFF=-1 (normal)
//     financial_income_annual ← cn_stock_financial_income_general_pit (fs_quarter_index=4)
//                                                                   CUTOFF=-1 (normal)
//     forecast              ← Tushare forecast                      CUTOFF=-1 (normal)
// ============================================================================

// ========== 网格 ==========

// cn_stock_real_bar1d (CUTOFF=-1): 不复权 OHLCV + 后复权乘子.
//   close            不复权 [元/股] (实际市场成交价, 除权日自然跳跃)
//   adjust_factor    BigQuant 后复权累积乘子 (close_hfq[d] = close[d] × adjust_factor[d]):
//                      - 平日 af 不变, close 变化 = close×af 变化 = 真实日收益
//                      - 除权日 close 跳 (含分红/送股), af 反向跳, close×af 平滑
//                      - 起点附近 af ≈ 累积初值 (e.g. 平安银行 2024-06 ≈ 116)
//   PitPool 暴露 close + adjust_factor; tensor 顶层只暴露 close_raw (= close 真价).
//   estimation 类 (mcap_raw / limit_up / low_p / cs_close / ...) 全部用 close_raw 真值 —
//   close × shares = 真市值; close vs limit_price 同口径才能判封板; < 1 元低价股看真实股价.
//   连续性类 (daily_return; 未来 momentum / vol / N 日收益) 内部叠 adjust_factor 算
//   hfq 链式 (= 含分红再投入的真持有收益, 除权日平滑无负跳; 见 feature.cpp::ts_daily_return).
//   adjust_factor 是 PitPool 内部细节, 不入 tensor 顶层契约 (按"额外复权/偏移
//   由特征自己内部处理, 不暴露顶层"原则).
struct GridBar1d {
  std::vector<float> close;
  std::vector<float> adjust_factor;
};

// cn_stock_shares (CUTOFF=-1): 各类股本 [股].
//   张量层用 total_shares (mcap_raw) + a_float_shares (fmcap_raw, BigQuant 实测口径
//     `float_market_cap` = close × a_float_shares; total_float 含 H 股, 002594/BYD 误差排查得).
//   total_float / free_float 暂不入张量, 后续如需可加.
struct GridShares {
  std::vector<float> total_shares;
  std::vector<float> a_float_shares;
};

// cn_stock_limit_price (CUTOFF=-1, normal): 当日适用涨跌停价 [元/股].
//   实际 BigQuant 入库 17:00 (盘后) → 承认滞后, row D=T 取 T-1 day file 的 limit.
//   ST 翻转日略不准 (T-1 是停牌日, T-1 limit 是旧 pct), 接受不 overlay.
struct GridLimitPrice {
  std::vector<float> upper_limit;
  std::vector<float> lower_limit;
};

// cn_stock_status (CUTOFF=0, hybrid 伪装): 两个字段:
//   st_status         int8 4 态 (派生): 0=正常 / 1=ST / 2=*ST / 3=退市整理期
//                     replay 时由原 BigQuant 字段派生:
//                       cn_stock_status (日频): st_status==1 → 1; ==2 → 2;
//                         (st==0 ∧ is_risk_warning==1) → 3; else → 0
//                       cn_stock_static_data (overlay): in_delist==1 → 3 (优先);
//                         否则 st_status 原值直落 (0/1/2)
//                     退市整理期: 交易所摘掉 *ST 标签 → 原 st_status 翻 0, 但
//                     is_risk_warning 仍 1 / static_data.in_delist=1; 用 4 态
//                     表达保留退市整理识别力, 同时区分 ST vs *ST. 下游 cs_tradable
//                     走 `> 0.5` OR 排除, 任一非零档都触发 filter.
//   suspended         uint8: 0/1                  → susp 直读 (1=当日停牌)
//   (price_limit_status / exdr 暂未入张量)
//   实际 BigQuant 入库 17:00 (盘后), 但 ST / 停牌当日开盘前即生效 → 业务上等同
//   "盘前可知". 历史 day file 按 CUTOFF=0 假装盘前; 最后一天 (= 实盘当日, day file
//   还未入库) 由 apply_meta_overlays 用 cn_stock_static_data 真盘前 09:00 填充
//   row=last_d.
struct GridStatus {
  std::vector<int8_t> st_status;
  std::vector<uint8_t> suspended;
};

// cn_stock_margin_trading_detail (CUTOFF=0): 当日两融明细.
//   is_margin                  uint8: 派生 — (D, A) 当日是否存在记录 (1=两融标的)
//   financing_balance          融资余额 [元]
//   securities_lending_balance 融券余额 [元]
//   注: rzrqye = financing_balance + securities_lending_balance (不入张量, 下游需要时自加)
struct GridMarginDetail {
  std::vector<uint8_t> is_margin;
  std::vector<float> financing_balance;
  std::vector<float> securities_lending_balance;
};

// ========== 事件 ==========

// cn_stock_industry_component WHERE industry='sw2021' (CUTOFF=-1, normal, MonthFirst):
//   每月初一份 sw2021 一级行业归属快照. 同 (D, instrument) 单条.
//   l1_id = sw2021_l1_name_to_id(industry_level1_name); 不在表内 → 0.
//   月初首日 industry_l1 自然延续上月 base (ts_industry_l1 last_id 单调推进, 见 feature.cpp).
struct IndustryComponentEv {
  int v;
  uint8_t l1_id; // sw2021 一级行业 ID (0=未知, 1..31)
};

// cn_stock_industry_change WHERE industry='sw2021' AND industry_level=1 AND
//   change_flag=1 (CUTOFF=-1, Day): 月内 sw2021 一级行业切换事件 (进入新行业).
//   同 (D, instrument) 通常只有 1 条 (一进对应一出, 只取进).
struct IndustryChangeEv {
  int v;
  uint8_t l1_id; // 切换后 sw2021 一级行业 ID
};

// cn_stock_dividend (CUTOFF=-1, Where on publish_date): 分红事件.
//   同 (instrument, publish_date, report_date) 单条.
//   dividend_st 用 cash_after_tax × share_raw[ev.v] 推 3y 累计现金分红 (按 report_date.Y 窗口).
//   dy_raw 用 cash_after_tax × share_raw[D] 推 trailing 12M 累计现金分红 (按 ex_date 日历窗口).
//   ex_date ≥ publish_date 一定 (公告早于除权), 故 ex_date ≤ T ⇒ publish_date ≤ T (PIT 自洽).
struct DividendEv {
  int v;
  std::string report_date;
  std::string ex_date;
  float cash_after_tax;
};

// Tushare forecast (CUTOFF=-1): 业绩预告.
//   profit_st / revenue_st 状态机触发源.
struct ForecastEv {
  int v;
  std::string end_date;
  std::string type;
  float last_parent_net;
};

// ----- BigQuant 财务事件 (PIT, 盘后 17:00–20:00 入库, CUTOFF=-1 normal) -----

// cn_stock_financial_ttm_shift: 财务 TTM 时序 (shift∈{0..76}).
//   每次披露一次发完整 shift 历史 (~77 期); 我们只取 shift=0 = 该 visible_date 的最新报告期 TTM.
//   shift=0 含意随披露时间漂移: 4 月底 ≈ 年报, 5 月初 ≈ Q1, 8 月底 ≈ 半年报, 11 月初 ≈ Q3.
//   字段 (BigQuant 实测口径, 见 doc/research/verify_valuation.py 验证):
//     total_operating_revenue_ttm                   ps_raw / rev_raw 分母 (含利息/保费,
//                                                                          ≠ operating_revenue_ttm)
//     net_profit_to_parent_shareholders_ttm         pe_raw / roe_raw / roa_raw 分子 (归母)
//     net_cffoa_ttm                                 pcf_raw 分母 (经营性现金流)
//   per-A 沿 v 单调推进取 latest event 即可 (shift=0 已锁"该 visible 最新报告期";
//   同 report_date 在新 visible_date 的 shift=0 行覆盖旧值, max v 自然取新).
struct FinancialTtmEv {
  int v;
  std::string report_date;
  float total_operating_revenue_ttm;
  float net_profit_to_parent_shareholders_ttm;
  float net_cffoa_ttm;
};

// cn_stock_financial_balance_general_pit: 资产负债表 PIT (MRQ snapshot).
//   同 visible_date 可见多个 report_date (历史报告期 + 修正), 取 max(report_date).
//   字段 (BigQuant 实测口径):
//     total_owner_equity                            pb_raw 分母 (含少数股东, ≠ parent equity;
//                                                                CATL/茅台 误差排查得)
//     total_equity_to_parent_shareholders           roe_raw 分母 (归母, 教科书 ROE 口径)
//     total_assets                                  roa_raw 分母
//   per-A 走 latest event 维护 map<report_date, latest_row by v>, 取 max report_date 即可.
struct FinancialBalanceEv {
  int v;
  std::string report_date;
  float total_owner_equity;
  float total_equity_to_parent_shareholders;
  float total_assets;
};

// cn_stock_financial_income_general_pit: 利润表 PIT.
//   只入 fs_quarter_index == 4 的年报 (aggregate 时过滤); 给 ni_raw 用 (dividend_st 阈值).
//   字段:
//     net_profit_to_parent_shareholders             ni_raw 数值 (归母年度 NI; 全栈对仗)
//   同 report_date 多版本取 latest visible (修正语义).
struct FinancialIncomeAnnualEv {
  int v;
  std::string report_date;
  float net_profit_to_parent_shareholders;
};

template <class Ev>
using EventStore = std::vector<std::vector<Ev>>;

struct PitPool {
  // 网格 (新基建)
  GridBar1d bar1d;
  GridShares shares;
  GridLimitPrice limit_price;
  GridStatus status;
  GridMarginDetail margin_detail;

  // 事件 (新基建)
  EventStore<IndustryComponentEv> industry_component;
  EventStore<IndustryChangeEv> industry_change;
  EventStore<DividendEv> dividend;
  EventStore<FinancialTtmEv> financial_ttm;
  EventStore<FinancialBalanceEv> financial_balance;
  EventStore<FinancialIncomeAnnualEv> financial_income_annual;

  // 事件 (Tushare 保留)
  EventStore<ForecastEv> forecast;
};

struct AggregateRow {
  std::string day;
  std::string code;
  float f0 = 0.0f;
  float f1 = 0.0f;
  float f2 = 0.0f;
  int i0 = 0;
  int i1 = 0;
  int i2 = 0;
  std::string s0;
  std::string s1;
  std::string s2;
};

// ============================================================================
// ItfDesc: 单 itf 的描述. 在 pit.cpp 内每个 itf 一组 fn (prealloc + aggregate +
//   replay + post_sort + post_ffill) 集中定义, 然后填进 ITFS[] 表.
//   load.cpp 仅迭代该表, 不出现具体 itf 名.
//   增减 itf 只需 (1) PitPool 加字段 (2) pit.cpp 加一组 fn (3) ITFS[] 加一行.
//
// 函数职责:
//   prealloc(axes, pool):
//     初始化 pool 中此 itf 的字段. 网格 itf 把每个字段 vector 设为
//     length=n_a*n_d 的 NaN/0; 事件 itf 把 EventStore[a] 设为 length=n_a 的空链.
//   aggregate(arr, day, out):
//     解析单 (day, itf) json 数组为 raw aggregate rows; 不依赖 Axes/PitPool.
//   replay(rows, axes, pool, mu):
//     把 raw aggregate rows 映射进 PitPool. 网格场合 mu==nullptr, 事件场合
//     mu 是 length=n_a 的 mutex 数组, 按 a 取锁 emplace.
//   post_sort(pool):
//     事件 itf 末段 sort by v 升序 (Phase 2 走单调指针扫). 网格 itf 留 nullptr.
//   post_ffill(axes, pool):
//     网格 itf per-A forward fill (停牌期间继承前值). 事件 itf 留 nullptr.
// ============================================================================
struct ItfDesc {
  const char *file_name; // .json basename, 也是日志/标识用名
  bool is_event;         // false=网格 (无锁), true=事件 (per-A mutex)

  void (*prealloc)(const Axes &, PitPool &);
  void (*aggregate)(yyjson_val *arr, std::string_view day,
                    std::vector<AggregateRow> &out);
  void (*replay)(std::span<const AggregateRow> rows, const Axes &, PitPool &,
                 std::vector<std::mutex> *mu);
  void (*post_sort)(PitPool &);                // 事件 itf 末段 sort by v; 网格 itf 留 nullptr
  void (*post_ffill)(const Axes &, PitPool &); // 网格 itf per-A forward fill; 事件 itf 留 nullptr
};

extern const ItfDesc ITFS[];
extern const int ITFS_COUNT;

// ============================================================================
// apply_meta_overlays — Phase 1 末段 hook (post_sort 之后, post_ffill 之前)
//
// 当前唯一 overlay 源: cn_stock_static_data (真盘前 09:00 全市场快照, _meta 单文件).
//   读 data/_meta/cn_stock_static_data.json (Snapshot kind, MAX(date) 一日全量行),
//   把 2 字段填充到 row = axes.n_d() - 1 (= 最后一天 / 实盘当日):
//     pool.status.suspended,  pool.status.st_status
//
// 设计动机 (hybrid 伪装, 兼顾"回测简洁 + 实盘正确 + 二者一致"):
//   - status 历史 (T < last_d): 沿用 cn_stock_status day file (实际盘后 17:00 入库,
//     张量层 CUTOFF=0 假装盘前可见).
//   - status 最后一天 (T = last_d, 实盘当日 day file 还没入库): static_data 是真盘前
//     数据, overlay 填入 row=last_d 即"交易时已可见且最新".
//   - 仅写 row=last_d 一行, 历史天完全不动 ⇒ 填充而非覆盖语义.
//   - 顺序要紧: 必须在 post_ffill 之前 — status 自身不做 ffill, 但其他 itf 可能做;
//     overlay 写真值后 ffill 看到非默认值即不动它.
//
// 注: cn_stock_limit_price 已退回 CUTOFF=-1 normal (承认滞后), 不再 overlay.
//
// _meta/cn_stock_static_data.json 不存在 → silently noop (历史回测 / 首次 build 容错).
// ============================================================================
void apply_meta_overlays(const Axes &axes, PitPool &pool);

} // namespace feature
