#include "mine/mine.hpp"

#include "backtest/engine.hpp"
#include "feature/cs.hpp"
#include "mine/spec.hpp"
#include "misc/affinity.hpp"
#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/timer.hpp"
#include "package/yyjson/yyjson.h"
#include "report/json.hpp"
#include "report/metrics.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace mine {

namespace fs = std::filesystem;

namespace {

using backtest::Engine;
using backtest::MarketWindow;
using backtest::NullRecorder;

// ---- 编译期校验 (spec.hpp 填错直接编译失败) --------------------------------
consteval bool validate_spec() {
  if (MINE_N_FACTORS <= 0 || MINE_LATTICE_M <= 0 || MINE_LATTICE_M > 127)
    return false;
  for (int i = 0; i < MINE_N_FACTORS; ++i) {
    const feature::FeatureSpec *f = MINE_FACTORS[i];
    if (f == nullptr || f->kind != feature::Kind::Factor)
      return false;
    for (int j = 0; j < i; ++j) {
      if (MINE_FACTORS[j] == f)
        return false;
    }
  }
  return true;
}

static_assert(validate_spec(), "MINE_FACTORS 必须非空 / 无重复 / 全 Kind::Factor; "
                               "MINE_LATTICE_M ∈ [1, 127]");

// top-K 选择余量: 归一化 (÷Σ|w|) 前后并列格局可能不同 (不同 acc 可映射到同一
//   float32 score), 故多选 TIE_MARGIN 个再归一重排. 边界仍并列 ⇒ assert.
constexpr int TIE_MARGIN = 16;

// 每线程一次带走的权重数. 内层循环换成 "先日后权重" 后, 一个日块
//   (cnt × F × 4B ≈ 100 KB) 在整批权重上复用 ⇒ DRAM 流量降到 1/BATCH.
//   这是"几百万组合"能在分钟级跑完的关键 (纯算力反而不是瓶颈).
constexpr int BATCH = 256;

// 窗口左端: 与 backtest.cpp find_d(floor=false) 同口径 (≥ start 的最小索引).
int lower_d(const feature::Axes &axes, std::string_view ymd) {
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), ymd);
  return (it == axes.dates.end())
             ? -1
             : static_cast<int>(it - axes.dates.begin());
}

// 内核实际会读到的候选前缀长度 (见 backtest/engine.hpp Engine::step):
//   卖出要查 top-(hold_n × exit_ratio); 买入循环从 0 走到第一个
//   "rank ≥ hold_n 且非持仓" 处 break — 持仓跳过不 break, 而进入买入段要求
//   kept ≤ hold_n − 1 ⇒ 最远只可能走到下标 2·hold_n − 1.
//   取二者上界即可让"截断候选"与"传全量候选"逐位等价.
int prefix_need(const strategy::StrategySpec &spec) {
  int n_exit = static_cast<int>(static_cast<float>(spec.hold_n) * spec.exit_ratio);
  return std::max(n_exit, 2 * spec.hold_n);
}

// ============================================================================
// 池内因子分位矩阵 — 逐权重回测唯一的读数据源, 全程只读共享.
//   pool 只取决于策略 (filters / rank_key / universe_size), 与权重无关 ⇒
//   每日每因子的池内分位可以预计算一次, 逐权重只剩一次点积.
//   CSR over 窗口日: 第 i 天池成员 memb[off[i] .. off[i+1]) (a 升序),
//   分位块 pr[off[i]·F + f·cnt + j] = pct_rank_pool(MINE_FACTORS[f]) 第 j 个成员.
//   因子主序 ⇒ 点积 = F 遍连续 axpy (可向量化), 且日块整体常驻 L2.
// ============================================================================
struct PrStore {
  int n_d = 0;
  int max_cnt = 0;
  std::vector<std::int64_t> off; // [n_d + 1]
  std::vector<std::int32_t> memb;
  std::vector<float> pr;

  int cnt(int i) const { return static_cast<int>(off[i + 1] - off[i]); }
  const std::int32_t *members(int i) const { return memb.data() + off[i]; }
  const float *block(int i) const {
    return pr.data() + off[i] * static_cast<std::int64_t>(MINE_N_FACTORS);
  }
};

