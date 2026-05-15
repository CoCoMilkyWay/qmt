#pragma once

#include "feature/feature.hpp"

#include <array>
#include <string_view>

namespace config {

// ============================================================================
// A. 数据源凭据 (与官方 CLI / 控制台 token 同源)
//   BigQuant DAI 凭据 (~/.bigquant/config.json::auth):
//     AK: Access Key, 12 字符, 明文回传 (X-BigQuant-Access-Key)
//     SK: Secret Key, 64 字符, 仅本地 HMAC + Flight Basic Token, 永不回传
//   Tushare pro token (官网 → 个人主页 → 接口 token); *_vip 接口需 5000+ 积分
//   想隔离不进 git: 整段挪去 cpp/include/secrets.hpp (.gitignore), 本文件改 include
// ============================================================================
inline constexpr const char *BIGQUANT_AK = "6dS0GYgxocXL";
inline constexpr const char *BIGQUANT_SK = "bKDM141Hz2etbj3QTLf9GA6aGmEZRu68MJuvaJBJyPgq22E3fNNDRehFCbgComTQ";
inline constexpr const char *TUSHARE_TOKEN = "439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72";

// ============================================================================
// B. 数据源端点 (网关 host / port / 超时 / 重试)
//   BigQuant 控制面: HTTPS  443       — HMAC-SHA256 签名, whoami / get_datasource_schema
//   BigQuant 数据面: Flight 17010     — 明文 gRPC + Arrow IPC RecordBatch, 零拷贝
//   Tushare:         HTTP   80        — 明文 JSON POST, 三张事件表
// 超时 = 单次连接+读写整体时长 (boost::system_error 触发 → retry 上层兜底)
// 重试 = RETRY_MAX 次额外重试 (实际尝试 = RETRY_MAX + 1); RETRY_INTERVAL 为线性间隔
// ============================================================================
inline constexpr const char *BIGQUANT_HTTPS_HOST = "bigquant.com";
inline constexpr const char *BIGQUANT_HTTPS_PORT = "443";
inline constexpr int BIGQUANT_HTTPS_TIMEOUT_SECONDS = 30;
inline constexpr int BIGQUANT_HTTPS_RETRY_MAX = 4; // 共 5 次尝试
inline constexpr int BIGQUANT_HTTPS_RETRY_INTERVAL_SECONDS = 5;

inline constexpr const char *BIGQUANT_FLIGHT_URI = "grpc+tcp://bigquant.com:17010";
inline constexpr int BIGQUANT_FLIGHT_GRPC_MAX_METADATA_SIZE = 16 * 1024 * 1024; // 16 MiB; SDK 默认 8KB 会被 JWT 撑爆

inline constexpr const char *TUSHARE_HTTP_HOST = "api.tushare.pro";
inline constexpr const char *TUSHARE_HTTP_PORT = "80";  // 走 80, 省掉 SSL 依赖
inline constexpr int TUSHARE_HTTP_TIMEOUT_SECONDS = 60; // range 接口序列化耗时可达 20s, 留余量
inline constexpr int TUSHARE_HTTP_RETRY_MAX = 4;        // 共 5 次尝试
inline constexpr int TUSHARE_HTTP_RETRY_INTERVAL_SECONDS = 30;

// ============================================================================
// C. 抓取流水线 (main.cpp 主入口; 公共参数 + per-source 单段拉取上限)
//   PIPELINE_START_DATE              首日; A 股财报电子化从 2015 起逐渐完整
//   PIPELINE_LOOKBACK_DAYS           最近 N 日历日强制重拉 (PK upsert 幂等); ≥5 交易日兜底
//   PIPELINE_DEDUP_WINDOW_SECONDS    单 itf 去重窗口: 上次成功距今 < 该值则跳过整段
//                                    (时间戳落 data/_meta/<name>.lastupdate)
//   <SRC>_FETCH_MAX_DAYS_PER_CALL    单次 fetch 跨度上限:
//     BigQuant: DAI 无明显行限, 单年事件/财务 ~ 几万-几十万行, 1 年安全
//     Tushare:  单次 8000 行硬限, 按月拉全市场仅几千行, 1 月安全
// ============================================================================
inline constexpr const char *PIPELINE_START_DATE = "20150101";
inline constexpr int PIPELINE_LOOKBACK_DAYS = 7;
inline constexpr int PIPELINE_DEDUP_WINDOW_SECONDS = 60 * 60;

inline constexpr int BIGQUANT_FETCH_MAX_DAYS_PER_CALL = 365;
inline constexpr int TUSHARE_FETCH_MAX_DAYS_PER_CALL = 31;

// ============================================================================
// D. Pool (basic + universe)
//   pool_b (TS, asset 静态过滤):
//     exchange    ∈ POOL_EXCHANGE_WHITELIST    匹配 _meta/stock_basic_info.json::exchange
//                                              候选: SSE / SZSE / BSE
//     market      ∈ POOL_MARKET_WHITELIST      匹配 _meta/stock_basic_info.json::market 中文枚举
//                                              候选: 主板 / 创业板 / 科创板 / CDR / ...
//     industry_l1 ∈ POOL_INDUSTRY_L1_WHITELIST 匹配 itf:industry_component 申万 SW2021 一级行业中文名
//                                              (全量 31 个 L1, 此处保留 28 — 排除 环保/交通运输/房地产)
//     POOL_INCLUDE_MARGIN                      是否在池子里包括两融标的 (per-D per-A 动态)
//   pool (CS): pool_b ∧ rank(mcap_raw asc) ≤ POOL_UNIVERSE_SIZE  (per D, 截面 top-N)
//   注: feature `mb` 仍单独硬编码 "主板", 因为 low_mc / revenue_st / dividend_st 依赖
//       板块特定阈值与适用范围 (业务规则, 非策略可调过滤).
// ============================================================================
inline constexpr std::array<std::string_view, 2> POOL_EXCHANGE_WHITELIST = {{
    "SSE",
    "SZSE",
}};

inline constexpr std::array<std::string_view, 1> POOL_MARKET_WHITELIST = {{
    "主板",
}};

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

inline constexpr bool POOL_INCLUDE_MARGIN = true; // true = 池内含两融 (与 py margin_tradings=["两融标的","非两融标的"] 等价)
inline constexpr int POOL_UNIVERSE_SIZE = 100;

// ============================================================================
// E. 策略 (cs_tradable filter 子集 + cs_factor_score 加权)
//   STRATEGY_ENABLED_FILTERS  cs_tradable = pool ∧ ¬OR(此处 filter); 删一行即禁用一项
//   STRATEGY_FACTOR_WEIGHTS   cs_factor_score = Σ w_f · factor_f / Σ w_f · 1{finite}, 限于 pool
//     w 必须 > 0; 不想要的 factor 直接删行 (=禁用); F 必须是 enum 中的 factor (Kind::Factor)
// ============================================================================
inline constexpr std::array<feature::F, 6> STRATEGY_ENABLED_FILTERS = {{
    feature::F::profit_st,
    feature::F::revenue_st,
    feature::F::dividend_st,
    feature::F::trading_st,
    feature::F::risk_warn,
    feature::F::new_list,
}};

struct FactorWeight {
  feature::F f;
  float w;
};
inline constexpr std::array<FactorWeight, 2> STRATEGY_FACTOR_WEIGHTS = {{
    {feature::F::mcap, 0.6f},
    {feature::F::close, 0.4f},
}};

// ============================================================================
// F. 回测 (窗口 + 持仓 + 成本 + 再平衡; 右端点固定为 axes 最新日)
//   BACKTEST_START_DATE            左端点 (YYYYMMDD, 含); 必为交易日或落 axes 内
//   BACKTEST_HOLD_N                目标持仓数
//   BACKTEST_EXIT_RATIO            离开 top-(HOLD_N × EXIT_RATIO) 的持仓必卖
//   BACKTEST_CAPITAL_BASE          初始资金 [元]
//   BACKTEST_PRICE_LIMIT_EPS       涨跌停判定容差
//   BACKTEST_BUY_COST              单边买入成本 (万 3)
//   BACKTEST_SELL_COST             单边卖出成本 (万 13)
//   BACKTEST_MIN_COST              单笔最低成本 [元]
//   BACKTEST_REBALANCE_THRESHOLD   持仓与目标差额 ≥ THD × pv_after 才补仓
// ============================================================================
inline constexpr const char *BACKTEST_START_DATE = "20170101";
inline constexpr int BACKTEST_HOLD_N = 10;
inline constexpr float BACKTEST_EXIT_RATIO = 1.0f;
inline constexpr float BACKTEST_CAPITAL_BASE = 1.0e6f;
inline constexpr float BACKTEST_PRICE_LIMIT_EPS = 1e-4f;
inline constexpr float BACKTEST_BUY_COST = 3e-4f;
inline constexpr float BACKTEST_SELL_COST = 13e-4f;
inline constexpr float BACKTEST_MIN_COST = 5.0f;
inline constexpr float BACKTEST_REBALANCE_THRESHOLD = 0.02f;

// ============================================================================
// G. 分析 (因子分层 + IC 移动平均)
//   ANALYSIS_N_QUANTILES   分层桶数 (TAG 4 排名分析)
//   ANALYSIS_IC_MA_WINDOW  因子 IC 移动平均窗口 (日)
// ============================================================================
inline constexpr int ANALYSIS_N_QUANTILES = 10;
inline constexpr int ANALYSIS_IC_MA_WINDOW = 250;

// ============================================================================
// H. Describe (phase 4 stats; 关闭则只输出 "all" 一行/feature)
//   DESCRIBE_ENABLE   总开关
//   DESCRIBE_BY_YEAR  关闭则只 "all"; 开启则额外按自然年 (axes.dates 前 4 位) 展开
// ============================================================================
inline constexpr bool DESCRIBE_ENABLE = false;
inline constexpr bool DESCRIBE_BY_YEAR = false;

// ============================================================================
// I. Build 契约 (build 末尾必须全 finite 的 feature 白名单; fail fast)
//   任何一格 NaN 都表示 compute fn 漏写或上游污染. raw / factor / *_age /
//   daily_return 等列预期可能 NaN, 不在此列表.
// ============================================================================
inline constexpr std::array<feature::F, 15> BUILD_NO_NAN_FEATURES = {{
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
