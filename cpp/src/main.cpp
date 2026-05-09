#include "misc/date.hpp"
#include "config.hpp"
#include "factor/axis.hpp"
#include "factor/build.hpp"
#include "factor/tensor.hpp"
#include "tushare/pipeline.hpp"
#include "tushare/spec.hpp"

int main() {
  tushare::update(config::PIPELINE_START_DATE, misc::today_yyyymmdd(),
                  tushare::SPECS, config::LOOKBACK_DAYS);

  factor::Axes axes;
  factor::StockMeta meta;
  factor::Tensor T = factor::build(axes, meta);
  (void)T;
  return 0;
}
