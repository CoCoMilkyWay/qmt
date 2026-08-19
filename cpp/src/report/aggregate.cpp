#include "report/aggregate.hpp"

#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/timer.hpp"
#include "package/yyjson/yyjson.h"
#include "report/json.hpp"
#include "report/metrics.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace report {

namespace fs = std::filesystem;

namespace {

// 把某策略的净值截到公共窗口 [d_lo_ag, d_lo_ag + n) 并归一到窗口首日 = 1.0.
//   归一不改变年化 / 波动 / 夏普 / 回撤 (全是比率型指标), 只让叠加图起点对齐.
std::vector<float> slice_norm(const backtest::Result &r, int d_lo_ag, int n) {
  int off = d_lo_ag - r.d_lo;
  assert(off >= 0 && off + n <= static_cast<int>(r.strategy_nav.size()) &&
         "aggregate: 公共窗口越出该策略回测窗口");
  float base = r.strategy_nav[static_cast<std::size_t>(off)];
  assert(base > 0.0f && "aggregate: 窗口首日净值必须 > 0");
  std::vector<float> out(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i)
    out[static_cast<std::size_t>(i)] =
        r.strategy_nav[static_cast<std::size_t>(off + i)] / base;
  return out;
}

// 某策略在全局 D 索引 d 当日的持仓段 [lo, hi) (索引到 r.hold_codes).
std::pair<std::size_t, std::size_t> hold_span(const backtest::Result &r, int d) {
  int i = d - r.d_lo;
  assert(i >= 0 && i + 1 < static_cast<int>(r.hold_off.size()) &&
         "aggregate: 日期越出该策略持仓 CSR");
  return {static_cast<std::size_t>(r.hold_off[static_cast<std::size_t>(i)]),
          static_cast<std::size_t>(r.hold_off[static_cast<std::size_t>(i) + 1])};
}

void add_nav_stats(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                   std::string_view key, const NavStats &s,
                   const RelStats *rel) {
  yyjson_mut_val *o = add_obj(doc, parent, key);
  yyjson_mut_obj_add_int(doc, o, "天数", s.n_days);
  add_f4(doc, o, "年化", s.ann_return);
  add_f4(doc, o, "波动率", s.ann_vol);
  add_f4(doc, o, "夏普", s.sharpe);
  add_f4(doc, o, "最大回撤", s.max_drawdown);
  yyjson_mut_obj_add_int(doc, o, "创新高最长天数", s.longest_no_new_high);
  if (rel != nullptr) {
    add_f4(doc, o, "信息比率", rel->info_ratio);
    add_f4(doc, o, "Beta", rel->beta);
    add_f4(doc, o, "Alpha", rel->alpha);
    add_f4(doc, o, "跟踪误差", rel->tracking_error);
  }
}

} // namespace

