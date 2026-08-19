#pragma once

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
inline constexpr const char *TUSHARE_TOKEN = "6b5ea435a4626b1eeedefb2115bcf9e84fc64a0d212d21cf2be03d54";

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
//   数据集唯一落地形态 = data/YYYY-MM/<name>.parquet 月度分片 (+ _meta 单文件).
//   调度单点 misc::plan_months (bigquant / tushare 共用), 规则:
//     关月 (月末 < today - LOOKBACK): parquet 存在 → skip; 缺失 → 整月 fetch
//     开放月 (含当月): 水位增量 — 只拉 vd ∈ [文件内 max(vd)+1, horizon] 新行
//       append (horizon 由每表 avail_hour 定, 见 misc/schedule.hpp; DAI 配额
//       按返回 cell 数计, 已到水位 0 行响应免费; 月内漏的回填关月重拉兜回)
//
//   PIPELINE_UPDATE                  true  = main 先跑 preflight (两路 API 状态彩色
//                                            展示 + 交互确认), 确认后才联网同步;
//                                    false = 完全不联网, 直接用 data/ 现有 parquet
//                                            跑 build / backtest / analysis
//   PIPELINE_START_DATE              首日; A 股财报电子化从 2015 起逐渐完整
//   PIPELINE_LOOKBACK_DAYS           月末仍在该窗口内的月视为开放月 (兜服务端回填修订)
//   PIPELINE_DEDUP_WINDOW_SECONDS    去重窗口: 开放月/单文件 parquet 自身 mtime 距今
//                                    < 该值则跳过 (无额外 lastupdate 状态文件)
// ============================================================================
inline constexpr bool PIPELINE_UPDATE = true;
inline constexpr const char *PIPELINE_START_DATE = "20150101";
inline constexpr int PIPELINE_LOOKBACK_DAYS = 7;
inline constexpr int PIPELINE_DEDUP_WINDOW_SECONDS = 60 * 60;

// ============================================================================
// D/E. Pool / 策略参数已迁入策略层 (每策略一个 spec 文件):
//   cpp/include/strategy/strategy.hpp   PoolSpec / FactorWeight / StrategySpec / SF
//   cpp/include/strategy/def/<name>.hpp 每策略白名单 / filter / 因子权重 /
//                                       回测窗口 (bt_start_date / hold_n / exit_ratio)
//   cpp/include/strategy/registry.hpp   STRATEGIES[] 挂载表 + consteval 校验
//   注: low_mc / revenue_st / dividend_st 仍硬编码 list_sector==1 (主板) 判定阈值,
//       因为这些是业务规则 (板块特定阈值与适用范围), 非策略可调过滤.
//
// F. 回测 (成本 + 资金; 券商账户属性, 全策略共享)
//   BACKTEST_CAPITAL_BASE          初始资金 [元]
//   BACKTEST_PRICE_LIMIT_EPS       涨跌停判定容差
//   BACKTEST_BUY_COST              单边买入成本 (万 3)
//   BACKTEST_SELL_COST             单边卖出成本 (万 13)
//   BACKTEST_MIN_COST              单笔最低成本 [元]
// ============================================================================
inline constexpr float BACKTEST_CAPITAL_BASE = 1.0e6f;
inline constexpr float BACKTEST_PRICE_LIMIT_EPS = 1e-4f;
inline constexpr float BACKTEST_BUY_COST = 3e-4f;
inline constexpr float BACKTEST_SELL_COST = 13e-4f;
inline constexpr float BACKTEST_MIN_COST = 5.0f;

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
// H2. Tensor dump (逐 (a, d) 张量导出, 供 test/ 的 Python 参考实现逐点对账)
//   TENSOR_DUMP_ENABLE  总开关; 开启后写 output/tensor/<feature>.npy
//                       全量 47 feature × n_a × n_d × 4B ≈ 3 GB, 对账完毕应关回
// ============================================================================
inline constexpr bool TENSOR_DUMP_ENABLE = true;

// ============================================================================
// I. Build 契约 (build 末尾必须全 finite 校验) — 已下沉到每个节点自己声明的
//   FeatureSpec::must_be_finite (feature/graph.hpp), 无中心清单.
//   build.cpp 遍历 feature::ALL_NODES, must_be_finite=true 的节点逐一校验.
// ============================================================================

// ============================================================================
// J. 特征依赖表打印 (feature/report.cpp; 公共 → 各策略专属, 沿拓扑序;
//   formula/assumption 取自各节点 FeatureSpec 定义, 无额外文案需要维护)
// ============================================================================
inline constexpr bool FEATURE_TABLE_ENABLE = true;

} // namespace config
