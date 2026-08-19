#pragma once

#include "feature/def/factor/dy_ttm12.hpp"
#include "feature/def/factor/ep_ttm12.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/mr_bal.hpp"
#include "feature/def/factor/ms_bal.hpp"
#include "feature/def/factor/roa_ttm12.hpp"
#include "feature/def/factor/roe_ttm12.hpp"
#include "strategy/def/low_price_small_cap.hpp" // 复用其 exchange/sector/industry 白名单 + filters
#include "strategy/strategy.hpp"

// 策略: diagnostic — 对账诊断策略. 不为交易, 只为把重构后掉出计算图的中性因子
//   (ep_ttm12/roe_ttm12/roa_ttm12/dy_ttm12) 及孤儿 raw (mr_bal_raw/ms_bal_raw) 拉回
//   计算图使其被计算/落张量, 供 test/compare.py 对账. pool/filters/bt 沿用
//   low_price_small_cap (行为不变基线), 仅 weights 换成诊断因子集.

namespace strategy::def {

inline constexpr FactorWeight diagnostic_weights[] = {
    {&feature::def::ep_ttm12_spec, 1.0f},
    {&feature::def::roe_ttm12_spec, 1.0f},
    {&feature::def::roa_ttm12_spec, 1.0f},
    {&feature::def::dy_ttm12_spec, 1.0f},
    {&feature::def::mr_bal_spec, 1.0f},
    {&feature::def::ms_bal_spec, 1.0f},
};

inline constexpr StrategySpec diagnostic{
    .name = "对账诊断",
    .pool =
        {
            .exchange_wl = low_price_small_cap_exchange_wl,
            .list_sector_wl = low_price_small_cap_list_sector_wl,
            .industry_l1_wl = low_price_small_cap_industry_l1_wl,
            .margin_policy = MarginPolicy::Include,
            .rank_key = &feature::def::mcap_raw_spec,
            .rank_asc = true,
            .universe_size = 400,
        },
    .filters = low_price_small_cap_filters,
    .weights = diagnostic_weights,
    .bt_start_date = "20170101",
    .hold_n = 10,
    .exit_ratio = 2.0f,
};

} // namespace strategy::def
