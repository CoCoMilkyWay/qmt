#pragma once

#include "package/yyjson/yyjson.h"

#include <cstdint>
#include <mutex>
#include <string>
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
// parse fn 直接写 pool.<itf>.<field>; 增减 itf 只需改 PitPool 与 itf 模块的
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
//     forecast              ← Tushare forecast                      CUTOFF=-1 (normal)
//
//   财务事件 (用户决策: 财务部分暂保留旧 Tushare itf 不动, 实际数据未在新基建落地,
//   parse 不会被触发; EventStore 永远空, 财务 raw feature 全 NaN — 后续单独迁移):
//     report / income / cashflow / fina_indicator (旧 Tushare 字段)
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
//   当前张量层只用 total_shares + total_float_shares; a_float / free_float 备用.
struct GridShares {
  std::vector<float> total_shares;
  std::vector<float> total_float_shares;
};

// cn_stock_limit_price (CUTOFF=-1, normal): 当日适用涨跌停价 [元/股].
//   实际 BigQuant 入库 17:00 (盘后) → 承认滞后, row D=T 取 T-1 day file 的 limit.
//   ST 翻转日略不准 (T-1 是停牌日, T-1 limit 是旧 pct), 接受不 overlay.
struct GridLimitPrice {
  std::vector<float> upper_limit;
  std::vector<float> lower_limit;
};

// cn_stock_status (CUTOFF=0, hybrid 伪装): 两个字段:
//   st_status         int8: 0=正常, 1=ST, 2=*ST  → risk_warn 直读
//   suspended         uint8: 0/1                  → susp 直读 (1=当日停牌)
//   (is_risk_warning / price_limit_status / exdr 暂未入张量)
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
//   dividend_st 用 cash_after_tax × share_raw[ev.v] 推 3y 累计现金分红.
struct DividendEv {
  int v;
  std::string report_date;
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

// ----- 以下 4 个事件 itf 是旧 Tushare 财务 (用户决策: 财务先不管, 暂保留) -----
// 实际新基建未落地这几张表, parse 不会被触发, EventStore 保持空状态.
// 留作占位; 财务部分迁移后会被新 BigQuant cn_stock_financial_* 替代.

struct ReportEv {
  int v;
  std::string end_date;
};

struct IncomeEv {
  int v;
  std::string end_date;
  std::string report_type;
  float revenue;
  float n_income_attr_p;
};

struct CashflowEv {
  int v;
  std::string end_date;
  std::string report_type;
  float n_cashflow_act;
};

struct FinaIndEv {
  int v;
  std::string end_date;
  float roe;
  float roa;
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

  // 事件 (Tushare 保留)
  EventStore<ForecastEv> forecast;

  // 事件 (财务旧 Tushare 占位, 暂保留待后续迁移)
  EventStore<ReportEv> report;
  EventStore<IncomeEv> income;
  EventStore<CashflowEv> cashflow;
  EventStore<FinaIndEv> fina_indicator;
};

// ============================================================================
// ItfDesc: 单 itf 的描述. 在 pit.cpp 内每个 itf 一组 fn (prealloc + parse +
//   post_sort + post_ffill) 集中定义, 然后填进 ITFS[] 表. load.cpp 仅迭代该表,
//   不出现具体 itf 名.
//   增减 itf 只需 (1) PitPool 加字段 (2) pit.cpp 加一组 fn (3) ITFS[] 加一行.
//
// 函数职责:
//   prealloc(axes, pool):
//     初始化 pool 中此 itf 的字段. 网格 itf 把每个字段 vector 设为
//     length=n_a*n_d 的 NaN/0; 事件 itf 把 EventStore[a] 设为 length=n_a 的空链.
//   parse(arr, v_idx, axes, pool, mu):
//     解析单 (day, itf) json 数组到 pool. 网格场合 mu==nullptr, 直接 dense slot
//     写入无锁; 事件场合 mu 是 length=n_a 的 mutex 数组, 按 a 取锁 emplace.
//   post_sort(pool):
//     事件 itf 末段 sort by v 升序 (Phase 2 走单调指针扫). 网格 itf 留 nullptr.
//   post_ffill(axes, pool):
//     网格 itf per-A forward fill (停牌期间继承前值). 事件 itf 留 nullptr.
// ============================================================================
struct ItfDesc {
  const char *file_name; // .json basename, 也是日志/标识用名
  bool is_event;         // false=网格 (无锁), true=事件 (per-A mutex)

  void (*prealloc)(const Axes &, PitPool &);
  void (*parse)(yyjson_val *arr, int v_idx, const Axes &, PitPool &,
                std::vector<std::mutex> *mu /* 网格场合可为 nullptr */);
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