PrStore build_pr(const feature::Axes &axes, const feature::Tensor &T, int s_idx,
                 int d_lo, int n_d) {
  misc::Timer t("[mine] 池内因子分位预计算");
  const int n_a = axes.n_a();
  const int slot_pool = strategy::slot(s_idx, strategy::SF::pool);
  unsigned n_threads = misc::Affinity::core_count();

  PrStore st;
  st.n_d = n_d;
  st.off.assign(static_cast<std::size_t>(n_d) + 1, 0);

  // pass 1: per-day 池成员数 → 前缀和 (CSR offset)
  {
    std::atomic<int> next{0};
    auto worker = [&]() {
      std::vector<float> pool(static_cast<std::size_t>(n_a));
      for (;;) {
        int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= n_d)
          break;
        T.strat_gather_cs_row(slot_pool, d_lo + i, pool);
        std::int64_t c = 0;
        for (int a = 0; a < n_a; ++a) {
          if (pool[static_cast<std::size_t>(a)] > 0.5f)
            ++c;
        }
        st.off[static_cast<std::size_t>(i) + 1] = c;
      }
    };
    std::vector<std::thread> ths;
    ths.reserve(n_threads);
    for (unsigned k = 0; k < n_threads; ++k)
      ths.emplace_back(worker);
    for (auto &th : ths)
      th.join();
  }
  for (int i = 0; i < n_d; ++i) {
    std::int64_t c = st.off[static_cast<std::size_t>(i) + 1];
    assert(c > 0 && "pool 空日 — 策略母集当日无标的, 回测无意义");
    st.max_cnt = std::max(st.max_cnt, static_cast<int>(c));
    st.off[static_cast<std::size_t>(i) + 1] = st.off[static_cast<std::size_t>(i)] + c;
  }
  std::int64_t total = st.off[static_cast<std::size_t>(n_d)];
  st.memb.resize(static_cast<std::size_t>(total));
  st.pr.resize(static_cast<std::size_t>(total) *
               static_cast<std::size_t>(MINE_N_FACTORS));

  // pass 2: 成员表 + 池内分位. 与 strategy/columns.cpp::cs_score 同口径 —
  //   池外置 NaN 后 pct_rank 只对池内格排秩, 等价于"直接对池内紧凑数组排秩"
  //   (pct 只取决于 finite 值的多重集与位次), 故这里直接算紧凑版.
  {
    std::atomic<int> next{0};
    auto worker = [&]() {
      std::vector<float> pool(static_cast<std::size_t>(n_a));
      std::vector<float> col(static_cast<std::size_t>(n_a));
      std::vector<float> tmp(static_cast<std::size_t>(st.max_cnt));
      for (;;) {
        int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= n_d)
          break;
        int d = d_lo + i;
        int cnt = st.cnt(i);
        std::int32_t *mem = st.memb.data() + st.off[static_cast<std::size_t>(i)];
        T.strat_gather_cs_row(slot_pool, d, pool);
        int j = 0;
        for (int a = 0; a < n_a; ++a) {
          if (pool[static_cast<std::size_t>(a)] > 0.5f)
            mem[j++] = a;
        }
        assert(j == cnt && "pool 两遍扫描不一致");
        float *blk = st.pr.data() + st.off[static_cast<std::size_t>(i)] *
                                        static_cast<std::int64_t>(MINE_N_FACTORS);
        for (int f = 0; f < MINE_N_FACTORS; ++f) {
          T.gather_cs_row(*MINE_FACTORS[f], d, col);
          for (int q = 0; q < cnt; ++q) {
            float v = col[static_cast<std::size_t>(mem[q])];
            assert(feature::is_finite(v) &&
                   "MINE_FACTORS 池内非 finite (factor 契约应保证)");
            tmp[static_cast<std::size_t>(q)] = v;
          }
          feature::pct_rank(std::span<float>(tmp.data(), static_cast<std::size_t>(cnt)));
          std::copy(tmp.data(), tmp.data() + cnt,
                    blk + static_cast<std::int64_t>(f) * cnt);
        }
      }
    };
    std::vector<std::thread> ths;
    ths.reserve(n_threads);
    for (unsigned k = 0; k < n_threads; ++k)
      ths.emplace_back(worker);
    for (auto &th : ths)
      th.join();
  }

  std::printf("[mine] 池: %lld 个 (日, 标的) 格, 日均 %.0f, 峰值 %d; "
              "分位矩阵 %.0f MB\n",
              static_cast<long long>(total),
              static_cast<double>(total) / static_cast<double>(n_d), st.max_cnt,
              static_cast<double>(st.pr.size() * sizeof(float)) / 1048576.0);
  return st;
}

