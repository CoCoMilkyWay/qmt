// 主入口: bigquant::update → tushare::update → feature::build.
//   数据接入 (Phase 1) 把 26 张 BigQuant + 3 张 Tushare 表落 data/, 然后
//   feature::build 在 PIT-clean cutoff 下产出 Tensor T[F][A][D].
//   backtest / analysis 后续启用.

#include "api/bigquant/import.hpp"
#include "api/bigquant/pipeline.hpp"
#include "api/bigquant/spec.hpp"
#include "api/tushare/pipeline.hpp"
#include "api/tushare/spec.hpp"
#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/build.hpp"
#include "feature/tensor.hpp"
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

  // ---- Phase 2: feature build → Tensor T[F][A][D] ----
  feature::Axes axes;
  feature::StockMeta meta;
  feature::Tensor T = feature::build(axes, meta);
  (void)T; // 待 backtest / analysis 启用后消费

  return 0;
}
