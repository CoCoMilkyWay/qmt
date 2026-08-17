#pragma once

#include "api/bigquant/spec.hpp"

#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// BigQuant DAI 月度流水线 (与 tushare::update 完全对仗)
//   - Static / Snapshot → data/_meta/<name>.parquet 单文件整刷 (mtime dedup)
//   - 其余              → misc::plan_months (关月存在→skip / 开放月 mtime dedup /
//                         缺失→拉; 历史月缺失撞 API_MIN_DATE 护栏 assert) →
//                         fetch(月) → data/YYYY-MM/<name>.parquet 整月覆盖
// 落盘 = 服务端响应原样 (单文件 tmp+rename 原子; 0 行月也落 0 行文件).
// ============================================================================
void update(std::string_view start, std::string_view end,
            const std::vector<TableSpec> &specs, int lookback_days);

} // namespace bigquant
