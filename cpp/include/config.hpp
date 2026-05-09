#pragma once

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

inline constexpr int API_DEDUP_WINDOW_SECONDS = 600; // 单 itf 去重窗口：上次成功更新距今 < 该值则跳过；时间戳落 data/_meta/<name>.lastupdate (粒度=数据文件名 spec.name)

} // namespace config
