#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// 统一月度调度器 — bigquant / tushare 共用唯一入口.
//
// 数据集唯一落地形态 = data/YYYY-MM/<name>.parquet (0 行月也落 0 行文件).
// 因此调度只有一条规则, 按月两轴判定:
//
//   关月 (月末 < today - lookback_days):
//     parquet 存在 → skip (已冻结; 0 行文件 = 拉过为空, 同样 skip)
//     parquet 缺失 → fetch 整月 [max(m01, start), m末]
//       (bigquant 侧 fetch 自带 API_MIN_DATE 护栏: 历史月必须已在 archive,
//        缺失即 assert — 历史不可在线补)
//
//   开放月 (月末仍在 lookback 窗口内, 含当月):
//     parquet mtime 距今 < dedup_seconds → skip (文件自身 mtime 即去重时间戳)
//     否则 → fetch [max(m01, start), min(m末, today)] 整月覆盖 (幂等)
//
// 完整性 / 去重全部由 "单文件存在性 + mtime" 判定, 无额外状态文件.
// ============================================================================
namespace misc {

struct FetchMonth {
  std::string ym;    // "YYYY-MM" (data/ 子目录名)
  std::string start; // YYYYMMDD 闭区间
  std::string end;   // YYYYMMDD 闭区间
};

std::vector<FetchMonth> plan_months(std::string_view name,
                                    std::string_view start_date,
                                    std::string_view today, int lookback_days,
                                    int dedup_seconds);

// 文件存在且 mtime 距今 < seconds — _meta 单文件表 (Static / Snapshot) 的
// dedup 判定与开放月同语义 (文件自身 mtime 即去重时间戳).
bool file_fresh(const std::filesystem::path &p, int seconds);

} // namespace misc
