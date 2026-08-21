// 主入口: [preflight → bigquant::update → tushare::update] → feature::build →
//   per-strategy {backtest::run → analysis::run} → report::aggregate →
//   write_meta. 完整闭环: 数据接入 → PIT 张量 (共享图 + 策略列) → 回测 → 因子
//   分析 → 跨策略聚合 → summary JSON, 落 <git_root>/output/{meta.json,
//   strategy/<name>/{backtest/*, analysis/*}, aggregate/*}.
//   py/report.py 直读 output/ 生成 HTML 报告.
//   方括号段由 config::PIPELINE_UPDATE 门控: false 则完全不联网, 直接吃本地 parquet.

#include "analysis/analysis.hpp"
#include "api/bigquant/dai.hpp"
#include "api/bigquant/pipeline.hpp"
#include "api/bigquant/spec.hpp"
#include "api/tushare/pipeline.hpp"
#include "api/tushare/spec.hpp"
#include "backtest/backtest.hpp"
#include "config_main.hpp"
#include "config_mine.hpp"
#include "feature/axis.hpp"
#include "feature/build.hpp"
#include "feature/describe.hpp"
#include "feature/registry.hpp"
#include "feature/report.hpp"
#include "feature/tensor.hpp"
#include "mine/mine.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"
#include "report/aggregate.hpp"
#include "report/json.hpp"
#include "report/labels.hpp"
#include "strategy/registry.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// ANSI 转义 (run.py → subprocess 继承 tty, 直显).
constexpr const char *C_RESET = "\033[0m";
constexpr const char *C_BOLD = "\033[1m";
constexpr const char *C_DIM = "\033[2m";
constexpr const char *C_RED = "\033[31m";
constexpr const char *C_GREEN = "\033[32m";
constexpr const char *C_YELLOW = "\033[33m";
constexpr const char *C_CYAN = "\033[36m";

// MarginPolicy → 展示名 (meta.json 的策略配置对比表用).
const char *margin_policy_name(strategy::MarginPolicy p) {
  switch (p) {
  case strategy::MarginPolicy::Exclude:
    return "排除两融";
  case strategy::MarginPolicy::Include:
    return "含两融";
  case strategy::MarginPolicy::Only:
    return "仅两融";
  }
  assert(false && "未覆盖的 MarginPolicy");
  return "";
}

