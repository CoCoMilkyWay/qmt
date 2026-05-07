#include "misc/date.hpp"
#include "config.hpp"
#include "tushare/pipeline.hpp"
#include "tushare/spec.hpp"

int main() {
  tushare::update(config::PIPELINE_START_DATE, misc::today_yyyymmdd(),
                  tushare::SPECS, config::LOOKBACK_DAYS);
  return 0;
}
