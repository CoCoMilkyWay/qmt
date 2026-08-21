#pragma once

#include "feature/feature.hpp"
#include "feature/graph.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace strategy {

// ============================================================================
// 策略层 — 每策略一个 spec 文件 (strategy/def/<name>.hpp), registry.hpp 挂载.
//
// 每策略绑定固定 4 列特征 (SF), 构成策略自己的固定小管线, 叶子全指向共享图
// 节点 (FeatureSpec 指针), 存 Tensor.strat_mats (不占 Tensor.mats); 计算见
// strategy/columns.cpp:
//   pool_b (TS, bool)  静态白名单母集 = exchange/板块/行业白名单 ∧ 已上市
//                      ∧ ¬susp ∧ ¬退市 ∧ margin 开关
//   pool   (CS, bool)  可买母集 = (pool_b ∧ ¬OR(filters)) 里
//                      rank(rank_key) ≤ universe_size; ≤0 = 不截断
//   score  (CS, float) Σ w·pct_rank_pool(factor) / Σ|w|; 每因子先在 pool 内重做
//                      截面分位再加权, pool 外恒 0 (见 columns.cpp::cs_score)
//   rank   (CS, float) score 在 pool 内的 1-based 降序排名; 0 = 不在母集.
//                      回测 top-N / exit / watch 与实盘选股读同一列 ⇒
//                      "回测 = 实盘" 收敛到单一入口, 且可 dump 对账.
// ============================================================================

enum class SF : int {
  pool_b = 0,
  pool,
  score,
  rank,
  COUNT,
};

inline constexpr int SF_COUNT = static_cast<int>(SF::COUNT);

inline constexpr std::string_view SF_NAMES[SF_COUNT] = {
    "pool_b",
    "pool",
    "score",
    "rank",
};

// 策略 s (STRATEGIES[] 下标) 的列 c → Tensor.strat_mats 扁平 slot.
inline constexpr int slot(int s, SF c) {
  return s * SF_COUNT + static_cast<int>(c);
}

struct FactorWeight {
  const feature::FeatureSpec *f; // 必须 Kind::Factor (registry consteval 校验)
  float w;                       // 必须 != 0 (符号定义方向, 正/负均可); 不想要的
                                 // factor 直接删行 (=禁用)
};

// 两融标的 (is_margin==1) 的池化策略 — per-D per-A 动态判定:
//   Exclude = 排除两融标的 (只留非两融); Include = 包含两融 (普通+两融都留);
//   Only = 只要两融标的 (只留 is_margin==1).
enum class MarginPolicy : std::uint8_t {
  Exclude,
  Include,
  Only,
};

// Pool 定义 — 可独立成常量被多个策略复用 (定义级共享).
struct PoolSpec {
  // exchange 中文全称白名单 (匹配 _meta/cn_stock_basic_info::exchange):
  //   "上海证券交易所" / "深圳证券交易所" / "北京证券交易所"
  std::span<const std::string_view> exchange_wl;
  // list_sector 中文白名单 (匹配 StockMeta.list_sector, 与 exchange 同口径):
  //   "主板" / "创业板" / "科创板" / "北交所"
  std::span<const std::string_view> list_sector_wl;
  // 申万 SW2021 一级行业中文名白名单 (columns.cpp 启动期一次性转 ID mask;
  //   industry_l1 ID 0 = 未知, 永远不命中)
  std::span<const std::string_view> industry_l1_wl;
  MarginPolicy margin_policy;           // 两融标的池化策略 (Exclude/Include/Only)
  const feature::FeatureSpec *rank_key; // 截面 universe 排名 key (小市值池 = mcap_raw_spec)
  bool rank_asc;                        // true = 升序取前 N (小市值), false = 降序 (大市值)
  // pool = (pool_b ∧ ¬filters) 里 rank(rank_key) ≤ universe_size 的部分;
  //   ≤ 0 = 不截断, 全部入池 (见 columns.cpp::cs_pool).
  int universe_size;
};

struct StrategySpec {
  std::string_view name; // 唯一; = 输出目录名 output/strategy/<name>/
  PoolSpec pool;
  // pool 排除 OR(filters); 全部 Kind::Filter (registry consteval 校验)
  std::span<const feature::FeatureSpec *const> filters;
  // score = Σ w·pct_rank_pool(factor) / Σ|w|
  std::span<const FactorWeight> weights;
  // 回测窗口 / 持仓 / 退出 — per-strategy (交易成本 / capital_base 是券商账户
  // 属性, 留全局 config.hpp)
  const char *bt_start_date; // 左端点 YYYYMMDD (含); 右端点固定 axes 最新日
  int hold_n;                // 目标持仓数
  float exit_ratio;          // 离开 top-(hold_n × exit_ratio) 的持仓必卖
};

} // namespace strategy