// output/meta.json: py/report.py 直读的全局元数据 (轴 / per-a 标签 / 策略配置 /
//   运行诊断). 序列与指标都在各自的 output/**/{*.npy, report.json} 里, 这里只放
//   "全策略共享且前端要按索引查表" 的东西.
//   schema:
//     dates[]        axes D 轴 "YYYYMMDD"
//     codes[]        axes A 轴 instrument
//     names[]        per-a 末日简称 (与 codes 同序; 末日持仓 / 下单台显示用)
//     industries[]   per-a "SW2021一级 -- 二级" (见 report/labels.hpp)
//     factor_names[]        Kind::Factor 全集 (英文, analysis 的因子轴顺序)
//     factor_cn_names[]      对应中文名 (与 factor_names 同序)
//     strategies[]   {name, hold_n, exit_ratio, start_date, filters[],
//                     weights{}, pool{...}, timing{backtest_seconds,
//                     analysis_seconds}}  ← pool 摘要供策略配置对比表
//     config{ic_ma_window, n_quantiles, capital_base, buy_cost, sell_cost}
//     timing{tensor_bytes, aggregate_seconds}
//   yyjson + misc::atomic_write_json (与 bigquant / tushare store 对仗).
void write_meta(const feature::Axes &axes, const feature::Tensor &T,
                const feature::StockMeta &meta,
                const backtest::NameTimeline &name_timeline,
                std::span<const backtest::Result> results, double ag_seconds) {
  namespace fs = std::filesystem;
  fs::path out_dir = misc::git_root() / "output";
  fs::create_directories(out_dir);

  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  report::add_str_arr(doc, root, "dates", axes.dates);
  report::add_str_arr(doc, root, "codes", axes.codes);
  report::add_str_arr(doc, root, "names",
                      backtest::last_names(axes, meta, name_timeline));
  report::add_str_arr(doc, root, "industries",
                      report::load_industry_labels(axes));

  yyjson_mut_val *factor_arr = report::add_arr(doc, root, "factor_names");
  yyjson_mut_val *factor_cn_arr = report::add_arr(doc, root, "factor_cn_names");
  for (const feature::FeatureSpec *f : feature::ALL_NODES) {
    if (f->kind == feature::Kind::Factor) {
      yyjson_mut_arr_add_str(doc, factor_arr, f->name);
      yyjson_mut_arr_add_str(doc, factor_cn_arr, f->cn_name);
    }
  }

  yyjson_mut_val *strat_arr = report::add_arr(doc, root, "strategies");
  for (int i = 0; i < strategy::N_STRATEGIES; ++i) {
    const strategy::StrategySpec *spec =
        strategy::STRATEGIES[static_cast<std::size_t>(i)];
    yyjson_mut_val *s = yyjson_mut_arr_add_obj(doc, strat_arr);
    report::add_str(doc, s, "name", spec->name);
    yyjson_mut_obj_add_int(doc, s, "hold_n", spec->hold_n);
    yyjson_mut_obj_add_real(doc, s, "exit_ratio",
                            static_cast<double>(spec->exit_ratio));
    yyjson_mut_obj_add_str(doc, s, "start_date", spec->bt_start_date);
    yyjson_mut_val *filters = report::add_arr(doc, s, "filters");
    for (const feature::FeatureSpec *f : spec->filters)
      yyjson_mut_arr_add_str(doc, filters, f->name);
    yyjson_mut_val *weights = report::add_obj(doc, s, "weights");
    for (const strategy::FactorWeight &fw : spec->weights)
      yyjson_mut_obj_add_real(doc, weights, fw.f->name,
                              static_cast<double>(fw.w));

    // pool 摘要 — 策略配置对比表 (四个策略到底差在哪) 的唯一数据源.
    yyjson_mut_val *pool = report::add_obj(doc, s, "pool");
    report::add_sv_arr(doc, pool, "exchange", spec->pool.exchange_wl);
    report::add_sv_arr(doc, pool, "list_sector", spec->pool.list_sector_wl);
    report::add_sv_arr(doc, pool, "industry_l1", spec->pool.industry_l1_wl);
    yyjson_mut_obj_add_str(doc, pool, "margin_policy",
                           margin_policy_name(spec->pool.margin_policy));
    yyjson_mut_obj_add_str(doc, pool, "rank_key", spec->pool.rank_key->name);
    yyjson_mut_obj_add_bool(doc, pool, "rank_asc", spec->pool.rank_asc);
    yyjson_mut_obj_add_int(doc, pool, "universe_size",
                           spec->pool.universe_size);

    // per-strategy 计时 (原先是全策略累加, 多策略下对任何单个策略都不成立)
    yyjson_mut_val *tm = report::add_obj(doc, s, "timing");
    yyjson_mut_obj_add_real(doc, tm, "backtest_seconds",
                            results[static_cast<std::size_t>(i)].elapsed_seconds);
    yyjson_mut_obj_add_real(
        doc, tm, "analysis_seconds",
        results[static_cast<std::size_t>(i)].analysis_seconds);
  }

  yyjson_mut_val *cfg = report::add_obj(doc, root, "config");
  yyjson_mut_obj_add_int(doc, cfg, "ic_ma_window",
                         ::config::ANALYSIS_IC_MA_WINDOW);
  yyjson_mut_obj_add_int(doc, cfg, "n_quantiles",
                         ::config::ANALYSIS_N_QUANTILES);
  report::add_f4(doc, cfg, "capital_base", ::config::BACKTEST_CAPITAL_BASE);
  report::add_f4(doc, cfg, "buy_cost", ::config::BACKTEST_BUY_COST);
  report::add_f4(doc, cfg, "sell_cost", ::config::BACKTEST_SELL_COST);

  std::size_t tensor_bytes = 0;
  for (const auto &m : T.mats)
    tensor_bytes += m.size() * sizeof(float);
  for (const auto &m : T.strat_mats)
    tensor_bytes += m.size() * sizeof(float);
  yyjson_mut_val *timing = report::add_obj(doc, root, "timing");
  yyjson_mut_obj_add_uint(doc, timing, "tensor_bytes",
                          static_cast<std::uint64_t>(tensor_bytes));
  yyjson_mut_obj_add_real(doc, timing, "aggregate_seconds", ag_seconds);

  misc::atomic_write_json(out_dir / "meta.json", doc);
  yyjson_mut_doc_free(doc);
}