// ============================================================================
// 一个权重方向的执行计划: (PR 列下标, 权重) 按**累加顺序**排好.
//   累加顺序 = cs_score 里 spec.weights 的遍历顺序 ⇒ 浮点求和逐位一致.
//   lattice 点按 MINE_FACTORS 顺序 (跳 k=0 项, 加 0 不改累加器, 等价);
//   自检点按目标策略 spec.weights 顺序.
// ============================================================================
struct Plan {
  int n = 0;
  int col[MINE_N_FACTORS];
  float w[MINE_N_FACTORS];
  float wsum = 0.0f;

  void push(int c, float weight) {
    assert(n < MINE_N_FACTORS);
    col[n] = c;
    w[n] = weight;
    ++n;
  }
  void seal() {
    for (int q = 0; q < n; ++q)
      wsum += std::fabs(w[q]);
    assert(n > 0 && wsum > 0.0f);
  }
};

Plan plan_from_k(const std::int8_t *k) {
  Plan p;
  const float m = static_cast<float>(MINE_LATTICE_M);
  for (int f = 0; f < MINE_N_FACTORS; ++f) {
    if (k[f] == 0)
      continue;
    p.push(f, static_cast<float>(k[f]) / m);
  }
  p.seal();
  return p;
}

struct Metrics {
  float v[MINE_N_METRICS];
};

static_assert(sizeof(Metrics) == sizeof(float) * MINE_N_METRICS,
              "Metrics 必须是紧凑 float 数组 (直接 reinterpret 落 npy)");

struct Scratch {
  std::vector<float> acc;                 // [max_cnt]
  std::vector<std::pair<float, int>> sel; // [prefix_need + TIE_MARGIN]
  std::vector<double> turn_sum;           // [BATCH]
  std::vector<float> navs;                // [BATCH × n_d]
  std::vector<Plan> plans;                // [BATCH]
};

