#include "analysis/analysis.hpp"
#include "backtest/backtest.hpp"
#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/build.hpp"
#include "feature/describe.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "tushare/pipeline.hpp"
#include "tushare/spec.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

// 写 output/meta.json — py 报告生成器读它取 axes 标签 / factor 名 / 配置 / 计时.
//   内容是手摸 JSON; 不引入 yyjson, 因为只是少量标量+数组. 字符串假设无引号.
void write_meta(const feature::Axes &axes, const feature::Tensor &T,
                double bt_seconds, double an_seconds) {
  namespace fs = std::filesystem;
  fs::path out = misc::git_root() / "output";
  fs::create_directories(out);
  std::ofstream f(out / "meta.json");
  assert(f.is_open());

  auto dump_str_arr = [&](const std::vector<std::string> &v) {
    f << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
      f << '"' << v[i] << '"';
      if (i + 1 < v.size())
        f << ',';
    }
    f << ']';
  };

  // factor 列表 (Kind::Factor, 与 analysis 输出顺序一致)
  std::vector<std::string> factor_names;
  std::vector<int> factor_indices;
  for (std::size_t i = 0; i < feature::FEATURES.size(); ++i) {
    if (feature::FEATURES[i].kind == feature::Kind::Factor) {
      factor_names.emplace_back(feature::FEATURES[i].name);
      factor_indices.push_back(static_cast<int>(i));
    }
  }
  // filter 列表
  std::vector<std::string> filter_names;
  std::vector<std::string> enabled_filter_names;
  for (std::size_t i = 0; i < feature::FEATURES.size(); ++i) {
    if (feature::FEATURES[i].kind == feature::Kind::Filter) {
      filter_names.emplace_back(feature::FEATURES[i].name);
    }
  }
  for (feature::F ef : ::config::ENABLED_FILTERS) {
    enabled_filter_names.emplace_back(
        feature::FEATURES[static_cast<std::size_t>(ef)].name);
  }
  // factor weights 配置 (与 FACTOR_WEIGHTS 同序)
  std::vector<std::string> fw_names;
  std::vector<float> fw_weights;
  for (const auto &fw : ::config::FACTOR_WEIGHTS) {
    fw_names.emplace_back(
        feature::FEATURES[static_cast<std::size_t>(fw.f)].name);
    fw_weights.push_back(fw.w);
  }

  std::size_t tensor_bytes = 0;
  for (const auto &m : T.mats)
    tensor_bytes += m.size() * sizeof(float);

  f << "{\n";
  f << "  \"dates\": ";
  dump_str_arr(axes.dates);
  f << ",\n";
  f << "  \"codes\": ";
  dump_str_arr(axes.codes);
  f << ",\n";
  f << "  \"factor_names\": ";
  dump_str_arr(factor_names);
  f << ",\n";
  f << "  \"filter_names\": ";
  dump_str_arr(filter_names);
  f << ",\n";
  f << "  \"enabled_filters\": ";
  dump_str_arr(enabled_filter_names);
  f << ",\n";
  f << "  \"factor_weights_names\": ";
  dump_str_arr(fw_names);
  f << ",\n";
  f << "  \"factor_weights_values\": [";
  for (std::size_t i = 0; i < fw_weights.size(); ++i) {
    f << fw_weights[i];
    if (i + 1 < fw_weights.size())
      f << ',';
  }
  f << "],\n";
  f << "  \"config\": {\n";
  f << "    \"backtest_start\": \"" << ::config::BACKTEST_START_DATE
    << "\",\n";
  f << "    \"backtest_end\": \"" << ::config::BACKTEST_END_DATE << "\",\n";
  f << "    \"hold_n\": " << ::config::BT_HOLD_N << ",\n";
  f << "    \"exit_ratio\": " << ::config::BT_EXIT_RATIO << ",\n";
  f << "    \"capital_base\": " << ::config::BT_CAPITAL_BASE << ",\n";
  f << "    \"buy_cost\": " << ::config::BT_BUY_COST << ",\n";
  f << "    \"sell_cost\": " << ::config::BT_SELL_COST << ",\n";
  f << "    \"universe_size\": " << ::config::UNIVERSE_SIZE << ",\n";
  f << "    \"n_quantiles\": " << ::config::N_QUANTILES << ",\n";
  f << "    \"ic_ma_window\": " << ::config::IC_MA_WINDOW << "\n";
  f << "  },\n";
  f << "  \"timing\": {\n";
  f << "    \"backtest_seconds\": " << bt_seconds << ",\n";
  f << "    \"analysis_seconds\": " << an_seconds << ",\n";
  f << "    \"tensor_bytes\": " << tensor_bytes << "\n";
  f << "  }\n";
  f << "}\n";
}

} // namespace

int main() {
  tushare::update(config::PIPELINE_START_DATE, misc::today_yyyymmdd(),
                  tushare::SPECS, config::LOOKBACK_DAYS);

  feature::Axes axes;
  feature::StockMeta meta;
  feature::Tensor T = feature::build(axes, meta);
  if (config::ENABLE_DESCRIBE) {
    feature::describe(axes, T);
  }

  double bt_sec = backtest::run(axes, meta, T);
  double an_sec = analysis::run(axes, T);
  write_meta(axes, T, bt_sec, an_sec);
  return 0;
}