// 清掉 output/strategy/ 下不在 STRATEGIES[] 里的子目录 — 删过 / 改名过的策略会
//   留下幽灵输出 (报告按 meta.json 迭代不会误读, 但会一直堆磁盘且让人误判).
void prune_strategy_dirs() {
  namespace fs = std::filesystem;
  fs::path root = misc::git_root() / "output" / "strategy";
  if (!fs::exists(root))
    return;
  std::unordered_set<std::string> live;
  for (const strategy::StrategySpec *spec : strategy::STRATEGIES)
    live.insert(std::string(spec->name));
  for (const auto &e : fs::directory_iterator(root)) {
    if (!e.is_directory())
      continue;
    std::string name = e.path().filename().string();
    if (live.count(name))
      continue;
    std::cout << C_DIM << "[output] 清理已下线策略目录 " << name << C_RESET
              << std::endl;
    fs::remove_all(e.path());
  }
}

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

  // ---- Phase 2.4: 特征依赖表 (公共 → 各策略专属, gate config::FEATURE_TABLE_ENABLE)
  if (::config::FEATURE_TABLE_ENABLE) {
    feature::print_dependency_table();
  }

  // ---- Phase 2.5: per-feature × per-year describe (gate config::DESCRIBE_ENABLE)
  if (::config::DESCRIBE_ENABLE) {
    feature::describe(axes, T);
  }

  // ---- Phase 2.6: 逐 (a,d) 张量导出 → output/tensor/*.npy (gate
  //   config::TENSOR_DUMP_ENABLE), 供 test/ Python 参考实现逐点对账
  if (::config::TENSOR_DUMP_ENABLE) {
    feature::dump_tensor(axes, T);
  }

  // ---- Phase 3+4: per-strategy backtest (per-D 状态机) + analysis (per-D IC /
  //   quantile / corr) → output/strategy/<name>/{backtest, analysis}/*.npy.
  //   name_timeline 全策略共享, 循环外只加载一次; Result 留到聚合层用.
  //   写任何输出之前先清幽灵目录, 保证 output/strategy/ 恰好 = STRATEGIES[].
  prune_strategy_dirs();
  backtest::NameTimeline name_timeline = backtest::load_name_timeline(axes);
  std::vector<backtest::Result> results;
  results.reserve(static_cast<std::size_t>(strategy::N_STRATEGIES));
  for (int s = 0; s < strategy::N_STRATEGIES; ++s) {
    const strategy::StrategySpec &spec =
        *strategy::STRATEGIES[static_cast<std::size_t>(s)];
    std::cout << "\n[strategy] " << spec.name << std::endl;
    results.push_back(backtest::run(axes, meta, T, name_timeline, spec, s));
    results.back().analysis_seconds = analysis::run(axes, T, spec, s);
  }

  // ---- Phase 4.5: 因子权重 lattice 挖掘 → output/mine/ (gate mine::MINE_ENABLE)
  //   继承 mine::MINE_STRATEGY 那个策略的全部配置 (只搜 weights):
  //   全格滑窗分层 + 顶档滑窗夏普 → 三个截面分数相乘 → 持仓去重 → 最终名单真
  //   回测 (走 backtest/engine.hpp 同一份内核; 启动先拿该策略自己的 weights 与
  //   刚跑完的 strategy_nav 对账). 后处理见 py/app/mine.py.
  if (mine::MINE_ENABLE) {
    double mine_seconds = mine::run(axes, T, results);
    std::cout << C_DIM << "[mine] 总耗时 " << mine_seconds << "s" << C_RESET
              << std::endl;
  }

  // ---- Phase 5: 跨策略聚合 → output/aggregate/{*.npy, report.json} ----
  double ag_seconds = report::aggregate(axes, results);

  // ---- 收尾: output/meta.json (轴 / per-a 标签 / 策略配置 / 计时) ----
  write_meta(axes, T, meta, name_timeline, results, ag_seconds);

  return 0;
}
