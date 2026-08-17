// 主入口: [preflight → bigquant::update → tushare::update] → feature::build →
//   backtest::run → analysis::run → write_meta. 完整闭环: 数据接入 → PIT 张量 →
//   回测 → 因子分析 → summary JSON, 落 <git_root>/output/{meta.json, backtest/*,
//   analysis/*}. py/report.py 直读 output/ 生成 HTML 报告.
//   方括号段由 config::PIPELINE_UPDATE 门控: false 则完全不联网, 直接吃本地 parquet.

#include "analysis/analysis.hpp"
#include "api/bigquant/dai.hpp"
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
#include <iomanip>
#include <iostream>
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
  for (const auto &m : T.mats)
    tensor_bytes += m.size() * sizeof(float);
  yyjson_mut_val *timing = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_real(doc, timing, "backtest_seconds", bt_seconds);
  yyjson_mut_obj_add_real(doc, timing, "analysis_seconds", an_seconds);
  yyjson_mut_obj_add_uint(doc, timing, "tensor_bytes",
                          static_cast<std::uint64_t>(tensor_bytes));
  yyjson_mut_obj_add_val(doc, root, "timing", timing);

  misc::atomic_write_json(out_dir / "meta.json", doc);
  yyjson_mut_doc_free(doc);
}

// ANSI 转义 (run.py → subprocess 继承 tty, 直显).
constexpr const char *C_RESET = "\033[0m";
constexpr const char *C_BOLD = "\033[1m";
constexpr const char *C_DIM = "\033[2m";
constexpr const char *C_RED = "\033[31m";
constexpr const char *C_GREEN = "\033[32m";
constexpr const char *C_YELLOW = "\033[33m";
constexpr const char *C_CYAN = "\033[36m";

// 周配额已用比例配色: < 60% 绿 / < 85% 黄 / 否则红.
const char *usage_color(double pct) {
  if (pct < 60.0)
    return C_GREEN;
  if (pct < 85.0)
    return C_YELLOW;
  return C_RED;
}

// preflight — 联网同步前的两路 API 状态展示 + 人工确认闸门.
//   tushare  最短探针 (forecast_vip 空 period, 0 行响应) 证明积分 ≥ 5000
//   bigquant DoAction("quota") 取周配额, 展示已用百分比
//     (配额按返回 cell 数计, 每周重置; 服务端不返回重置时刻, 故不展示)
// 返回 true 表示放行 (y / Y / 回车), 其余一律取消.
bool preflight() {
  std::cout << "\n"
            << C_BOLD << "preflight" << C_RESET << "\n";

  tushare::probe();
  std::cout << "  " << C_CYAN << "tushare " << C_RESET << " " << C_GREEN << "OK"
            << C_RESET << "   forecast_vip 可调 " << C_DIM << "(积分 ≥ 5000)"
            << C_RESET << "\n";

  bigquant::DaiClient client;
  bigquant::Quota q = client.quota();
  double pct =
      100.0 * static_cast<double>(q.used) / static_cast<double>(q.weekly);
  std::cout << "  " << C_CYAN << "bigquant" << C_RESET << " " << C_GREEN << "OK"
            << C_RESET << "   weekly quota " << usage_color(pct) << std::fixed
            << std::setprecision(1) << pct << "%" << C_RESET << " used "
            << C_DIM << "(" << q.used << " / " << q.weekly << ")" << C_RESET
            << "\n";

  std::cout << "\n"
            << C_BOLD << "拉取数据? [Y/n] " << C_RESET << std::flush;
  std::string line;
  std::getline(std::cin, line);
  return line.empty() || line == "y" || line == "Y";
}

} // namespace

int main() {
  std::string today = misc::today_yyyymmdd();

  // config::PIPELINE_UPDATE = false ⇒ 完全不联网, 直接用 data/ 现有 parquet 跑.
  // pending 纯本地判定: 全部表在 dedup 窗口内 / 已到水位 ⇒ 连 preflight 都跳过
  // (零网络, 连跑秒过); 有任一待拉才走 preflight → 确认 → update.
  if (::config::PIPELINE_UPDATE) {
    bool need = bigquant::pending(::config::PIPELINE_START_DATE, today,
                                  bigquant::SPECS,
                                  ::config::PIPELINE_LOOKBACK_DAYS) ||
                tushare::pending(::config::PIPELINE_START_DATE, today,
                                 tushare::SPECS,
                                 ::config::PIPELINE_LOOKBACK_DAYS);
    if (!need) {
      std::cout << "\n"
                << C_DIM
                << "[update] 全部表在 dedup 窗口内 / 已到水位, 跳过联网同步"
                << C_RESET << std::endl;
    } else {
      if (!preflight()) {
        std::cout << C_YELLOW << "已取消." << C_RESET << std::endl;
        return 0;
      }
      std::cout << std::endl;

      bigquant::update(::config::PIPELINE_START_DATE, today, bigquant::SPECS,
                       ::config::PIPELINE_LOOKBACK_DAYS);

      tushare::update(::config::PIPELINE_START_DATE, today, tushare::SPECS,
                      ::config::PIPELINE_LOOKBACK_DAYS);
    }
  }

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
