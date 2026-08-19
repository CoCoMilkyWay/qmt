#pragma once

#include "feature/def/factor/cffoa_ttm12.hpp"
#include "feature/def/factor/close.hpp"
#include "feature/def/factor/fmcap.hpp"
#include "feature/def/factor/mcap.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/filter/dividend_st.hpp"
#include "feature/def/filter/new_list.hpp"
#include "feature/def/filter/profit_st.hpp"
#include "feature/def/filter/revenue_st.hpp"
#include "feature/def/filter/risk_warn.hpp"
#include "feature/def/filter/trading_st.hpp"
#include "strategy/strategy.hpp"

#include <array>

// 策略: small_cap — 主板小市值 + 低价/现金流因子 (完整继承拆分前 config.hpp
//   D/E/F 段现值, 行为不变基线).

namespace strategy::def {

inline constexpr std::array<std::string_view, 2> small_cap_exchange_wl = {{
    "上海证券交易所",
    "深圳证券交易所",
}};

inline constexpr std::array<std::int8_t, 1> small_cap_list_sector_wl = {{
    1, // 主板
}};

// 申万 SW2021 一级全量 31 个; 此处保留 25, 排除 环保/交通运输/房地产/农林牧渔/钢铁/银行.
inline constexpr std::array<std::string_view, 25> small_cap_industry_l1_wl = {{
    "基础化工",
    "有色金属",
    "建筑材料",
    "建筑装饰",
    "机械设备",
    "电子",
    "汽车",
    "家用电器",
    "食品饮料",
    "纺织服饰",
    "轻工制造",
    "医药生物",
    "公用事业",
    "商贸零售",
    "社会服务",
    "非银金融",
    "综合",
    "电力设备",
    "国防军工",
    "计算机",
    "传媒",
    "通信",
    "煤炭",
    "石油石化",
    "美容护理",
    //"农林牧渔",
    //"钢铁",
    //"银行",
}};

inline constexpr const feature::FeatureSpec *small_cap_filters[] = {
    &feature::def::profit_st_spec,
    &feature::def::revenue_st_spec,
    &feature::def::dividend_st_spec,
    &feature::def::trading_st_spec,
    &feature::def::risk_warn_spec,
    &feature::def::new_list_spec,
};

inline constexpr FactorWeight small_cap_weights[] = {
    {&feature::def::mcap_spec, 0.7f},
    {&feature::def::fmcap_spec, 0.7f},
    {&feature::def::close_spec, 0.1f},
    {&feature::def::cffoa_ttm12_spec, 0.1f},
};

inline constexpr StrategySpec small_cap{
    .name = "small_cap",
    .pool =
        {
            .exchange_wl = small_cap_exchange_wl,
            .list_sector_wl = small_cap_list_sector_wl,
            .industry_l1_wl = small_cap_industry_l1_wl,
            .include_margin = true, // 池内含两融 (= py margin_tradings 全选)
            .rank_key = &feature::def::mcap_raw_spec,
            .rank_asc = true, // 小市值: 升序取前 N
            .universe_size = 400,
        },
    .filters = small_cap_filters,
    .weights = small_cap_weights,
    .bt_start_date = "20170101",
    .hold_n = 10,
    .exit_ratio = 2.0f,
};

} // namespace strategy::def
