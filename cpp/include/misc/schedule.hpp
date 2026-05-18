#pragma once

#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// 统一抓取调度器 (bigquant DAI Day + tushare 共用)
//
// 输入: 表名 / outer [start, end] / lookback_days / 该 API 是否支持区间查询.
// 输出: 升序的 FetchSegment 列表, 每段 = [seg.start, seg.end] (闭区间).
//
// 算法 (与 misc::store::scan_missing_days 串联):
//   1. scan_missing_days → 拿到 [start, end] 内 (3 态判定 + lookback 强拉) 的缺失日.
//   2. can_range = false  → 每个缺失日各自一段 [d, d] (per-day API 强制).
//   3. can_range = true   → 按自然月聚合:
//        - 月内 (clamp 到 outer 后) 每个日历日都在 missing → 一段 [clamp_first, clamp_last]
//        - 否则月内每个 missing 日 → 单日段 [d, d]
//      该规则在 "整月空洞" 与 "近端 lookback 补丁" 之间自动取舍, 避免为补 7 天而拉一年.
//
// 注:
//   - 只服务 Day 频率. bigquant Static 不走调度, MonthFirst 仍用 scan_missing_months.
//   - lookback 是 "日历日" (不是交易日), 直接 misc::store::scan_missing_days 行为.
//   - 段日期格式与 scan_missing_days 一致, 即 "YYYYMMDD" (8 字符).
// ============================================================================
namespace misc {

struct FetchSegment {
  std::string start; // YYYYMMDD
  std::string end;   // YYYYMMDD; per-day 段 start == end
};

std::vector<FetchSegment>
plan_fetch_segments(std::string_view name, std::string_view start,
                    std::string_view end, int lookback_days, bool can_range);

} // namespace misc
