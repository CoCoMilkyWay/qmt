#pragma once

#include "feature/def/factor/close.hpp"
#include "feature/def/factor/cp_ttm12.hpp"
#include "feature/def/factor/mcap.hpp"
#include "feature/def/factor/mcap_raw.hpp"
#include "feature/def/factor/mr_bal.hpp"
#include "feature/def/factor/ms_bal.hpp"
#include "feature/def/filter/dividend_st.hpp"
#include "feature/def/filter/new_list.hpp"
#include "feature/def/filter/profit_st.hpp"
#include "feature/def/filter/revenue_st.hpp"
#include "feature/def/filter/risk_warn.hpp"
#include "feature/def/filter/trading_st.hpp"
#include "strategy/strategy.hpp"

#include <array>

// 策略: low_price_small_cap — 主板小市值 + 低价/现金流因子 (完整继承拆分前
//   config.hpp D/E/F 段现值, 行为不变基线).

namespace strategy::def {

inline constexpr std::array<std::string_view, 2> low_price_small_cap_exchange_wl = {{
    "上海证券交易所",
    "深圳证券交易所",
    //"北京证券交易所",
}};

inline constexpr std::array<std::int8_t, 1> low_price_small_cap_list_sector_wl = {{
    1, // 主板
       // 2, // 创业板
       // 3, // 科创板
       // 4, // 北交所
}};

// 申万 SW2021
inline constexpr std::array<std::string_view, 26> low_price_small_cap_industry_l1_wl = {{
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

inline constexpr const feature::FeatureSpec *low_price_small_cap_filters[] = {
    &feature::def::profit_st_spec,
    &feature::def::revenue_st_spec,
    &feature::def::dividend_st_spec,
    &feature::def::trading_st_spec,
    &feature::def::risk_warn_spec,
    &feature::def::new_list_spec,
};

inline constexpr FactorWeight low_price_small_cap_weights[] = {
    {&feature::def::mcap_spec, -1.0f / 6},
    {&feature::def::close_spec, -1.0f / 6},
    {&feature::def::cp_ttm12_spec, 1.0f / 6},
    {&feature::def::mr_bal_spec, -1.0f / 6},
    {&feature::def::ms_bal_spec, 2.0f / 6},
};

inline constexpr StrategySpec low_price_small_cap{
    .name = "低价小市值",
    .pool =
        {
            .exchange_wl = low_price_small_cap_exchange_wl,
            .list_sector_wl = low_price_small_cap_list_sector_wl,
            .industry_l1_wl = low_price_small_cap_industry_l1_wl,
            // margin_policy: Exclude=排除两融 / Include=含两融 / Only=仅两融
            .margin_policy = MarginPolicy::Include,
            //.margin_policy = MarginPolicy::Exclude,
            //.margin_policy = MarginPolicy::Only,
            // rank_key: 截面 universe 排名 key
            .rank_key = &feature::def::mcap_raw_spec,
            // rank_asc: true=升序取前 N (小市值) / false=降序 (大市值)
            .rank_asc = true,
            // universe_size: pool = pool_b ∧ rank(rank_key) ≤ universe_size
            .universe_size = 2000,
        },
    .filters = low_price_small_cap_filters,
    .weights = low_price_small_cap_weights,
    .bt_start_date = "20170101",
    .hold_n = 10,
    .exit_ratio = 2.0f,
};

} // namespace strategy::def
