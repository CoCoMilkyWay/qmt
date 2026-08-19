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
// 每策略绑定固定 5 列特征 (SF), 构成策略自己的固定小管线, 叶子全指向共享图
// 节点 (FeatureSpec 指针), 存 Tensor.strat_mats (不占 Tensor.mats); 计算见
// strategy/columns.cpp:
//   pool_b   (TS, bool)  静态白名单母集 = exchange/板块/行业白名单 ∧ 已上市
//                        ∧ ¬susp ∧ ¬退市 ∧ margin 开关
//   pool     (CS, bool)  截面 universe = pool_b ∧ rank(rank_key) ≤ universe_size
//   tradable (CS, bool)  pool ∧ ¬OR(filters)  (可买母集)
//   score    (CS, float) Σ w·factor / Σ w·1{finite}  (全截面可算, 供 analysis IC)
//   rank     (CS, float) score 在 tradable 内的 1-based 降序排名; 0 = 不在母集.
//                        回测 top-N / exit / watch 与实盘选股读同一列 ⇒
//                        "回测 = 实盘" 收敛到单一入口, 且可 dump 对账.
// ============================================================================

enum class SF : int {
  pool_b = 0,
  pool,
  tradable,
  score,
  rank,
  COUNT,
};

inline constexpr int SF_COUNT = static_cast<int>(SF::COUNT);

inline constexpr std::string_view SF_NAMES[SF_COUNT] = {
    "pool_b",
    "pool",
    "tradable",
    "score",
    "rank",
};

// 策略 s (STRATEGIES[] 下标) 的列 c → Tensor.strat_mats 扁平 slot.
inline constexpr int slot(int s, SF c) {
  return s * SF_COUNT + static_cast<int>(c);
}

struct FactorWeight {
  const feature::FeatureSpec *f; // 必须 Kind::Factor (registry consteval 校验)
  float w;                       // 必须 > 0; 不想要的 factor 直接删行 (=禁用)
};

// Pool 定义 — 可独立成常量被多个策略复用 (定义级共享).
struct PoolSpec {
  // exchange 中文全称白名单 (匹配 _meta/cn_stock_basic_info::exchange):
  //   "上海证券交易所" / "深圳证券交易所" / "北京证券交易所"
  std::span<const std::string_view> exchange_wl;
  // list_sector int8 编码白名单: 1=主板 / 2=创业板 / 3=科创板 / 4=北交所
  std::span<const std::int8_t> list_sector_wl;
  // 申万 SW2021 一级行业中文名白名单 (columns.cpp 启动期一次性转 ID mask;
  //   industry_l1 ID 0 = 未知, 永远不命中)
  std::span<const std::string_view> industry_l1_wl;
  bool include_margin;                  // 是否包含两融标的 (per-D per-A 动态)
  const feature::FeatureSpec *rank_key; // 截面 universe 排名 key (小市值池 = mcap_raw_spec)
  bool rank_asc;                        // true = 升序取前 N (小市值), false = 降序 (大市值)
  int universe_size;                    // pool = pool_b ∧ rank(rank_key) ≤ universe_size
};

struct StrategySpec {
  std::string_view name; // 唯一; = 输出目录名 output/strategy/<name>/
  PoolSpec pool;
  // tradable = pool ∧ ¬OR(filters); 全部 Kind::Filter (registry consteval 校验)
  std::span<const feature::FeatureSpec *const> filters;
  // score = Σ w·factor / Σ w·1{finite}
  std::span<const FactorWeight> weights;
  // 回测窗口 / 持仓 / 退出 — per-strategy (交易成本 / capital_base 是券商账户
  // 属性, 留全局 config.hpp)
  const char *bt_start_date; // 左端点 YYYYMMDD (含); 右端点固定 axes 最新日
  int hold_n;                // 目标持仓数
  float exit_ratio;          // 离开 top-(hold_n × exit_ratio) 的持仓必卖
};

} // namespace strategy