double aggregate(const feature::Axes &axes,
                 std::span<const backtest::Result> results) {
  misc::Timer t("[aggregate] run");
  auto t0 = std::chrono::high_resolution_clock::now();

  int n_s = static_cast<int>(results.size());
  assert(n_s == strategy::N_STRATEGIES &&
         "aggregate: results 与 STRATEGIES[] 数量不一致");
  assert(n_s > 0 && "aggregate: 无策略");

  // ---- 公共窗口 = 各策略窗口交集 ------------------------------------------
  int d_lo = results[0].d_lo;
  for (const backtest::Result &r : results)
    d_lo = std::max(d_lo, r.d_lo);
  int d_hi = axes.n_d(); // 右端点全策略统一 = axes 最新日 + 1 (half-open)
  int n_d = d_hi - d_lo;
  assert(n_d > 0 && "aggregate: 各策略回测窗口交集为空");

  std::vector<std::int32_t> dates_out(static_cast<std::size_t>(n_d));
  for (int i = 0; i < n_d; ++i)
    dates_out[static_cast<std::size_t>(i)] = d_lo + i;

  // ---- 各策略净值 (归一) + 日收益 -----------------------------------------
  std::vector<std::vector<float>> navs;
  std::vector<std::vector<float>> rets;
  navs.reserve(static_cast<std::size_t>(n_s));
  rets.reserve(static_cast<std::size_t>(n_s));
  for (const backtest::Result &r : results) {
    navs.push_back(slice_norm(r, d_lo, n_d));
    rets.push_back(daily_returns(navs.back()));
  }

  // 扁平 [n_s, n_d] 落盘
  std::vector<float> nav_flat(static_cast<std::size_t>(n_s) *
                              static_cast<std::size_t>(n_d));
  for (int s = 0; s < n_s; ++s) {
    std::copy(navs[static_cast<std::size_t>(s)].begin(),
              navs[static_cast<std::size_t>(s)].end(),
              nav_flat.begin() + static_cast<std::ptrdiff_t>(s) * n_d);
  }

  // ---- 等权组合: 每日 rebalance 到 1/n_s ⇒ 组合日收益 = 各策略日收益均值 ----
  std::vector<float> combo_ret(static_cast<std::size_t>(n_d), 0.0f);
  for (int i = 0; i < n_d; ++i) {
    double sum = 0.0;
    for (int s = 0; s < n_s; ++s)
      sum += rets[static_cast<std::size_t>(s)][static_cast<std::size_t>(i)];
    combo_ret[static_cast<std::size_t>(i)] =
        static_cast<float>(sum / static_cast<double>(n_s));
  }
  std::vector<float> combo_nav = cum_nav(combo_ret);

  // ---- 策略间日收益相关矩阵 (分散化价值的定量依据) ------------------------
  std::vector<float> corr(static_cast<std::size_t>(n_s) *
                              static_cast<std::size_t>(n_s),
                          std::nanf(""));
  for (int i = 0; i < n_s; ++i) {
    for (int j = i; j < n_s; ++j) {
      float c = (i == j) ? 1.0f
                         : pearson(rets[static_cast<std::size_t>(i)],
                                   rets[static_cast<std::size_t>(j)]);
      corr[static_cast<std::size_t>(i) * static_cast<std::size_t>(n_s) +
           static_cast<std::size_t>(j)] = c;
      corr[static_cast<std::size_t>(j) * static_cast<std::size_t>(n_s) +
           static_cast<std::size_t>(i)] = c;
    }
  }

  // ---- 持仓重叠度: 每日被 ≥2 策略同时持有的股票数 -------------------------
  //   多策略同选一只票 ⇒ 实盘该股实际敞口翻倍, 是四份独立报告看不见的真实风险.
  std::vector<std::int32_t> overlap(static_cast<std::size_t>(n_d), 0);
  {
    std::unordered_map<int, int> cnt;
    for (int i = 0; i < n_d; ++i) {
      cnt.clear();
      for (int s = 0; s < n_s; ++s) {
        const backtest::Result &r = results[static_cast<std::size_t>(s)];
        auto [lo, hi] = hold_span(r, d_lo + i);
        for (std::size_t k = lo; k < hi; ++k)
          ++cnt[static_cast<int>(r.hold_codes[k])];
      }
      int n_dup = 0;
      for (const auto &kv : cnt) {
        if (kv.second >= 2)
          ++n_dup;
      }
      overlap[static_cast<std::size_t>(i)] = static_cast<std::int32_t>(n_dup);
    }
  }

  // ---- 写盘 ----------------------------------------------------------------
  fs::path out = misc::git_root() / "output" / "aggregate";
  fs::create_directories(out);
  {
    std::size_t s1[1] = {static_cast<std::size_t>(n_d)};
    misc::write_npy_i4(out / "dates.npy",
                       std::span<const std::int32_t>(dates_out),
                       std::span<const std::size_t>(s1, 1));
    misc::write_npy_f4(out / "combo_nav.npy",
                       std::span<const float>(combo_nav),
                       std::span<const std::size_t>(s1, 1));
    misc::write_npy_i4(out / "overlap_count.npy",
                       std::span<const std::int32_t>(overlap),
                       std::span<const std::size_t>(s1, 1));
    std::size_t s2[2] = {static_cast<std::size_t>(n_s),
                         static_cast<std::size_t>(n_d)};
    misc::write_npy_f4(out / "strategy_nav.npy",
                       std::span<const float>(nav_flat),
                       std::span<const std::size_t>(s2, 2));
    std::size_t sc[2] = {static_cast<std::size_t>(n_s),
                         static_cast<std::size_t>(n_s)};
    misc::write_npy_f4(out / "corr.npy", std::span<const float>(corr),
                       std::span<const std::size_t>(sc, 2));
  }

  // ---- report.json ---------------------------------------------------------
  {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    std::vector<std::string_view> names;
    names.reserve(static_cast<std::size_t>(n_s));
    for (const strategy::StrategySpec *spec : strategy::STRATEGIES)
      names.push_back(spec->name);
    add_sv_arr(doc, root, "strategies", names);

    // 基准 = strategy::BENCHMARK 的日收益 (nullptr ⇒ 不出相对指标)
    int bench_idx = -1;
    for (int s = 0; s < n_s; ++s) {
      if (strategy::STRATEGIES[static_cast<std::size_t>(s)] ==
          strategy::BENCHMARK)
        bench_idx = s;
    }
    if (bench_idx >= 0) {
      add_str(doc, root, "benchmark",
              strategy::STRATEGIES[static_cast<std::size_t>(bench_idx)]->name);
    } else {
      yyjson_mut_obj_add_null(doc, root, "benchmark");
    }

    yyjson_mut_val *met = add_obj(doc, root, "metrics");
    for (int s = 0; s < n_s; ++s) {
      NavStats st = nav_stats(navs[static_cast<std::size_t>(s)]);
      RelStats rel;
      bool has_rel = bench_idx >= 0 && s != bench_idx;
      if (has_rel)
        rel = rel_stats(rets[static_cast<std::size_t>(s)],
                        rets[static_cast<std::size_t>(bench_idx)]);
      add_nav_stats(doc, met, names[static_cast<std::size_t>(s)], st,
                    has_rel ? &rel : nullptr);
    }
    {
      NavStats st = nav_stats(combo_nav);
      RelStats rel;
      bool has_rel = bench_idx >= 0;
      if (has_rel)
        rel = rel_stats(combo_ret, rets[static_cast<std::size_t>(bench_idx)]);
      add_nav_stats(doc, met, "等权组合", st, has_rel ? &rel : nullptr);
    }

    // ---- 今日多策略下单台 ------------------------------------------------
    //   末日各策略持仓合并; per_w[s][row] = 该策略给该股的权重 (0 = 未持有),
    //   combo_w = Σ per_w / n_s (等权组合下的实际敞口), n_hit = 命中策略数.
    //   按 combo_w 降序 ⇒ 敞口最大的 (通常是被多策略同选的重叠股) 排最前.
    {
      int d_last = d_hi - 1;
      std::unordered_map<int, std::vector<float>> by_a;
      for (int s = 0; s < n_s; ++s) {
        const backtest::Result &r = results[static_cast<std::size_t>(s)];
        auto [lo, hi] = hold_span(r, d_last);
        for (std::size_t k = lo; k < hi; ++k) {
          int a = static_cast<int>(r.hold_codes[k]);
          auto it = by_a.find(a);
          if (it == by_a.end())
            it = by_a.emplace(a, std::vector<float>(
                                     static_cast<std::size_t>(n_s), 0.0f))
                     .first;
          it->second[static_cast<std::size_t>(s)] = r.hold_weights[k];
        }
      }

      struct Row {
        int a;
        float combo_w;
        int n_hit;
        std::vector<float> w;
      };
      std::vector<Row> rows;
      rows.reserve(by_a.size());
      for (auto &kv : by_a) {
        float sum = 0.0f;
        int hit = 0;
        for (float w : kv.second) {
          sum += w;
          if (w > 0.0f)
            ++hit;
        }
        rows.push_back(Row{kv.first, sum / static_cast<float>(n_s), hit,
                           std::move(kv.second)});
      }
      std::sort(rows.begin(), rows.end(), [](const Row &x, const Row &y) {
        if (x.combo_w != y.combo_w)
          return x.combo_w > y.combo_w;
        return x.a < y.a; // 权重并列时按 a 升序, 保证输出确定
      });

      std::vector<std::int32_t> c_a, c_hit;
      std::vector<float> c_combo;
      c_a.reserve(rows.size());
      c_hit.reserve(rows.size());
      c_combo.reserve(rows.size());
      for (const Row &r : rows) {
        c_a.push_back(static_cast<std::int32_t>(r.a));
        c_hit.push_back(static_cast<std::int32_t>(r.n_hit));
        c_combo.push_back(r.combo_w);
      }

      yyjson_mut_val *desk = add_obj(doc, root, "desk");
      add_i4_arr(doc, desk, "a", c_a);
      add_i4_arr(doc, desk, "n_hit", c_hit);
      add_f4_arr(doc, desk, "combo_weight", c_combo);
      // per-strategy 权重列 (与 strategies[] 同序), 0 = 该策略未持有
      yyjson_mut_val *pw = add_obj(doc, desk, "weights");
      for (int s = 0; s < n_s; ++s) {
        std::vector<float> col;
        col.reserve(rows.size());
        for (const Row &r : rows)
          col.push_back(r.w[static_cast<std::size_t>(s)]);
        add_f4_arr(doc, pw, names[static_cast<std::size_t>(s)], col);
      }
    }

    misc::atomic_write_json(out / "report.json", doc);
    yyjson_mut_doc_free(doc);
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = t1 - t0;
  return dur.count();
}

} // namespace report
