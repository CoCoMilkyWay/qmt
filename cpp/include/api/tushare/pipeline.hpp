#pragma once

#include "api/tushare/spec.hpp"
#include <string_view>
#include <vector>

namespace tushare {

// lookback_days: 最近 N 个日历日强制重拉 (PK upsert 幂等)。
//   兜住当日数据未结算的延迟，缺省值见 config::PIPELINE_LOOKBACK_DAYS。
void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs, int lookback_days);

} // namespace tushare
