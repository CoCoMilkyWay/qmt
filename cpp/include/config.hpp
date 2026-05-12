#pragma once

#include "feature/feature.hpp"

#include <array>
#include <string_view>

namespace config {

inline constexpr const char *TUSHARE_TOKEN =
    "439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72"; // tushare pro token，*_vip 接口需要 5000+ 积分

inline constexpr const char *API_HOST = "api.tushare.pro"; // 官方 REST 网关，走明文 HTTP（无 HTTPS），省掉 SSL 依赖
inline constexpr const char *API_PORT = "80";              // 官方 REST 网关端口
inline constexpr int HTTP_TIMEOUT_SECONDS = 60;            // range 接口服务端序列化耗时可达 20s，60s 留余量；超时直接 assert，update 幂等重跑即可
inline constexpr int HTTP_RETRY_MAX = 5 - 1;               // 网络瞬抖（DNS 解析失败、连接复位、读写超时）重试次数；耗尽再 assert
inline constexpr int HTTP_RETRY_INTERVAL_SECONDS = 30;     // 重试间隔；HTTP_RETRY_MAX * 该值 ≈ 容忍的网络中断时长

inline constexpr const char *PIPELINE_START_DATE = "20150101"; // A 股财报电子化记录从 2015 起逐渐完整，再早数据稀疏
inline constexpr int LOOKBACK_DAYS = 7;                        // 最近 N 日历日强制重拉：≥5 交易日，兜住 tushare 端补登延迟
inline constexpr int FETCH_MAX_DAYS_PER_CALL = 31;             // 单次 API 调用跨度上限；按月拉全市场仅几千行，安全在 8000 行限额内

inline constexpr int API_DEDUP_WINDOW_SECONDS = 60 * 60; // 单 itf 去重窗口：上次成功更新距今 < 该值则跳过；时间戳落 data/_meta/<name>.lastupdate (粒度=数据文件名 spec.name)

// feature 子系统
inline constexpr int UNIVERSE_SIZE = 100; // pool 截面: pool_b ∧ rank(mcap_raw asc) ≤ N 取最终 strategy universe

// pool_b 交易所白名单 (asset 静态过滤, 与 _meta/stock_basic.json::exchange 匹配).
//   候选: SSE 上交所 / SZSE 深交所 / BSE 北交所.
inline constexpr std::array<std::string_view, 2> POOL_EXCHANGE_WHITELIST = {{
    "SSE",
    "SZSE",
}};

// pool_b 板块白名单 (asset 静态过滤, 与 _meta/stock_basic.json::market 中文枚举匹配).
//   候选枚举: 主板 / 创业板 / 科创板 / CDR / ...
//   注: feature `mb` 仍单独硬编码 "主板", 因为 `low_mc` / `revenue_st` / `dividend_st`
//       依赖板块特定阈值与适用范围 (业务规则, 非策略可调过滤).
inline constexpr std::array<std::string_view, 1> POOL_MARKET_WHITELIST = {{
    "主板",
}};

// pool_b 行业白名单 (asset 静态过滤, 与 _meta/index_member_all.json::l1_name 申万 SW2021 一级行业中文名匹配).
//   全量 31 个 L1, 此处保留 28 个 (排除「环保 / 交通运输 / 房地产」 — 弹性差, 长期沉底).
inline constexpr std::array<std::string_view, 28> POOL_INDUSTRY_L1_WHITELIST = {{
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
    "农林牧渔",
    "钢铁",
    "银行",
}};

// pool_b 是否在池子里包括两融标的 (per-D per-A 动态过滤, 来自 itf:margin_secs).
//   true  = 包括两融标的 (默认, 不过滤; 与 py 默认 margin_tradings=["两融标的","非两融标的"] 等价)
//   false = 不包括两融标的 (仅非两融标的进 pool_b; 与 py 风格 margin_tradings=["非两融标的"] 等价)
inline constexpr bool POOL_INCLUDE_MARGIN = true;

// describe (phase 4): 关闭则只输出 "all" 一行/feature; 开启则额外按自然年(axes.dates 前 4 位)展开
inline constexpr bool ENABLE_DESCRIBE = false;
inline constexpr bool DESCRIBE_BY_YEAR = false;

// ============================================================================
// strategy / 回测 / 分析 配置
// ============================================================================

// 回测窗口起点 (YYYYMMDD; 含端点); 必为交易日或落到 axes 内的日期.
//   右端点固定为 axes 最新日 (即 build 时拉到的最后一个交易日), 不再可配.
inline constexpr const char *BACKTEST_START_DATE = "20170101";

// tradable 启用哪些 filter (cs_tradable 在 pool ∧ ¬OR(此处 filter)); 删一行即禁用一项
inline constexpr std::array<feature::F, 6> ENABLED_FILTERS = {{
    feature::F::profit_st,
    feature::F::revenue_st,
    feature::F::dividend_st,
    feature::F::trading_st,
    feature::F::risk_warn,
    feature::F::new_list,
}};

// factor 加权合成 score (cs_factor_score = Σ w_f · factor_f / Σ w_f · 1{finite}, 限于 pool).
//   w 必须 > 0; 不想要的 factor 直接删行 (=禁用); F 必须是 enum 中的 factor (Kind::Factor)
struct FactorWeight {
  feature::F f;
  float w;
};
inline constexpr std::array<FactorWeight, 2> FACTOR_WEIGHTS = {{
    {feature::F::mcap, 0.6f},
    {feature::F::close, 0.4f},
}};

// backtest 行为
inline constexpr int BT_HOLD_N = 10;             // 目标持仓数
inline constexpr float BT_EXIT_RATIO = 1.0f;     // 离开 top-(HOLD_N*EXIT_RATIO) 的持仓必卖
inline constexpr float BT_CAPITAL_BASE = 1.0e6f; // 初始资金 [元]
inline constexpr float BT_PRICE_LIMIT_EPS = 1e-4f;
inline constexpr float BT_BUY_COST = 3e-4f;      // 单边万 3
inline constexpr float BT_SELL_COST = 13e-4f;    // 单边万 13
inline constexpr float BT_MIN_COST = 5.0f;       // 单笔最低 5 元
inline constexpr float BT_REBALANCE_THD = 0.02f; // 再平衡阈值: 持仓与目标差额 ≥ BT_REBALANCE_THD × pv_after 才补仓 (合理降低交易次数)

// analysis: 分层桶数 (TAG 4 排名分析); IC 移动平均窗口
inline constexpr int N_QUANTILES = 10;
inline constexpr int IC_MA_WINDOW = 250; // 因子 IC 250 日均值

// 契约: 这些 feature 在 build 末尾必须全 finite (0/1 bool).
//   任何一格 NaN 都表示 compute fn 漏写或上游污染, fail fast.
//   raw / factor / *_age / daily_return 等列预期可能 NaN, 不在此列表.
inline constexpr std::array<feature::F, 15> NO_NAN_FEATURES = {{
    // TS bool (ts_* 内 std::fill(0.0f) 后选中置 1.0f, 或 grid_copy_bool)
    feature::F::susp,
    feature::F::is_margin,
    feature::F::low_p,
    feature::F::low_mc,
    feature::F::limit_up,
    feature::F::limit_dn,
    feature::F::profit_st,
    feature::F::revenue_st,
    feature::F::dividend_st,
    feature::F::risk_warn,
    feature::F::trading_st,
    feature::F::new_list,
    feature::F::pool_b,
    // CS bool (cs_pool / cs_tradable scatter 全行)
    feature::F::pool,
    feature::F::tradable,
}};

} // namespace config
