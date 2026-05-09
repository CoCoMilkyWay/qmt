#include "misc/date.hpp"
#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/build.hpp"
#include "feature/tensor.hpp"
#include "tushare/pipeline.hpp"
#include "tushare/spec.hpp"

int main() {
  tushare::update(config::PIPELINE_START_DATE, misc::today_yyyymmdd(),
                  tushare::SPECS, config::LOOKBACK_DAYS);

  feature::Axes axes;
  feature::StockMeta meta;
  feature::Tensor T = feature::build(axes, meta);
  (void)T;
  return 0;
}
