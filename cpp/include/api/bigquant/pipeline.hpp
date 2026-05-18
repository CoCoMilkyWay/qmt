#pragma once

#include "api/bigquant/spec.hpp"

#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// BigQuant DAI 数据接入主流水线
//   - Static                 → data/_meta/<name>.json (DAI 一次响应直写)
//   - Partition/Where + Day  → misc::plan_day_segments (整月空洞→月段 / 局部缺失→日段
//                              / lookback 强拉) → DAI fetch → 按 visible_date 切日落盘
//   - Partition + MonthFirst → misc::plan_month_segments → 按月段 fetch → 同上
//   - emit_meta (axis 源)    → 上面 Partition+Day 落 day file 后, 末尾 aggregate_meta
//                              从全部 day file 聚合到 data/_meta/<name>.json
//   - 每张表整段成功后 mark_api_updated (lastupdate 去重)
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<TableSpec> &specs, int lookback_days);

} // namespace bigquant
