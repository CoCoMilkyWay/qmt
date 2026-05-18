// 主入口: bigquant::update → tushare::update → feature::build → backtest::run →
//   analysis::run → write_meta. 完整闭环: 数据接入 → PIT 张量 → 回测 → 因子分析 →
//   summary JSON, 落 <git_root>/output/{meta.json, backtest/*, analysis/*}.
//   py/report.py 直读 output/ 生成 HTML 报告.

#include "analysis/analysis.hpp"
#include "api/bigquant/import.hpp"
#include "api/bigquant/pipeline.hpp"
#include "api/bigquant/spec.hpp"
#include "api/tushare/pipeline.hpp"
#include "api/tushare/spec.hpp"
#include "backtest/backtest.hpp"
#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/build.hpp"
#include "feature/describe.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

// output/meta.json: py/report.py 直读的元数据 + 计时.
//   schema 仅含 py 实际消费的字段 (dates / codes / factor_names / config.ic_ma_window /
//   timing.{backtest_seconds, analysis_seconds, tensor_bytes}); 其他诊断字段不写.
//   yyjson + misc::atomic_write_json (与 bigquant / tushare store 对仗).
void write_meta(const feature::Axes &axes, const feature::Tensor &T,
                double bt_seconds, double an_seconds) {
  namespace fs = std::filesystem;
  fs::path out_dir = misc::git_root() / "output";
  fs::create_directories(out_dir);

  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  auto add_str_arr = [&](const char *key,
                         const std::vector<std::string> &v) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    for (const std::string &s : v) {
      yyjson_mut_arr_add_strn(doc, arr, s.data(), s.size());
    }
    yyjson_mut_obj_add_val(doc, root, key, arr);
  };

  add_str_arr("dates", axes.dates);
  add_str_arr("codes", axes.codes);

  yyjson_mut_val *factor_arr = yyjson_mut_arr(doc);
  for (std::size_t i = 0; i < feature::FEATURES.size(); ++i) {
    if (feature::FEATURES[i].kind == feature::Kind::Factor) {
      yyjson_mut_arr_add_str(doc, factor_arr, feature::FEATURES[i].name);
    }
  }
  yyjson_mut_obj_add_val(doc, root, "factor_names", factor_arr);

  yyjson_mut_val *cfg = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_int(doc, cfg, "ic_ma_window",
                         ::config::ANALYSIS_IC_MA_WINDOW);
  yyjson_mut_obj_add_val(doc, root, "config", cfg);

  std::size_t tensor_bytes = 0;
  for (const auto &m : T.mats) tensor_bytes += m.size() * sizeof(float);
  yyjson_mut_val *timing = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_real(doc, timing, "backtest_seconds", bt_seconds);
  yyjson_mut_obj_add_real(doc, timing, "analysis_seconds", an_seconds);
  yyjson_mut_obj_add_uint(doc, timing, "tensor_bytes",
                          static_cast<std::uint64_t>(tensor_bytes));
  yyjson_mut_obj_add_val(doc, root, "timing", timing);

  misc::atomic_write_json(out_dir / "meta.json", doc);
  yyjson_mut_doc_free(doc);
}

} // namespace

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

  // ---- Phase 2.5: per-feature × per-year describe (gate config::DESCRIBE_ENABLE)
  if (::config::DESCRIBE_ENABLE) {
    feature::describe(axes, T);
  }

  // ---- Phase 3: backtest (per-D 状态机) → output/backtest/*.npy + labels.json
  double bt_seconds = backtest::run(axes, meta, T);

  // ---- Phase 4: analysis (per-D IC / quantile / corr) → output/analysis/*.npy
  double an_seconds = analysis::run(axes, T);

  // ---- 收尾: output/meta.json (axes / factor_names / timing) ----
  write_meta(axes, T, bt_seconds, an_seconds);

  return 0;
}
