#pragma once

#include "api/bigquant/spec.hpp"

#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// BigQuant DAI 数据接入主流水线
//   - Static 表 → data/_meta/<name>.json (全量整刷)
//   - Partition/Where + Day → scan_missing_days → 按 BIGQUANT_FETCH_MAX_DAYS_PER_CALL
//                              切段 → DAI fetch → 按 visible_date 切日落盘
//   - Partition + MonthFirst → scan_missing_months → 按月段 fetch → 同上
//   - 每张表整段成功后 mark_api_updated (lastupdate 去重)
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<TableSpec> &specs, int lookback_days);

} // namespace bigquant
