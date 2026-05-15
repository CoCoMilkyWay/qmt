// Phase 1: 数据接入子系统 (bigquant DAI + tushare HTTP) → 落地 data/
// feature / backtest / analysis 暂停, 待 Phase 2 迁移到 bigquant itf 后串联.

#include "api/bigquant/import.hpp"
#include "api/bigquant/pipeline.hpp"
#include "api/bigquant/spec.hpp"
#include "api/tushare/pipeline.hpp"
#include "api/tushare/spec.hpp"
#include "config.hpp"
#include "misc/date.hpp"

int main() {
  std::string today = misc::today_yyyymmdd();

  // 附属: parquet 月数据库导入 (与 DAI 完全独立, 扫 BIGQUANT_DATABASE 整月覆盖)
  if (::config::BIGQUANT_IMPORT) {
    bigquant::import_parquet(::config::BIGQUANT_DATABASE, bigquant::SPECS);
  }

  bigquant::update(::config::PIPELINE_START_DATE, today, bigquant::SPECS,
                   ::config::PIPELINE_LOOKBACK_DAYS);

  tushare::update(::config::PIPELINE_START_DATE, today, tushare::SPECS,
                  ::config::PIPELINE_LOOKBACK_DAYS);

  return 0;
}