// ============================================================================
// 一批权重跑满窗口: 外层日 / 内层权重.
//   navs[b·n_d + i] = 第 b 个权重第 i 日的 NAV (自检要逐点对账);
//   out[b] = 指标 (列序 = MINE_METRIC_NAMES).
// ============================================================================
void run_batch(const PrStore &pr, int k_need, std::span<const Plan> plans,
               std::vector<Engine> &engs, Scratch &sc, std::span<Metrics> out) {
  const int np = static_cast<int>(plans.size());
  const int n_d = pr.n_d;
  assert(static_cast<int>(engs.size()) >= np && out.size() >= static_cast<std::size_t>(np));
  NullRecorder rec;

  for (int b = 0; b < np; ++b) {
    engs[static_cast<std::size_t>(b)].reset();
    sc.turn_sum[static_cast<std::size_t>(b)] = 0.0;
  }

  for (int i = 0; i < n_d; ++i) {
    const int cnt = pr.cnt(i);
    const float *blk = pr.block(i);
    const std::int32_t *mem = pr.members(i);
    const int n_keep = std::min(cnt, k_need);
    const int n_sel = std::min(cnt, k_need + TIE_MARGIN);
    float *acc = sc.acc.data();
    std::pair<float, int> *sel = sc.sel.data();

    for (int b = 0; b < np; ++b) {
      const Plan &pl = plans[static_cast<std::size_t>(b)];

      // (1) score 分子: Σ w · pct_rank_pool(f), 与 cs_score 同序累加.
      std::fill(acc, acc + cnt, 0.0f);
      for (int q = 0; q < pl.n; ++q) {
        const float *cv = blk + static_cast<std::int64_t>(pl.col[q]) * cnt;
        const float w = pl.w[q];
        for (int j = 0; j < cnt; ++j)
          acc[j] += w * cv[j];
      }

      // (2) 取前 n_sel 名 (按未归一 acc; 定长插入选择 — 绝大多数候选一次比较
      //   即被拒, 比全排序快一个量级).
      int ns = 0;
      for (int j = 0; j < cnt; ++j) {
        float s = acc[j];
        if (ns == n_sel && !(s > sel[n_sel - 1].first))
          continue;
        int p = (ns < n_sel) ? ns : n_sel - 1;
        if (ns < n_sel)
          ++ns;
        while (p > 0 && sel[p - 1].first < s) {
          sel[p] = sel[p - 1];
          --p;
        }
        sel[p] = {s, mem[j]};
      }

      // (3) 归一到 cs_score 的 score 口径, 再按 (score 降序, a 升序) 定序 —
      //   与 cs_rank 逐位同序. ÷wsum 保序但不保严格序 (不同 acc 可映射到同一
      //   float32), 故并列破序必须在归一之后做, TIE_MARGIN 提供余量.
      for (int j = 0; j < ns; ++j)
        sel[j].first /= pl.wsum;
      std::sort(sel, sel + ns,
                [](const std::pair<float, int> &x, const std::pair<float, int> &y) {
                  if (x.first != y.first)
                    return x.first > y.first;
                  return x.second < y.second;
                });
      // 边界并列一路顶到余量末尾 ⇒ 被挤掉的同分标的可能才是 top-K 里的那个.
      assert((ns < n_sel || n_sel == n_keep ||
              sel[n_keep - 1].first > sel[n_sel - 1].first) &&
             "top-K 边界并列跨越 TIE_MARGIN — 加大 TIE_MARGIN");

      // (4) 内核走一天 (与 backtest::run 同一份实现, 只是 Recorder 为空).
      Engine &e = engs[static_cast<std::size_t>(b)];
      e.step(i,
             std::span<const std::pair<float, int>>(sel, static_cast<std::size_t>(n_keep)),
             rec);
      sc.navs[static_cast<std::size_t>(b) * static_cast<std::size_t>(n_d) +
              static_cast<std::size_t>(i)] = static_cast<float>(e.pv_end);
      sc.turn_sum[static_cast<std::size_t>(b)] += e.turn_amt / (2.0 * e.pv);
    }
  }

  for (int b = 0; b < np; ++b) {
    std::span<const float> nav(sc.navs.data() + static_cast<std::size_t>(b) *
                                                    static_cast<std::size_t>(n_d),
                               static_cast<std::size_t>(n_d));
    report::NavStats s = report::nav_stats(nav);
    Metrics &m = out[static_cast<std::size_t>(b)];
    m.v[0] = s.ann_return;
    m.v[1] = s.sharpe;
    m.v[2] = s.ann_vol;
    m.v[3] = s.max_drawdown;
    m.v[4] = nav[static_cast<std::size_t>(n_d) - 1] / nav[0];
    m.v[5] = static_cast<float>(sc.turn_sum[static_cast<std::size_t>(b)] /
                                static_cast<double>(n_d) *
                                static_cast<double>(report::TRADING_DAYS));
    m.v[6] = static_cast<float>(s.longest_no_new_high);
  }
}

Scratch make_scratch(const PrStore &pr, int k_need) {
  Scratch sc;
  sc.acc.assign(static_cast<std::size_t>(pr.max_cnt), 0.0f);
  sc.sel.assign(static_cast<std::size_t>(k_need + TIE_MARGIN), {0.0f, -1});
  sc.turn_sum.assign(BATCH, 0.0);
  sc.navs.assign(static_cast<std::size_t>(BATCH) * static_cast<std::size_t>(pr.n_d),
                 0.0f);
  sc.plans.assign(BATCH, Plan{});
  return sc;
}

