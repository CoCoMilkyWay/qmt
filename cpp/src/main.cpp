// Phase 1: 数据接入子系统 (bigquant DAI + tushare HTTP) → 落地 data/
// feature / backtest / analysis 暂停, 待 Phase 2 迁移到 bigquant itf 后串联.

#include "api/bigquant/pipeline.hpp"
#include "api/bigquant/spec.hpp"
#include "api/tushare/pipeline.hpp"
#include "api/tushare/spec.hpp"
#include "config.hpp"
#include "misc/date.hpp"

int main() {
  std::string today = misc::today_yyyymmdd();

  bigquant::update(::config::PIPELINE_START_DATE, today, bigquant::SPECS,
                   ::config::LOOKBACK_DAYS);

  tushare::update(::config::PIPELINE_START_DATE, today, tushare::SPECS,
                  ::config::LOOKBACK_DAYS);

  return 0;
}
