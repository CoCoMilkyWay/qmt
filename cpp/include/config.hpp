#pragma once

#include "feature/feature.hpp"

#include <array>
#include <string_view>

namespace config {

// ============================================================================
// A. 数据源凭据 (与官方 CLI / 控制台 token 同源)
//   BigQuant DAI 凭据 (~/.bigquant/config.json::auth):
//     AK: Access Key, 12 字符, Flight Basic Token 用户名
//     SK: Secret Key, 64 字符, Flight Basic Token 密码, 永不回传
//   Tushare pro token (官网 → 个人主页 → 接口 token); *_vip 接口需 5000+ 积分
//   想隔离不进 git: 整段挪去 cpp/include/secrets.hpp (.gitignore), 本文件改 include
// ============================================================================
inline constexpr const char *BIGQUANT_AK = "6dS0GYgxocXL";
inline constexpr const char *BIGQUANT_SK = "bKDM141Hz2etbj3QTLf9GA6aGmEZRu68MJuvaJBJyPgq22E3fNNDRehFCbgComTQ";
inline constexpr const char *TUSHARE_TOKEN = "439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72";

// ============================================================================
// B. 数据源端点 (网关 host / port / 超时 / 重试)
//   BigQuant 数据面: Flight 17010     — 明文 gRPC + Arrow IPC RecordBatch, 零拷贝
//   Tushare:         HTTP   80        — 明文 JSON POST, 三张事件表
// 超时 = 单次连接+读写整体时长 (boost::system_error 触发 → retry 上层兜底)
// 重试 = RETRY_MAX 次额外重试 (实际尝试 = RETRY_MAX + 1); RETRY_INTERVAL 为线性间隔
// ============================================================================
inline constexpr const char *BIGQUANT_FLIGHT_URI = "grpc+tcp://bigquant.com:17010";
inline constexpr int BIGQUANT_FLIGHT_GRPC_MAX_METADATA_SIZE = 16 * 1024 * 1024; // 16 MiB; SDK 默认 8KB 会被 JWT 撑爆

inline constexpr const char *TUSHARE_HTTP_HOST = "api.tushare.pro";
inline constexpr const char *TUSHARE_HTTP_PORT = "80";  // 走 80, 省掉 SSL 依赖
inline constexpr int TUSHARE_HTTP_TIMEOUT_SECONDS = 60; // range 接口序列化耗时可达 20s, 留余量
inline constexpr int TUSHARE_HTTP_RETRY_MAX = 4;        // 共 5 次尝试
inline constexpr int TUSHARE_HTTP_RETRY_INTERVAL_SECONDS = 30;

// ============================================================================
// C. 抓取流水线 (main.cpp 主入口; 公共参数)
//   PIPELINE_START_DATE              首日; A 股财报电子化从 2015 起逐渐完整
//   PIPELINE_LOOKBACK_DAYS           最近 N 日历日强制重拉 (PK upsert 幂等); ≥5 交易日兜底
//   PIPELINE_DEDUP_WINDOW_SECONDS    单 itf 去重窗口: 上次成功距今 < 该值则跳过整段
//                                    (时间戳落 data/_meta/<name>.lastupdate)
// 单段切分语义 (调度入口见 misc/schedule.hpp):
//   plan_day_segments + can_range=true (bigquant Day / tushare range-capable):
//       missing 按自然月聚合, 月内 (clamp 到 outer) 全缺失 → 整月段; 否则 → 每个缺失日单段.
//       lookback 强拉最近 N 个日历日, 通常落在当前月, 形成 N 个单日段, 避免为补 7 天拉一整月.
//   plan_day_segments + can_range=false (tushare per-day API: day_params 非空):
//       每个缺失日 [d, d] 单日段, strategy 按 day_params 数量倍增 task.
//   plan_month_segments (bigquant MonthFirst, industry_component):
//       该月任一 day file 存在则跳过; 否则一段 [m_first, m_last] (clamp 到 outer).
//   bigquant Static: DAI 一次响应直写 _meta, 不走调度.
//   bigquant emit_meta (axis 源): 走 plan_day_segments 正常 day file, 末尾聚合 _meta.
// ============================================================================
inline constexpr const char *PIPELINE_START_DATE = "20150101";
inline constexpr int PIPELINE_LOOKBACK_DAYS = 7;
inline constexpr int PIPELINE_DEDUP_WINDOW_SECONDS = 60 * 60;

// ============================================================================
// C.1 BigQuant parquet 月数据库导入 (独立阶段, 与 DAI 完全解耦)
//   BIGQUANT_IMPORT          true 时启用 (与 DAI 阶段独立, 不依赖任何 DAI 配置)
//   BIGQUANT_DATABASE        parquet 月数据库根目录 (相对 git root 或绝对路径)
//                            预期布局 (与 doc/bigquant/fetch.py 输出一致):
//                              <root>/<yyyy>-<mm>/<table>.parquet
//                            静态表 (_meta/<table>.parquet) 不走 import, 留给 DAI.
//   行为: 扫 <root>/ 下所有 "YYYY-MM" 子目录 (其他名字如 _meta 静默跳过), 按 yyyymm
//     升序对每个月每张表 (SPECS 中 kind != Static) 执行 "整月替换覆盖":
//     1. 读 parquet → arrow::Table; 文件不存在 → 静默跳过
//     2. 走整月事务: stage 到 data/_journal/, atomic 写 manifest (commit point),
//        apply 把 staged day file rename 到 data/YYYY/MM/DD/<name>.json + 清掉本月
//        [01, 月末] 内不在 manifest 的残留 target + 更新 data/YYYY/MM/_empty.json,
//        cleanup
//     3. 进程启动时优先扫 data/_journal/ 重放残留 manifest (crash recovery)
//   完整性: 整月原子 (中断不留脏月) + 整月覆盖 (已有月先删后写).
// ============================================================================
inline constexpr bool BIGQUANT_IMPORT = false;
inline constexpr const char *BIGQUANT_DATABASE = "import/parquet";

// BigQuant DAI 拉取的最早允许 visible_date (dashed, "YYYY-MM-DD"; bigquant::fetch 处校验).
//   API 额度有限按日刷新, 此日期之前的历史数据必须走 BIGQUANT_IMPORT 压缩 archive 通道,
//   不再消耗在线调用配额. 任何 start < 本阈值的 DAI 查询在 fetch 直接 assert fail.
inline constexpr const char *BIGQUANT_API_MIN_DATE = "2026-05-01";

// ============================================================================
// D. Pool (basic + universe)
//   pool_b (TS, asset 静态 ∩ industry_l1 时变):
//     exchange     ∈ POOL_EXCHANGE_WHITELIST     匹配 _meta/cn_stock_basic_info.json::exchange
//                                                 (中文全称: "上海证券交易所" / "深圳证券交易所" /
//                                                  "北京证券交易所"; 与 BigQuant 字段值一致)
//     list_sector  ∈ POOL_LIST_SECTOR_WHITELIST  匹配 cn_stock_basic_info.list_sector (int8)
//                                                 编码: 1=主板 / 2=创业板 / 3=科创板 / 4=北交所
//     industry_l1  ∈ POOL_INDUSTRY_L1_WHITELIST  匹配 itf:cn_stock_industry_component 申万 SW2021
//                                                 一级行业中文名 (全量 31 个; 此处保留 28, 排除
//                                                 环保/交通运输/房地产). feature.cpp 启动期一次性
//                                                 转 ID mask, 运行期 inline 查 (industry_l1 ID 0
//                                                 = 未知, 永远不命中).
//     POOL_INCLUDE_MARGIN                         是否包含两融标的 (per-D per-A 动态)
//   pool (CS): pool_b ∧ rank(mcap_raw asc) ≤ POOL_UNIVERSE_SIZE  (per D, 截面 top-N)
//   注: low_mc / revenue_st / dividend_st 仍硬编码 list_sector==1 (主板) 判定阈值, 因为
//       这些是业务规则 (板块特定阈值与适用范围), 非策略可调过滤.
// ============================================================================
inline constexpr std::array<std::string_view, 2> POOL_EXCHANGE_WHITELIST = {{
    "上海证券交易所",
    "深圳证券交易所",
}};

// list_sector int8 编码: 1=主板, 2=创业板, 3=科创板, 4=北交所; 0=未知 (不应入).
inline constexpr std::array<int8_t, 1> POOL_LIST_SECTOR_WHITELIST = {{
    1, // 主板
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
//   STRATEGY_FACTOR_WEIGHTS   cs_factor_score = Σ w_f · factor_f / Σ w_f · 1{finite}
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
inline constexpr bool DESCRIBE_ENABLE = true;
inline constexpr bool DESCRIBE_BY_YEAR = false;

// ============================================================================
// I. Build 契约 (build 末尾必须全 finite 的 feature 白名单; fail fast)
//   任何一格 NaN 都表示 compute fn 漏写或上游污染. raw / *_age /
//   daily_return 等列预期可能 NaN, 不在此列表; factor 经截面均值补缺后必须全 finite.
// ============================================================================
inline constexpr std::array<feature::F, 26> BUILD_NO_NAN_FEATURES = {{
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
    // CS factor / score (factor_pipeline 截面均值补缺后应全 finite)
    feature::F::close,
    feature::F::mcap,
    feature::F::fmcap,
    feature::F::pe_ttm12,
    feature::F::pb_ttm3,
    feature::F::ps_ttm12,
    feature::F::pcf_ttm12,
    feature::F::roe_ttm12,
    feature::F::roa_ttm12,
    feature::F::dy_ttm12,
    feature::F::factor_score,
}};

} // namespace config
