#pragma once

#include <string>
#include <string_view>
#include <vector>

// ============================================================================
// 统一抓取调度器 — bigquant DAI + tushare 共用入口.
//
// 输入: 表名 / outer [start, end] / lookback_days.
// 输出: 升序的 FetchSegment 列表, 每段 = [seg.start, seg.end] (闭区间, "YYYYMMDD").
//
// 三态判定 (与 misc::store::_empty.json 串联):
//   day file 存在 → 拉过有数据
//   day file 不存在 + 在 _empty → 拉过无数据 (跳过)
//   day file 不存在 + 不在 _empty → 未拉 (必拉)
// 加上 lookback (最近 N 个日历日) 强拉, 兜住当日未结算的累积缺失.
//
// 两个对仗 planner:
//
//   plan_day_segments(name, [s, e], lookback, can_range)
//     - can_range = true:  整月空洞 → 月段 [m_first, m_last] (clamp 到 outer);
//                          否则该月每个缺失日 → 单日段 [d, d].
//     - can_range = false: 每个缺失日单独一段 [d, d] (per-day API 强制).
//     用例: bigquant Day, tushare 所有 strategy.
//
//   plan_month_segments(name, [s, e], lookback)
//     - 该月任一 day file 存在 → 整月跳过; 进入 lookback 窗口的月 → 必拉;
//       否则 → 一段 [m_first, m_last] (clamp 到 outer).
//     用例: bigquant MonthFirst (cn_stock_industry_component).
// ============================================================================
namespace misc {

struct FetchSegment {
  std::string start; // YYYYMMDD
  std::string end;   // YYYYMMDD; per-day 段 start == end
};

std::vector<FetchSegment>
plan_day_segments(std::string_view name, std::string_view start,
                  std::string_view end, int lookback_days, bool can_range);

std::vector<FetchSegment>
plan_month_segments(std::string_view name, std::string_view start,
                    std::string_view end, int lookback_days);

} // namespace misc
