#pragma once

// ============================================================================
// 策略挂载表 — 唯一挂载点.
//   新增策略: 1) strategy/def/<name>.hpp 写 spec  2) 此处 include + 挂一行.
//   consteval 校验: 名字唯一非空 / filters 全 Kind::Filter / weights 全
//   Kind::Factor 且 w != 0 / 参数域合法. 违规直接编译失败.
//
//   本文件不依赖 feature/registry.hpp (避免循环: feature/registry.hpp 需要
//   STRATEGIES[] 来推导计算图 roots). 校验只需 FeatureSpec::kind, 由
//   feature/graph.hpp 提供.
// ============================================================================

#include "feature/graph.hpp"
#include "strategy/def/diagnostic.hpp"
#include "strategy/def/low_pb_small_cap.hpp"
#include "strategy/def/low_price_small_cap.hpp"
#include "strategy/def/low_valuation_small_cap.hpp"
#include "strategy/def/margin_small_cap.hpp"
#include "strategy/strategy.hpp"

#include <array>
#include <cstddef>

namespace strategy {

inline constexpr std::array<const StrategySpec *, 5> STRATEGIES = {{
    &def::low_price_small_cap,
    &def::low_valuation_small_cap,
    &def::margin_small_cap,
    &def::low_pb_small_cap,
    &def::diagnostic,
}};

inline constexpr int N_STRATEGIES = static_cast<int>(STRATEGIES.size());
inline constexpr int N_STRAT_SLOTS = N_STRATEGIES * SF_COUNT;

namespace registry_detail {

consteval bool validate() {
  for (std::size_t i = 0; i < STRATEGIES.size(); ++i) {
    const StrategySpec &s = *STRATEGIES[i];
    if (s.name.empty())
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      if (STRATEGIES[j]->name == s.name)
        return false;
    }
    if (s.pool.rank_key == nullptr)
      return false;
    if (s.pool.universe_size <= 0)
      return false;
    for (const feature::FeatureSpec *f : s.filters) {
      if (f == nullptr || f->kind != feature::Kind::Filter)
        return false;
    }
    if (s.weights.empty())
      return false;
    for (const FactorWeight &fw : s.weights) {
      if (fw.f == nullptr || fw.f->kind != feature::Kind::Factor)
        return false;
      if (fw.w == 0.0f)
        return false;
    }
    if (s.bt_start_date == nullptr ||
        std::string_view(s.bt_start_date).size() != 8)
      return false;
    if (s.hold_n <= 0)
      return false;
    if (!(s.exit_ratio >= 1.0f))
      return false;
  }
  return true;
}

} // namespace registry_detail

static_assert(registry_detail::validate(),
              "STRATEGIES[]: 名字重复/为空, filter/factor Kind 不符, "
              "universe_size/hold_n/exit_ratio/bt_start_date 非法");

} // namespace strategy