std::vector<Engine> make_engines(const MarketWindow &mk,
                                 const strategy::StrategySpec &spec) {
  std::vector<Engine> engs;
  engs.reserve(BATCH);
  for (int b = 0; b < BATCH; ++b)
    engs.emplace_back(mk, spec.hold_n, spec.exit_ratio);
  return engs;
}

// ---- lattice ---------------------------------------------------------------

// 点数闭式: Σ_j C(n, j) · C(M−1, j−1) · 2^j  (j = 非零权重个数)
std::int64_t lattice_size() {
  auto binom = [](int a, int b) -> std::int64_t {
    if (b < 0 || b > a)
      return 0;
    std::int64_t r = 1;
    for (int i = 0; i < b; ++i)
      r = r * (a - i) / (i + 1);
    return r;
  };
  std::int64_t p = 0;
  for (int j = 1; j <= std::min(MINE_N_FACTORS, MINE_LATTICE_M); ++j)
    p += binom(MINE_N_FACTORS, j) * binom(MINE_LATTICE_M - 1, j - 1) *
         (static_cast<std::int64_t>(1) << j);
  return p;
}

// 全枚举 Σ|k| = M 的整数点, 行主序物化成 [P, n] 的 int8 表 — 之后并行调度与
//   落盘都只是按下标取块.
std::vector<std::int8_t> build_lattice() {
  misc::Timer t("[mine] lattice 枚举");
  std::int64_t p_expect = lattice_size();
  std::vector<std::int8_t> out;
  out.reserve(static_cast<std::size_t>(p_expect) *
              static_cast<std::size_t>(MINE_N_FACTORS));

  std::array<std::int8_t, MINE_N_FACTORS> k{};
  auto emit = [&]() { out.insert(out.end(), k.begin(), k.end()); };
  auto rec = [&](auto &&self, int i, int rem) -> void {
    if (i == MINE_N_FACTORS - 1) {
      // 末位必须把剩余预算一次用完 (rem = 0 时只能取 0).
      if (rem == 0) {
        k[static_cast<std::size_t>(i)] = 0;
        emit();
      } else {
        k[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(rem);
        emit();
        k[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(-rem);
        emit();
      }
      k[static_cast<std::size_t>(i)] = 0;
      return;
    }
    for (int v = -rem; v <= rem; ++v) {
      k[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(v);
      self(self, i + 1, rem - std::abs(v));
    }
    k[static_cast<std::size_t>(i)] = 0;
  };
  rec(rec, 0, MINE_LATTICE_M);

  assert(static_cast<std::int64_t>(out.size()) ==
             p_expect * MINE_N_FACTORS &&
         "lattice 枚举数与闭式不符");
  return out;
}

} // namespace

double run(const feature::Axes &axes, const feature::Tensor &T,
           std::span<const backtest::Result> results) {
  auto t0 = std::chrono::high_resolution_clock::now();

  // ---- 目标策略 (pool / filters / hold_n / exit_ratio / 窗口全部继承) ----
  int s_idx = -1;
  for (int s = 0; s < strategy::N_STRATEGIES; ++s) {
    if (strategy::STRATEGIES[static_cast<std::size_t>(s)]->name == MINE_STRATEGY)
      s_idx = s;
  }
  assert(s_idx >= 0 && "MINE_STRATEGY 不在 strategy::STRATEGIES[] 里");
  const strategy::StrategySpec &spec =
      *strategy::STRATEGIES[static_cast<std::size_t>(s_idx)];

  const int d_lo = lower_d(axes, spec.bt_start_date);
  assert(d_lo >= 0 && d_lo < axes.n_d() && "bt_start_date 落在 D 轴之外");
  const int n_d = axes.n_d() - d_lo;
  assert(results.size() == static_cast<std::size_t>(strategy::N_STRATEGIES));
  const backtest::Result &ref = results[static_cast<std::size_t>(s_idx)];
  assert(ref.d_lo == d_lo &&
         static_cast<int>(ref.strategy_nav.size()) == n_d &&
         "mine 窗口与 backtest::run 不一致");

  const int k_need = prefix_need(spec);
  const std::int64_t n_pts = lattice_size();
  std::printf("\n[mine] 目标 %.*s | 窗口 %s..%s (%d 日) | hold_n=%d "
              "exit_ratio=%.2f\n[mine] 因子 %d 个, lattice M=%d ⇒ %lld 个权重方向\n",
              static_cast<int>(spec.name.size()), spec.name.data(),
              axes.dates[static_cast<std::size_t>(d_lo)].c_str(),
              axes.dates.back().c_str(), n_d, spec.hold_n,
              static_cast<double>(spec.exit_ratio), MINE_N_FACTORS,
              MINE_LATTICE_M, static_cast<long long>(n_pts));

  MarketWindow mk = backtest::load_market(axes, T, d_lo, n_d);
  PrStore pr = build_pr(axes, T, s_idx, d_lo, n_d);

  // ---- 自检: 用目标策略自己的 weights 走本管线, 与 backtest::run 对账 ----
  //   过不了说明 "PR 预计算 + top-K 截断 + 内核" 这条链与报告路径有分歧,
  //   此时挖出来的权重填回 cpp 也不会复现 ⇒ 直接 fail, 不留侥幸.
  float selfcheck_max_rel = 0.0f;
  {
    Plan p0;
    assert(spec.weights.size() <= static_cast<std::size_t>(MINE_N_FACTORS));
    for (const strategy::FactorWeight &fw : spec.weights) {
      int c = -1;
      for (int f = 0; f < MINE_N_FACTORS; ++f) {
        if (MINE_FACTORS[f] == fw.f)
          c = f;
      }
      assert(c >= 0 && "自检: 目标策略 weights 里的因子不在 MINE_FACTORS 中");
      p0.push(c, fw.w);
    }
    p0.seal();

    std::vector<Engine> engs = make_engines(mk, spec);
    Scratch sc = make_scratch(pr, k_need);
    Metrics m0{};
    run_batch(pr, k_need, std::span<const Plan>(&p0, 1), engs, sc,
              std::span<Metrics>(&m0, 1));

    double worst = 0.0;
    for (int i = 0; i < n_d; ++i) {
      double a = static_cast<double>(sc.navs[static_cast<std::size_t>(i)]);
      double b = static_cast<double>(ref.strategy_nav[static_cast<std::size_t>(i)]);
      assert(b > 0.0);
      worst = std::max(worst, std::fabs(a - b) / b);
    }
    selfcheck_max_rel = static_cast<float>(worst);
    std::printf("[mine] 自检: NAV 逐点最大相对偏差 %.3e | 年化 %.4f 夏普 %.3f\n",
                worst, static_cast<double>(m0.v[0]), static_cast<double>(m0.v[1]));
    assert(worst < 1e-9 && "自检未过: 挖掘管线与 backtest::run 口径不一致");
  }

  // ---- lattice 扫描 ----
  std::vector<std::int8_t> kgrid = build_lattice();
  const std::int64_t n_p = static_cast<std::int64_t>(kgrid.size()) / MINE_N_FACTORS;
  std::vector<Metrics> mets(static_cast<std::size_t>(n_p));

  unsigned n_threads = misc::Affinity::core_count();
  std::atomic<std::int64_t> next{0};
  std::atomic<std::int64_t> done{0};
  auto t_scan = std::chrono::high_resolution_clock::now();
  const std::int64_t step = n_p / 100 + 1;

  auto worker = [&]() {
    std::vector<Engine> engs = make_engines(mk, spec);
    Scratch sc = make_scratch(pr, k_need);
    for (;;) {
      std::int64_t lo = next.fetch_add(BATCH, std::memory_order_relaxed);
      if (lo >= n_p)
        break;
      int np = static_cast<int>(std::min<std::int64_t>(BATCH, n_p - lo));
      for (int b = 0; b < np; ++b) {
        sc.plans[static_cast<std::size_t>(b)] =
            plan_from_k(kgrid.data() + (lo + b) * MINE_N_FACTORS);
      }
      run_batch(pr, k_need,
                std::span<const Plan>(sc.plans.data(), static_cast<std::size_t>(np)),
                engs, sc,
                std::span<Metrics>(mets.data() + lo, static_cast<std::size_t>(np)));

      std::int64_t prev = done.fetch_add(np, std::memory_order_relaxed);
      std::int64_t cur = prev + np;
      if (prev / step != cur / step) {
        std::chrono::duration<double> el =
            std::chrono::high_resolution_clock::now() - t_scan;
        double rate = static_cast<double>(cur) / el.count();
        std::printf("[mine] %5.1f%%  %.0f 权重/s  已用 %.0fs  剩余 ~%.0fs\n",
                    100.0 * static_cast<double>(cur) / static_cast<double>(n_p),
                    rate, el.count(),
                    static_cast<double>(n_p - cur) / rate);
        std::fflush(stdout);
      }
    }
  };
  {
    std::vector<std::thread> ths;
    ths.reserve(n_threads);
    for (unsigned k = 0; k < n_threads; ++k)
      ths.emplace_back(worker);
    for (auto &th : ths)
      th.join();
  }

  // ---- 落盘 (py/app/mine.py 直读) ----
  fs::path out = misc::git_root() / "output" / "mine";
  fs::create_directories(out);
  {
    std::size_t shape[2] = {static_cast<std::size_t>(n_p),
                            static_cast<std::size_t>(MINE_N_FACTORS)};
    misc::write_npy_i1(out / "k_grid.npy",
                       std::span<const std::int8_t>(kgrid.data(), kgrid.size()),
                       std::span<const std::size_t>(shape, 2));
  }
  {
    std::size_t shape[2] = {static_cast<std::size_t>(n_p),
                            static_cast<std::size_t>(MINE_N_METRICS)};
    misc::write_npy_f4(
        out / "metrics.npy",
        std::span<const float>(reinterpret_cast<const float *>(mets.data()),
                               static_cast<std::size_t>(n_p) *
                                   static_cast<std::size_t>(MINE_N_METRICS)),
        std::span<const std::size_t>(shape, 2));
  }
  {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    report::add_str(doc, root, "strategy", spec.name);
    yyjson_mut_obj_add_int(doc, root, "lattice_m", MINE_LATTICE_M);
    yyjson_mut_obj_add_int(doc, root, "n_points", n_p);

    std::vector<std::string> fnames, fcn;
    fnames.reserve(static_cast<std::size_t>(MINE_N_FACTORS));
    fcn.reserve(static_cast<std::size_t>(MINE_N_FACTORS));
    for (int f = 0; f < MINE_N_FACTORS; ++f) {
      fnames.emplace_back(MINE_FACTORS[f]->name);
      fcn.emplace_back(MINE_FACTORS[f]->cn_name);
    }
    report::add_str_arr(doc, root, "factor_names", fnames);
    report::add_str_arr(doc, root, "factor_cn_names", fcn);
    report::add_sv_arr(doc, root, "metric_names",
                       std::span<const std::string_view>(MINE_METRIC_NAMES,
                                                         MINE_N_METRICS));

    yyjson_mut_val *win = report::add_obj(doc, root, "window");
    report::add_str(doc, win, "start", axes.dates[static_cast<std::size_t>(d_lo)]);
    report::add_str(doc, win, "end", axes.dates.back());
    yyjson_mut_obj_add_int(doc, win, "n_days", n_d);

    // 目标策略当前 weights (py 侧要在结果里标出"基线在哪儿")
    yyjson_mut_val *base = report::add_obj(doc, root, "baseline_weights");
    for (const strategy::FactorWeight &fw : spec.weights)
      yyjson_mut_obj_add_real(doc, base, fw.f->name, static_cast<double>(fw.w));

    yyjson_mut_val *chk = report::add_obj(doc, root, "selfcheck");
    report::add_f4(doc, chk, "nav_max_rel_diff", selfcheck_max_rel);

    misc::atomic_write_json(out / "meta.json", doc);
    yyjson_mut_doc_free(doc);
  }

  std::chrono::duration<double> dur =
      std::chrono::high_resolution_clock::now() - t0;
  return dur.count();
}

} // namespace mine
