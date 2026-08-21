#pragma once

#include "feature/def/all.hpp"
#include "strategy/strategy.hpp"

#include <array>

namespace strategy::def::strat_1 {

inline constexpr std::string_view name = "策略1";
inline constexpr int universe_size = 2000;
// margin_policy: Exclude=排除两融 / Include=含两融 / Only=仅两融
inline constexpr MarginPolicy margin_policy = MarginPolicy::Include;

inline constexpr const feature::FeatureSpec *filters[] = {
    &feature::def::profit_st_spec,
    &feature::def::revenue_st_spec,
    &feature::def::dividend_st_spec,
    &feature::def::trading_st_spec,
    &feature::def::risk_warn_spec,
    &feature::def::new_list_spec,
};

inline constexpr FactorWeight weights[] = {
    {&feature::def::mcap_spec, -1.0f / 6},
    {&feature::def::close_spec, -1.0f / 6},
    {&feature::def::cp_ttm12_spec, 1.0f / 6},
    {&feature::def::mr_bal_spec, -1.0f / 6},
    {&feature::def::ms_bal_spec, 2.0f / 6},
};

inline constexpr std::array<std::string_view, 2> exchange_wl = {{
    "上海证券交易所",
    "深圳证券交易所",
    //"北京证券交易所",
}};

inline constexpr std::array<std::string_view, 1> list_sector_wl = {{
    "主板",
    //"创业板",
    //"科创板",
    //"北交所",
}};

// 申万 SW2021
inline constexpr std::array<std::string_view, 26> industry_l1_wl = {{
    "交通运输",
    "传媒",
    "公用事业",
    //"农林牧渔",
    "医药生物",
    "商贸零售",
    "国防军工",
    "基础化工",
    "家用电器",
    "建筑材料",
    "建筑装饰",
    //"房地产",
    "有色金属",
    "机械设备",
    "汽车",
    "煤炭",
    //"环保",
    "电力设备",
    "电子",
    "石油石化",
    "社会服务",
    "纺织服饰",
    "综合",
    "美容护理",
    "计算机",
    "轻工制造",
    "通信",
    //"钢铁",
    //"银行",
    "非银金融",
    "食品饮料",
}};

inline constexpr StrategySpec spec{
    .name = name,
    .pool =
        {
            .exchange_wl = exchange_wl,
            .list_sector_wl = list_sector_wl,
            .industry_l1_wl = industry_l1_wl,
            .margin_policy = margin_policy,
            .rank_key = &feature::def::mcap_raw_spec,
            .rank_asc = true,
            .universe_size = universe_size,
        },
    .filters = filters,
    .weights = weights,
    .bt_start_date = "20170101",
    .hold_n = 10,
    .exit_ratio = 2.0f,
};

} // namespace strategy::def::strat_1
