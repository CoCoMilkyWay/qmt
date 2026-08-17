#pragma once

#include "api/tushare/spec.hpp"
#include <string_view>
#include <vector>

namespace tushare {

// Tushare 月度流水线 (与 bigquant::update 完全对仗):
//   misc::plan_months (关月存在→skip / 开放月 mtime dedup / 缺失→拉) →
//   fetch_month → data/YYYY-MM/<name>.parquet 整月覆盖 (tmp+rename 原子).
// lookback_days: 月末仍在该窗口内的月视为开放月, 整月重拉 (幂等覆盖).
void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs, int lookback_days);

} // namespace tushare
