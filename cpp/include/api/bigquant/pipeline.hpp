#pragma once

#include "api/bigquant/spec.hpp"

#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// BigQuant DAI 数据接入主流水线
//   - Static 表 → data/_meta/<name>.json (全量整刷)
//   - Partition/Where + Day → misc::plan_fetch_segments (整月空洞→月段 / 局部
//                              缺失→日段 / lookback 强拉最近 N 个日历日)
//                              → DAI fetch → 按 visible_date 切日落盘
//   - Partition + MonthFirst → scan_missing_months → 按月段 fetch → 同上
//   - 每张表整段成功后 mark_api_updated (lastupdate 去重)
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<TableSpec> &specs, int lookback_days);

} // namespace bigquant
