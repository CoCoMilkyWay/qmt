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
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
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

// ---- 滑窗分层参数 (写死 — 口径固定才可跨次/跨策略比较) ----------------------
constexpr int LAYER_WINDOW = 252; // 窗口长 (交易日) ≈ 一年
constexpr int LAYER_STEP = 21;    // 步长 (交易日) ≈ 一月
static_assert(LAYER_WINDOW % LAYER_STEP == 0);
constexpr int LAYER_RING = LAYER_WINDOW / LAYER_STEP + 1; // NAV 快照环槽数
constexpr double LAYER_K_SE = 2.0;                        // 梳子分 S = b − k·se 的 k
// score 直方图桶数 — 分箱免排序的关键: 档序对 score 的正仿射变换不变 ⇒
//   min-max 归一后落桶, 按累计数切等份档, 跨界桶按比例劈开 (并列大块均摊到
//   跨过的档 — 并列本就不可排序, 均摊即正确语义). O(cnt), 确定性.
constexpr int N_BUCKETS = 1024;
static_assert(N_BUCKETS % 64 == 0, "触碰桶位图按 64 位字切分");

// 直方图单元 — 计数与收益和挤进同一 16B: 一次散射只碰一条 cache line
//   (拆两个数组要碰两条), 整表 16 KB 常驻 L1.
struct alignas(16) HistCell {
  double sum;
  std::int64_t cnt;
};
static_assert(sizeof(HistCell) == 16);

// 去重线: 平均逐日持仓重合度 ≥ 此值 ⇒ 同一风格 (自解释, 非调参 — "平均而言
//   半数持仓相同"). 零假设线不可用: 400 池独立选 10 只的期望重合 ≈ 0.25 只,
//   所有真实候选都远高于它, 按零假设筛会一个不留.
constexpr double DEDUP_OVERLAP = 0.5;

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
//   另备一份 Phase 1 (分层) 专用的 valid 紧凑视图 (v* 系列): 只含 "fret
//   finite" 的成员 (顺序保持) — 分层只用这些成员, 提前紧凑一次, 全格扫描的
//   内层循环就没有任何 finite 分支, 打分 / min-max / 分桶全部可向量化.
// ============================================================================
struct PrStore {
  int n_d = 0;
  int max_cnt = 0;
  std::vector<std::int64_t> off; // [n_d + 1]
  std::vector<std::int32_t> memb;
  std::vector<float> pr;
  // 成员对齐的次日收益 close[i+1]/close[i] − 1 (close = MarketWindow 的
  //   last_close 口径, 与内核 mark-to-market 同一份数据). NaN = 当日停牌 /
  //   无价 / 末日无次日 ⇒ 分层时该成员当日不参与.
  std::vector<float> fret;
  // valid 紧凑视图 (fret finite 的成员, 原序; 末日 v 成员数为 0, 分层不用它).
  std::vector<std::int64_t> voff; // [n_d + 1]
  std::vector<float> vpr;         // 因子主序, 只含 valid 成员
  std::vector<float> vfret;       // 紧凑, 全 finite

  int cnt(int i) const { return static_cast<int>(off[i + 1] - off[i]); }
  const std::int32_t *members(int i) const { return memb.data() + off[i]; }
  const float *block(int i) const {
    return pr.data() + off[i] * static_cast<std::int64_t>(MINE_N_FACTORS);
  }
  const float *fret_row(int i) const { return fret.data() + off[i]; }

  int vcnt(int i) const { return static_cast<int>(voff[i + 1] - voff[i]); }
  const float *vblock(int i) const {
    return vpr.data() + voff[i] * static_cast<std::int64_t>(MINE_N_FACTORS);
  }
  const float *vfret_row(int i) const { return vfret.data() + voff[i]; }
};

PrStore build_pr(const feature::Axes &axes, const feature::Tensor &T,
                 const MarketWindow &mk, int s_idx, int d_lo, int n_d) {
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
  st.fret.resize(static_cast<std::size_t>(total));

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
        // 成员次日收益 (分层用).
        float *fr = st.fret.data() + st.off[static_cast<std::size_t>(i)];
        if (i + 1 < n_d) {
          const float *c0 = mk.close_row(i);
          const float *c1 = mk.close_row(i + 1);
          const std::uint8_t *fl = mk.flag_row(i);
          for (int q = 0; q < cnt; ++q) {
            int a = mem[q];
            float v = std::nanf("");
            if ((fl[a] & MarketWindow::SUSP) == 0 &&
                feature::is_finite(c0[a]) && feature::is_finite(c1[a])) {
              assert(c0[a] > 0.0f);
              v = c1[a] / c0[a] - 1.0f;
            }
            fr[q] = v;
          }
        } else {
          for (int q = 0; q < cnt; ++q)
            fr[q] = std::nanf("");
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

  // pass 3: Phase 1 专用 valid 紧凑视图. 全格扫描每 (权重, 日) 都要过一遍
  //   成员数组, 这里一次性把 finite 判断从内层循环里买断.
  st.voff.assign(static_cast<std::size_t>(n_d) + 1, 0);
  for (int i = 0; i < n_d; ++i) {
    const float *fr = st.fret_row(i);
    const int cnt = st.cnt(i);
    std::int64_t c = 0;
    for (int q = 0; q < cnt; ++q) {
      if (feature::is_finite(fr[q]))
        ++c;
    }
    st.voff[static_cast<std::size_t>(i) + 1] =
        st.voff[static_cast<std::size_t>(i)] + c;
  }
  st.vpr.resize(static_cast<std::size_t>(st.voff[static_cast<std::size_t>(n_d)]) *
                static_cast<std::size_t>(MINE_N_FACTORS));
  st.vfret.resize(
      static_cast<std::size_t>(st.voff[static_cast<std::size_t>(n_d)]));
  {
    std::atomic<int> next{0};
    auto worker = [&]() {
      std::vector<std::int32_t> idx(static_cast<std::size_t>(st.max_cnt));
      for (;;) {
        int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= n_d)
          break;
        const int cnt = st.cnt(i);
        const float *fr = st.fret_row(i);
        int nv = 0;
        for (int q = 0; q < cnt; ++q) {
          if (feature::is_finite(fr[q]))
            idx[static_cast<std::size_t>(nv++)] = q;
        }
        assert(nv == st.vcnt(i) && "valid 两遍扫描不一致");
        const float *blk = st.block(i);
        float *vb = st.vpr.data() + st.voff[static_cast<std::size_t>(i)] *
                                        static_cast<std::int64_t>(MINE_N_FACTORS);
        for (int f = 0; f < MINE_N_FACTORS; ++f) {
          const float *src = blk + static_cast<std::int64_t>(f) * cnt;
          float *dst = vb + static_cast<std::int64_t>(f) * nv;
          for (int j = 0; j < nv; ++j)
            dst[j] = src[idx[static_cast<std::size_t>(j)]];
        }
        float *vf = st.vfret.data() + st.voff[static_cast<std::size_t>(i)];
        for (int j = 0; j < nv; ++j)
          vf[j] = fr[idx[static_cast<std::size_t>(j)]];
      }
    };
    std::vector<std::thread> ths;
    ths.reserve(n_threads);
    for (unsigned k = 0; k < n_threads; ++k)
      ths.emplace_back(worker);
    for (auto &th : ths)
      th.join();
  }

  std::printf("[mine] 池: %lld 个 (日, 标的) 格 (有效 %lld), 日均 %.0f, "
              "峰值 %d; 分位矩阵 %.0f MB (+valid 视图 %.0f MB)\n",
              static_cast<long long>(total),
              static_cast<long long>(st.voff[static_cast<std::size_t>(n_d)]),
              static_cast<double>(total) / static_cast<double>(n_d), st.max_cnt,
              static_cast<double>(st.pr.size() * sizeof(float)) / 1048576.0,
              static_cast<double>(st.vpr.size() * sizeof(float)) / 1048576.0);
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
  float v[MINE_N_BT_METRICS];
};

static_assert(sizeof(Metrics) == sizeof(float) * MINE_N_BT_METRICS,
              "Metrics 必须是紧凑 float 数组 (直接 reinterpret 落 npy)");

struct Scratch {
  std::vector<float> acc;                 // [max_cnt]
  std::vector<std::pair<float, int>> sel; // [prefix_need + TIE_MARGIN]
  std::vector<double> turn_sum;           // [BATCH]
  std::vector<float> navs;                // [BATCH × n_d]
  std::vector<Plan> plans;                // [BATCH]
};

// ============================================================================
// 某日某权重的候选前 n_keep 名 — 选股口径的**唯一实现** (回测路径与去重路径
//   共用, 保证"去重看的持仓"就是"回测会买的持仓").
//   与 strategy/columns.cpp 逐位同构: score = Σ w·pct_rank_pool(f) / Σ|w|
//   (累加序 = Plan 序), 定序 = (score 降序, 并列 a 升序).
//   写 sel[0 .. n_keep), 需 sel 容量 ≥ n_keep + TIE_MARGIN.
// ============================================================================
void select_top(const float *blk, const std::int32_t *mem, int cnt,
                const Plan &pl, int n_keep, float *acc,
                std::pair<float, int> *sel) {
  assert(n_keep <= cnt);
  const int n_sel = std::min(cnt, n_keep + TIE_MARGIN);

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
}

// ============================================================================
// 一批权重跑满窗口: 外层日 / 内层权重.
//   navs[b·n_d + i] = 第 b 个权重第 i 日的 NAV (自检要逐点对账);
//   out[b] = 指标 (列序 = MINE_BT_METRIC_NAMES).
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
    float *acc = sc.acc.data();
    std::pair<float, int> *sel = sc.sel.data();

    for (int b = 0; b < np; ++b) {
      select_top(blk, mem, cnt, plans[static_cast<std::size_t>(b)], n_keep, acc,
                 sel);

      // 内核走一天 (与 backtest::run 同一份实现, 只是 Recorder 为空).
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

// ============================================================================
// Phase 1: 滑窗收益分层 — 梳子分 (语义见 mine/mine.hpp 头注释).
//   每日: score 分箱 → n_bins 档等权次日收益 → 档 NAV 复利;
//   每 LAYER_STEP 日快照档 NAV (环形缓存), 距今 LAYER_WINDOW 的快照对儿即
//   一个滑窗 → OLS(档 log 收益 ~ 档秩) → S_w = b − k·se.
//   不跑内核、不需要 top-K/tie 处理; score 不 ÷Σ|w| (分箱对正仿射不变).
// ============================================================================
struct LayerParams {
  int n_bins = 0; // universe_size / hold_n — 档宽 ≈ hold_n 只
  int n_ret = 0;  // 收益日数 = n_d − 1
  int n_win = 0;
  std::vector<double> xc; // 居中档秩: (j + 0.5) / n_bins − 0.5, 升 score 序
  double sxx = 0.0;       // Σ xc²
};

LayerParams make_layer(const strategy::StrategySpec &spec, int n_d) {
  LayerParams lp;
  lp.n_bins = spec.pool.universe_size / spec.hold_n;
  assert(lp.n_bins >= 10 && "档数不足 10 — universe_size / hold_n 太小");
  lp.n_ret = n_d - 1;
  assert(lp.n_ret >= LAYER_WINDOW && "回测期不足一个滑窗");
  lp.n_win = lp.n_ret / LAYER_STEP - LAYER_WINDOW / LAYER_STEP + 1;
  assert(lp.n_win >= 2);
  lp.xc.resize(static_cast<std::size_t>(lp.n_bins));
  for (int j = 0; j < lp.n_bins; ++j) {
    double x = (static_cast<double>(j) + 0.5) / static_cast<double>(lp.n_bins) - 0.5;
    lp.xc[static_cast<std::size_t>(j)] = x;
    lp.sxx += x * x;
  }
  return lp;
}

struct WinStat {
  float s, b, r2, sharpe;
};

// 顶档窗口夏普 — 与 report::nav_stats 同口径: 简单日收益, population std,
//   rf = 0, ×√252. 由 Σr / Σr² 的窗口差 O(1) 算出 (double 下抵消可忽略:
//   日收益 ~1e-2 ⇒ Σr² 全期 ~0.2, 窗口差 ~0.03, 相对抵消仅一个数量级).
float win_sharpe(double sr0, double sr20, double sr1, double sr21) {
  const double n = static_cast<double>(LAYER_WINDOW);
  double mean = (sr1 - sr0) / n;
  double var = (sr21 - sr20) / n - mean * mean;
  if (!(var > 0.0))
    return 0.0f;
  return static_cast<float>(mean / std::sqrt(var) *
                            std::sqrt(static_cast<double>(report::TRADING_DAYS)));
}

// OLS: y_j = log(nav1[j] / nav0[j]) 对 xc 回归. 窗口恰 252 日 ⇒ y 即年化 log.
WinStat regress(const double *nav0, const double *nav1, const LayerParams &lp) {
  const int n = lp.n_bins;
  double sy = 0.0, sxy = 0.0, syy = 0.0;
  for (int j = 0; j < n; ++j) {
    double y = std::log(nav1[j] / nav0[j]);
    sy += y;
    sxy += lp.xc[static_cast<std::size_t>(j)] * y;
    syy += y * y;
  }
  double ybar = sy / static_cast<double>(n);
  double syy_c = syy - static_cast<double>(n) * ybar * ybar;
  double b = sxy / lp.sxx;
  if (syy_c <= 0.0) // 全档同收益 (退化: score 全并列 ⇒ 均摊)
    return WinStat{0.0f, 0.0f, 0.0f, 0.0f};
  double ess = b * sxy;
  double rss = std::max(0.0, syy_c - ess);
  double r2 = std::min(1.0, ess / syy_c);
  double se = std::sqrt(rss / static_cast<double>(n - 2) / lp.sxx);
  return WinStat{static_cast<float>(b - LAYER_K_SE * se), static_cast<float>(b),
                 static_cast<float>(r2), 0.0f};
}

struct LayerScratch {
  int np_cap = 0;
  std::vector<float> acc;                            // [max_cnt]
  std::vector<HistCell> hist;                        // [N_BUCKETS] — sweep 时只清触碰项
  std::array<std::uint64_t, N_BUCKETS / 64> hmask{}; // 触碰桶位图
  std::vector<std::int32_t> hbuf;                    // [max_cnt] 每成员桶号 (分桶 pass 向量化)
  std::vector<double> bin_ret;                       // [n_bins]
  std::vector<double> nav;                           // [np_cap][n_bins]
  std::vector<double> ring;                          // [LAYER_RING][np_cap][n_bins] NAV 快照
  std::vector<double> top_sum;                       // [np_cap][2] 顶档 Σr / Σr² (running)
  std::vector<double> top_ring;                      // [LAYER_RING][np_cap][2] 同上快照
  std::vector<float> sw, bw, r2w, shw;               // [np_cap][n_win]
  std::vector<float> tmp;                            // [n_win] (p10 用)
  std::vector<Plan> plans;                           // [np_cap]
};

LayerScratch make_layer_scratch(const PrStore &pr, const LayerParams &lp,
                                int np_cap) {
  LayerScratch sc;
  sc.np_cap = np_cap;
  std::size_t nb = static_cast<std::size_t>(lp.n_bins);
  std::size_t cap = static_cast<std::size_t>(np_cap);
  std::size_t nw = static_cast<std::size_t>(lp.n_win);
  sc.acc.assign(static_cast<std::size_t>(pr.max_cnt), 0.0f);
  sc.hist.assign(N_BUCKETS, HistCell{0.0, 0});
  sc.hmask.fill(0);
  sc.hbuf.assign(static_cast<std::size_t>(pr.max_cnt), 0);
  sc.bin_ret.assign(nb, 0.0);
  sc.nav.assign(cap * nb, 1.0);
  sc.ring.assign(static_cast<std::size_t>(LAYER_RING) * cap * nb, 1.0);
  sc.top_sum.assign(cap * 2, 0.0);
  sc.top_ring.assign(static_cast<std::size_t>(LAYER_RING) * cap * 2, 0.0);
  sc.sw.assign(cap * nw, 0.0f);
  sc.bw.assign(cap * nw, 0.0f);
  sc.r2w.assign(cap * nw, 0.0f);
  sc.shw.assign(cap * nw, 0.0f);
  sc.tmp.assign(nw, 0.0f);
  sc.plans.assign(cap, Plan{});
  return sc;
}

// 一批权重的分层 + 顶档夏普全流程 (一遍日循环).
//   out_sum [np][10] 摘要 (列序 = MINE_POINT_METRIC_NAMES 前 10 项);
//   out_win 可空, 否则 [np][n_win][MINE_N_WINDOW_METRICS] = (S, b, R², 夏普).
void run_layer_batch(const PrStore &pr, const LayerParams &lp,
                     std::span<const Plan> plans, LayerScratch &sc,
                     float *out_sum, float *out_win) {
  const int np = static_cast<int>(plans.size());
  const int nb = lp.n_bins;
  const int nw = lp.n_win;
  assert(np <= sc.np_cap);
  const std::size_t nav_stride = static_cast<std::size_t>(nb);
  const std::size_t ring_stride =
      static_cast<std::size_t>(sc.np_cap) * nav_stride;
  const std::size_t top_stride = static_cast<std::size_t>(sc.np_cap) * 2;

  std::fill(sc.nav.begin(),
            sc.nav.begin() + static_cast<std::ptrdiff_t>(np) *
                                 static_cast<std::ptrdiff_t>(nb),
            1.0);
  std::fill(sc.top_sum.begin(),
            sc.top_sum.begin() + static_cast<std::ptrdiff_t>(np) * 2, 0.0);
  // 快照 m=0: NAV 全 1, Σr / Σr² 全 0.
  std::fill(sc.ring.begin(),
            sc.ring.begin() + static_cast<std::ptrdiff_t>(ring_stride), 1.0);
  std::fill(sc.top_ring.begin(),
            sc.top_ring.begin() + static_cast<std::ptrdiff_t>(top_stride), 0.0);

  for (int t = 0; t < lp.n_ret; ++t) {
    // valid 紧凑视图: 只含有次日收益的成员 ⇒ 内层零 finite 分支.
    const int cnt = pr.vcnt(t);
    const float *blk = pr.vblock(t);
    const float *fr = pr.vfret_row(t);
    const double per = static_cast<double>(cnt) / static_cast<double>(nb);
    float *acc = sc.acc.data();
    std::int32_t *hb = sc.hbuf.data();
    HistCell *hist = sc.hist.data();
    std::uint64_t *hmask = sc.hmask.data();

    for (int b = 0; b < np; ++b) {
      const Plan &pl = plans[static_cast<std::size_t>(b)];

      // (1) score 分子 (不 ÷Σ|w| — 分箱对正仿射不变). 首因子直写省一遍
      //   清零 pass (0 + w·x ≡ w·x), 后续累加序仍 = Plan 序.
      {
        const float *cv = blk + static_cast<std::int64_t>(pl.col[0]) * cnt;
        const float w0 = pl.w[0];
        for (int j = 0; j < cnt; ++j)
          acc[j] = w0 * cv[j];
      }
      for (int q = 1; q < pl.n; ++q) {
        const float *cv = blk + static_cast<std::int64_t>(pl.col[q]) * cnt;
        const float w = pl.w[q];
        for (int j = 0; j < cnt; ++j)
          acc[j] += w * cv[j];
      }

      // (2) score min-max — 无分支, 可向量化.
      float mn = acc[0], mx = acc[0];
      for (int j = 1; j < cnt; ++j) {
        mn = std::min(mn, acc[j]);
        mx = std::max(mx, acc[j]);
      }
      const float inv =
          (mx > mn) ? static_cast<float>(N_BUCKETS) / (mx - mn) : 0.0f;

      // (3) 分桶号 — 独立 pass, min 钳位代替越界分支, 可向量化.
      for (int j = 0; j < cnt; ++j) {
        int h = static_cast<int>((acc[j] - mn) * inv);
        hb[j] = std::min(h, N_BUCKETS - 1);
      }

      // (4) 散射: 桶计数 + 桶内收益和 (16B 单元一条 cache line), 触碰桶记位图.
      for (int j = 0; j < cnt; ++j) {
        const int h = hb[j];
        hist[h].sum += static_cast<double>(fr[j]);
        hist[h].cnt += 1;
        hmask[h >> 6] |= std::uint64_t{1} << (h & 63);
      }

      // (5) 位图扫桶 → 等份档 (升 score 序, 与全量扫描同序). 整桶落档直加
      //   桶和 (零除法); 只有跨界桶才算均值按比例劈开 (并列本就不可排序,
      //   均摊即正确语义), 每权重日 ≤ n_bins−1 次. 顺手清零触碰项与位图.
      int bin = 0;
      double need = per, accr = 0.0;
      for (int wq = 0; wq < N_BUCKETS / 64; ++wq) {
        std::uint64_t mword = hmask[wq];
        if (mword == 0)
          continue;
        hmask[wq] = 0;
        do {
          const int h = wq * 64 + std::countr_zero(mword);
          mword &= mword - 1;
          HistCell &cell = hist[h];
          const double sum = cell.sum;
          const double cd = static_cast<double>(cell.cnt);
          cell.sum = 0.0;
          cell.cnt = 0;
          if (bin == nb - 1) { // 末档吸收全部剩余 (含 fp 尾差)
            accr += sum;
            continue;
          }
          if (cd <= need) { // 整桶落在当前档内
            accr += sum;
            need -= cd;
            if (need <= 1e-9) {
              sc.bin_ret[static_cast<std::size_t>(bin)] = accr / per;
              ++bin;
              need = per;
              accr = 0.0;
            }
            continue;
          }
          const double mean_r = sum / cd; // 跨界桶
          double rem = cd;
          while (rem > 1e-12) {
            if (bin == nb - 1) {
              accr += rem * mean_r;
              break;
            }
            const double take = std::min(rem, need);
            accr += take * mean_r;
            rem -= take;
            need -= take;
            if (need <= 1e-9) {
              sc.bin_ret[static_cast<std::size_t>(bin)] = accr / per;
              ++bin;
              need = per;
              accr = 0.0;
            }
          }
        } while (mword != 0);
      }
      assert(bin == nb - 1 && "分箱质量不守恒");
      sc.bin_ret[static_cast<std::size_t>(bin)] = accr / per;

      // (6) 档 NAV 复利 + 顶档 Σr / Σr² 累加 (顶档 = top-hold_n 只 = 策略实际
      //   持仓的等权无成本版本; 夏普就建在它上面).
      double *nv = sc.nav.data() + static_cast<std::size_t>(b) * nav_stride;
      for (int j = 0; j < nb; ++j) {
        nv[j] *= 1.0 + sc.bin_ret[static_cast<std::size_t>(j)];
        assert(nv[j] > 0.0);
      }
      double r_top = sc.bin_ret[static_cast<std::size_t>(nb - 1)];
      sc.top_sum[static_cast<std::size_t>(b) * 2] += r_top;
      sc.top_sum[static_cast<std::size_t>(b) * 2 + 1] += r_top * r_top;
    }

    // (7) 月末快照 + 到期窗口回归 / 夏普.
    if ((t + 1) % LAYER_STEP == 0) {
      const int m = (t + 1) / LAYER_STEP;
      const int slot = m % LAYER_RING;
      for (int b = 0; b < np; ++b) {
        const double *nv = sc.nav.data() + static_cast<std::size_t>(b) * nav_stride;
        double *snap = sc.ring.data() + static_cast<std::size_t>(slot) * ring_stride +
                       static_cast<std::size_t>(b) * nav_stride;
        std::copy(nv, nv + nb, snap);
        const double *ts = sc.top_sum.data() + static_cast<std::size_t>(b) * 2;
        double *tsnap = sc.top_ring.data() +
                        static_cast<std::size_t>(slot) * top_stride +
                        static_cast<std::size_t>(b) * 2;
        tsnap[0] = ts[0];
        tsnap[1] = ts[1];
        const int w = m - LAYER_WINDOW / LAYER_STEP;
        if (w >= 0 && w < nw) {
          const std::size_t slot0 = static_cast<std::size_t>(w % LAYER_RING);
          const double *nav0 = sc.ring.data() + slot0 * ring_stride +
                               static_cast<std::size_t>(b) * nav_stride;
          const double *ts0 = sc.top_ring.data() + slot0 * top_stride +
                              static_cast<std::size_t>(b) * 2;
          WinStat st = regress(nav0, nv, lp);
          std::size_t o = static_cast<std::size_t>(b) * static_cast<std::size_t>(nw) +
                          static_cast<std::size_t>(w);
          sc.sw[o] = st.s;
          sc.bw[o] = st.b;
          sc.r2w[o] = st.r2;
          sc.shw[o] = win_sharpe(ts0[0], ts0[1], ts[0], ts[1]);
        }
      }
    }
  }

  // (8) 跨窗口摘要 (+ 可选 per-window 明细).
  //   分层与夏普走**同一条聚合规则** (均值 / IR / p10 / 胜率), 只有一套口径.
  const int iq = (nw - 1) / 10; // p10: 下取整位次
  auto summarize = [&](const float *v, float *o) {
    double sm = 0.0;
    int n_pos = 0;
    for (int w = 0; w < nw; ++w) {
      sm += static_cast<double>(v[w]);
      if (v[w] > 0.0f)
        ++n_pos;
    }
    double mean = sm / static_cast<double>(nw);
    double var = 0.0;
    for (int w = 0; w < nw; ++w) {
      double d = static_cast<double>(v[w]) - mean;
      var += d * d;
    }
    var /= static_cast<double>(nw);
    std::copy(v, v + nw, sc.tmp.data());
    std::nth_element(sc.tmp.data(), sc.tmp.data() + iq, sc.tmp.data() + nw);
    o[0] = static_cast<float>(mean);
    o[1] = static_cast<float>((var > 0.0) ? mean / std::sqrt(var) : 0.0);
    o[2] = sc.tmp[static_cast<std::size_t>(iq)];
    o[3] = static_cast<float>(n_pos) / static_cast<float>(nw);
  };

  for (int b = 0; b < np; ++b) {
    const std::size_t wo =
        static_cast<std::size_t>(b) * static_cast<std::size_t>(nw);
    const float *sw = sc.sw.data() + wo;
    const float *bw = sc.bw.data() + wo;
    const float *r2w = sc.r2w.data() + wo;
    const float *shw = sc.shw.data() + wo;

    float *o = out_sum + static_cast<std::size_t>(b) *
                             static_cast<std::size_t>(MINE_N_POINT_METRICS);
    summarize(sw, o);      // 梳子 均值/IR/p10/胜率
    summarize(shw, o + 6); // 夏普 均值/IR/p10/胜率
    double sb = 0.0, sr2 = 0.0;
    for (int w = 0; w < nw; ++w) {
      sb += static_cast<double>(bw[w]);
      sr2 += static_cast<double>(r2w[w]);
    }
    o[4] = static_cast<float>(sb / static_cast<double>(nw));
    o[5] = static_cast<float>(sr2 / static_cast<double>(nw));

    if (out_win != nullptr) {
      float *ow = out_win + static_cast<std::size_t>(b) *
                                static_cast<std::size_t>(nw) *
                                static_cast<std::size_t>(MINE_N_WINDOW_METRICS);
      for (int w = 0; w < nw; ++w) {
        ow[w * MINE_N_WINDOW_METRICS + 0] = sw[w];
        ow[w * MINE_N_WINDOW_METRICS + 1] = bw[w];
        ow[w * MINE_N_WINDOW_METRICS + 2] = r2w[w];
        ow[w * MINE_N_WINDOW_METRICS + 3] = shw[w];
      }
    }
  }
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

// ============================================================================
// lattice 组合序号 — build_lattice 的枚举序本身是 lattice → [0, P) 的双射, 故
//   邻居定位不需要哈希表 / 排序 / 边表: 一张 N[i][rem] 计数表 (<1KB, 常驻 L1)
//   就能把任意 k 直接算成它的行号.
//     N[i][rem] = 坐标 i..n−1 用完预算 rem 的方案数
//     index(k)  = Σ_i Σ_{v < k_i} N[i+1][rem_i − |v|]   (末位只需判正负)
//   ⇒ 每个邻居 ~O(n·M) 次纯算术、零 DRAM 访问. 启动期 assert index(kgrid[r])
//   == r 把"枚举序"与"查表"的一致性钉死.
// ============================================================================
struct LatticeIndex {
  // [n+1][M+1]; 末位 i = n−1 单独处理, 表里存 i ∈ [0, n]
  std::int64_t n_ways[MINE_N_FACTORS + 1][MINE_LATTICE_M + 1] = {};

  LatticeIndex() {
    // i = n−1: 预算 0 只有 k=0; 预算 rem>0 有 ±rem 两种.
    for (int rem = 0; rem <= MINE_LATTICE_M; ++rem)
      n_ways[MINE_N_FACTORS - 1][rem] = (rem == 0) ? 1 : 2;
    for (int i = MINE_N_FACTORS - 2; i >= 0; --i) {
      for (int rem = 0; rem <= MINE_LATTICE_M; ++rem) {
        std::int64_t s = n_ways[i + 1][rem]; // v = 0
        for (int u = 1; u <= rem; ++u)
          s += 2 * n_ways[i + 1][rem - u]; // v = ±u
        n_ways[i][rem] = s;
      }
    }
  }

  std::int64_t total() const { return n_ways[0][MINE_LATTICE_M]; }

  std::int64_t index(const std::int8_t *k) const {
    std::int64_t idx = 0;
    int rem = MINE_LATTICE_M;
    for (int i = 0; i < MINE_N_FACTORS - 1; ++i) {
      const int ki = k[i];
      assert(std::abs(ki) <= rem);
      for (int v = -rem; v < ki; ++v)
        idx += n_ways[i + 1][rem - std::abs(v)];
      rem -= std::abs(ki);
    }
    assert(std::abs(static_cast<int>(k[MINE_N_FACTORS - 1])) == rem &&
           "k 不在 Σ|k| = M 的 lattice 上");
    if (rem > 0 && k[MINE_N_FACTORS - 1] < 0)
      idx += 1; // 末位枚举序是 (+rem, −rem)
    return idx;
  }
};

// 1 跳邻居 = 把一个单位从坐标 i 挪到坐标 j (Σ|k| = M 不变):
//   |k_i| 减 1 (朝 0 走), |k_j| 加 1 (背离 0; k_j = 0 时正负各一个).
//   越界不可能: Σ|k| = M 且 k_i ≠ 0 ⇒ |k_j| ≤ M−1, 加一仍 ≤ M.
//   把每个邻居的行号交给 fn. n=13/M=8 下约 120 个.
template <class F>
void for_each_neighbor(const std::int8_t *k, const LatticeIndex &li, F &&fn) {
  std::int8_t nb[MINE_N_FACTORS];
  std::copy(k, k + MINE_N_FACTORS, nb);
  for (int i = 0; i < MINE_N_FACTORS; ++i) {
    if (k[i] == 0)
      continue;
    const std::int8_t si = static_cast<std::int8_t>(k[i] > 0 ? 1 : -1);
    nb[i] = static_cast<std::int8_t>(k[i] - si);
    for (int j = 0; j < MINE_N_FACTORS; ++j) {
      if (j == i)
        continue;
      if (k[j] == 0) {
        nb[j] = 1;
        fn(li.index(nb));
        nb[j] = -1;
        fn(li.index(nb));
      } else {
        nb[j] = static_cast<std::int8_t>(k[j] + (k[j] > 0 ? 1 : -1));
        fn(li.index(nb));
      }
      nb[j] = k[j];
    }
    nb[i] = k[i];
  }
}

// ============================================================================
// 持仓去重 — "分层+夏普+稳健都好"的点会大量扎堆在同一个风格上 (权重稍微挪一格,
//   选出的 10 只可能一模一样). 判据直接用**持仓**而不是权重距离: 权重差很远却
//   选出同一批股票的情况很常见.
// ============================================================================

// 逐日 top-hold_n 持仓, [n_d][hold_n] int32, 每日内 a 升序 (便于求交).
//   选股走 select_top ⇒ 与回测路径同一份口径.
void build_holdings(const PrStore &pr, const Plan &pl, int hold_n, float *acc,
                    std::pair<float, int> *sel, std::int32_t *out) {
  for (int i = 0; i < pr.n_d; ++i) {
    const int cnt = pr.cnt(i);
    assert(cnt >= hold_n && "池内成员少于 hold_n");
    select_top(pr.block(i), pr.members(i), cnt, pl, hold_n, acc, sel);
    std::int32_t *row = out + static_cast<std::size_t>(i) *
                                  static_cast<std::size_t>(hold_n);
    for (int j = 0; j < hold_n; ++j)
      row[j] = sel[j].second;
    std::sort(row, row + hold_n);
  }
}

// 平均逐日持仓重合度 ∈ [0, 1] = mean_t |A_t ∩ B_t| / hold_n.
double holdings_overlap(const std::int32_t *x, const std::int32_t *y, int n_d,
                        int hold_n) {
  std::int64_t hit = 0;
  for (int i = 0; i < n_d; ++i) {
    const std::int32_t *a = x + static_cast<std::size_t>(i) *
                                    static_cast<std::size_t>(hold_n);
    const std::int32_t *b = y + static_cast<std::size_t>(i) *
                                    static_cast<std::size_t>(hold_n);
    int p = 0, q = 0;
    while (p < hold_n && q < hold_n) {
      if (a[p] < b[q])
        ++p;
      else if (b[q] < a[p])
        ++q;
      else {
        ++hit;
        ++p;
        ++q;
      }
    }
  }
  return static_cast<double>(hit) /
         (static_cast<double>(n_d) * static_cast<double>(hold_n));
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
  PrStore pr = build_pr(axes, T, mk, s_idx, d_lo, n_d);
  const LayerParams lp = make_layer(spec, n_d);
  std::printf("[mine] 分层: %d 档 (档宽 ≈ %d 只), 滑窗 %d 日 / 步 %d 日 ⇒ %d 窗\n",
              lp.n_bins, spec.hold_n, LAYER_WINDOW, LAYER_STEP, lp.n_win);
  // 原先逐 (权重, 日) 检查的 "可交易成员 ≥ 档数" 只取决于日, 启动期一次钉死.
  for (int t = 0; t < lp.n_ret; ++t)
    assert(pr.vcnt(t) >= lp.n_bins && "当日可交易池成员数少于档数");

  // 目标策略自己的 weights → Plan (自检 + 基线梳子分共用).
  Plan p0;
  {
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
  }

  // ---- 自检: 用目标策略自己的 weights 走本管线, 与 backtest::run 对账 ----
  //   过不了说明 "PR 预计算 + top-K 截断 + 内核" 这条链与报告路径有分歧,
  //   此时挖出来的权重填回 cpp 也不会复现 ⇒ 直接 fail, 不留侥幸.
  float selfcheck_max_rel = 0.0f;
  {
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

  // ---- 基线 (目标策略当前 weights, 进 meta 供 py 侧对照"挖掘到底有没有用") ----
  float base_pt[MINE_N_POINT_METRICS] = {};
  {
    LayerScratch lsc = make_layer_scratch(pr, lp, 1);
    run_layer_batch(pr, lp, std::span<const Plan>(&p0, 1), lsc, base_pt,
                    nullptr);
    std::printf("[mine] 基线: 梳子均值 %.4f IR %.2f | 夏普均值 %.2f IR %.2f\n",
                static_cast<double>(base_pt[0]), static_cast<double>(base_pt[1]),
                static_cast<double>(base_pt[6]), static_cast<double>(base_pt[7]));
  }

  const std::vector<std::int8_t> kgrid = build_lattice();
  const std::int64_t n_p = static_cast<std::int64_t>(kgrid.size()) / MINE_N_FACTORS;
  unsigned n_threads = misc::Affinity::core_count();

  // 组合序号自检: 枚举序 ⇔ index() 必须是同一个双射, 否则邻域查表全错.
  const LatticeIndex li;
  {
    assert(li.total() == n_p && "组合计数表与 lattice 枚举数不符");
    const std::int64_t stride = std::max<std::int64_t>(1, n_p / 10000);
    for (std::int64_t r = 0; r < n_p; r += stride)
      assert(li.index(kgrid.data() + r * MINE_N_FACTORS) == r &&
             "lattice 组合序号与枚举序不一致");
  }

  // ---- Phase 1: 全 lattice 一遍日循环 (滑窗分层 + 顶档滑窗夏普) ----
  std::vector<float> ptm(static_cast<std::size_t>(n_p) *
                         static_cast<std::size_t>(MINE_N_POINT_METRICS));
  {
    std::atomic<std::int64_t> next{0};
    std::atomic<std::int64_t> done{0};
    auto t_scan = std::chrono::high_resolution_clock::now();
    const std::int64_t step = n_p / 100 + 1;

    auto worker = [&]() {
      LayerScratch lsc = make_layer_scratch(pr, lp, BATCH);
      for (;;) {
        std::int64_t lo = next.fetch_add(BATCH, std::memory_order_relaxed);
        if (lo >= n_p)
          break;
        int np = static_cast<int>(std::min<std::int64_t>(BATCH, n_p - lo));
        for (int b = 0; b < np; ++b) {
          lsc.plans[static_cast<std::size_t>(b)] =
              plan_from_k(kgrid.data() + (lo + b) * MINE_N_FACTORS);
        }
        run_layer_batch(pr, lp,
                        std::span<const Plan>(lsc.plans.data(),
                                              static_cast<std::size_t>(np)),
                        lsc,
                        ptm.data() + static_cast<std::size_t>(lo) *
                                         static_cast<std::size_t>(MINE_N_POINT_METRICS),
                        nullptr);

        std::int64_t prev = done.fetch_add(np, std::memory_order_relaxed);
        std::int64_t cur = prev + np;
        if (prev / step != cur / step) {
          std::chrono::duration<double> el =
              std::chrono::high_resolution_clock::now() - t_scan;
          double rate = static_cast<double>(cur) / el.count();
          std::printf("[mine] 扫描 %5.1f%%  %.0f 权重/s  已用 %.0fs  剩余 ~%.0fs\n",
                      100.0 * static_cast<double>(cur) / static_cast<double>(n_p),
                      rate, el.count(),
                      static_cast<double>(n_p - cur) / rate);
          std::fflush(stdout);
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

  // ---- Phase 2: 三个截面分数 → 总分 (全格, 纯内存) ----
  //   col 0 = 梳子均值 → u1;  col 6 = 夏普均值 → u2;
  //   敏感性建在 u2 上 (关心的是"收益对权重扰动有多敏感");
  //   u3 = pctrank(−敏感性);  总分 = u1·u2·u3.
  {
    misc::Timer t2("[mine] 截面评分 + 邻域敏感性");
    const std::size_t P = static_cast<std::size_t>(n_p);
    const std::size_t C = static_cast<std::size_t>(MINE_N_POINT_METRICS);
    std::vector<float> u1(P), u2(P), u3(P);
    for (std::size_t i = 0; i < P; ++i) {
      u1[i] = ptm[i * C + 0];
      u2[i] = ptm[i * C + 6];
    }
    feature::pct_rank(std::span<float>(u1));
    feature::pct_rank(std::span<float>(u2));

    // 敏感性 = mean_nb |u2(nb) − u2(k)| / E_null|U − u2(k)|.
    //   分母是邻域纯噪声时的闭式期望 ⇒ 1 = 与噪声无异, 0 = 完美平原, >1 = 真尖峰.
    //   不除掉它, 纯噪声下两端点 (含榜首) 会仅因身处边界而被多罚.
    {
      std::atomic<std::int64_t> next{0};
      auto worker = [&]() {
        for (;;) {
          std::int64_t lo = next.fetch_add(8192, std::memory_order_relaxed);
          if (lo >= n_p)
            break;
          std::int64_t hi = std::min<std::int64_t>(lo + 8192, n_p);
          for (std::int64_t r = lo; r < hi; ++r) {
            const double vc = static_cast<double>(u2[static_cast<std::size_t>(r)]);
            double sum = 0.0;
            std::int64_t n_nb = 0;
            for_each_neighbor(
                kgrid.data() + r * MINE_N_FACTORS, li, [&](std::int64_t nb) {
                  assert(nb >= 0 && nb < n_p);
                  sum += std::fabs(static_cast<double>(u2[static_cast<std::size_t>(nb)]) - vc);
                  ++n_nb;
                });
            assert(n_nb > 0 && "lattice 点无 1 跳邻居 — MINE_N_FACTORS < 2?");
            const double null = 0.5 * (vc * vc + (1.0 - vc) * (1.0 - vc));
            u3[static_cast<std::size_t>(r)] = static_cast<float>(
                sum / static_cast<double>(n_nb) / null);
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
    for (std::size_t i = 0; i < P; ++i)
      ptm[i * C + 12] = u3[i]; // 敏感性 (原始值, 有刻度)
    for (std::size_t i = 0; i < P; ++i)
      u3[i] = -u3[i];
    feature::pct_rank(std::span<float>(u3));

    for (std::size_t i = 0; i < P; ++i) {
      ptm[i * C + 10] = u1[i];
      ptm[i * C + 11] = u2[i];
      ptm[i * C + 13] = u3[i];
      ptm[i * C + 14] = u1[i] * u2[i] * u3[i];
    }
  }

  // 基线在全格中的位置 (梳子均值 / 夏普均值 各自的百分位).
  float base_pct_comb = 0.0f, base_pct_sharpe = 0.0f;
  {
    const std::size_t C = static_cast<std::size_t>(MINE_N_POINT_METRICS);
    std::int64_t lo_c = 0, lo_s = 0;
    for (std::int64_t i = 0; i < n_p; ++i) {
      if (ptm[static_cast<std::size_t>(i) * C + 0] < base_pt[0])
        ++lo_c;
      if (ptm[static_cast<std::size_t>(i) * C + 6] < base_pt[6])
        ++lo_s;
    }
    base_pct_comb = static_cast<float>(static_cast<double>(lo_c) /
                                       static_cast<double>(n_p));
    base_pct_sharpe = static_cast<float>(static_cast<double>(lo_s) /
                                         static_cast<double>(n_p));
    std::printf("[mine] 基线在全格中: 梳子均值 %.1f%% 分位, 夏普均值 %.1f%% 分位\n",
                100.0 * static_cast<double>(base_pct_comb),
                100.0 * static_cast<double>(base_pct_sharpe));
  }

  // ---- Phase 3: 持仓去重 (总分降序流式贪心) ----
  std::vector<std::int64_t> styles;
  {
    misc::Timer t3("[mine] 持仓去重");
    const std::size_t C = static_cast<std::size_t>(MINE_N_POINT_METRICS);
    const int hn = spec.hold_n;
    const std::size_t hsz = static_cast<std::size_t>(n_d) *
                            static_cast<std::size_t>(hn);

    // 按总分取前 MINE_DEDUP_CAND 名 (并列按下标破序 ⇒ 确定性).
    const std::int64_t n_cand = std::min<std::int64_t>(n_p, MINE_DEDUP_CAND);
    std::vector<std::int64_t> cand(static_cast<std::size_t>(n_p));
    for (std::int64_t i = 0; i < n_p; ++i)
      cand[static_cast<std::size_t>(i)] = i;
    auto better = [&](std::int64_t a, std::int64_t b) {
      float sa = ptm[static_cast<std::size_t>(a) * C + 14];
      float sb = ptm[static_cast<std::size_t>(b) * C + 14];
      if (sa != sb)
        return sa > sb;
      return a < b;
    };
    std::nth_element(cand.begin(),
                     cand.begin() + static_cast<std::ptrdiff_t>(n_cand),
                     cand.end(), better);
    cand.resize(static_cast<std::size_t>(n_cand));
    std::sort(cand.begin(), cand.end(), better);

    // 分块: 块内并行算持仓, 再串行做贪心接受 (接受决策依赖前序, 必须串行;
    //   但块内新接受的风格同样参与后续比较 ⇒ 与全串行等价).
    constexpr int CH = 512;
    std::vector<std::int32_t> chunk(static_cast<std::size_t>(CH) * hsz);
    std::vector<std::int32_t> accepted; // [styles.size()][hsz]
    for (std::int64_t base = 0; base < n_cand; base += CH) {
      const int nc = static_cast<int>(std::min<std::int64_t>(CH, n_cand - base));
      std::atomic<int> next{0};
      auto worker = [&]() {
        std::vector<float> acc(static_cast<std::size_t>(pr.max_cnt));
        std::vector<std::pair<float, int>> sel(
            static_cast<std::size_t>(hn + TIE_MARGIN));
        for (;;) {
          int b = next.fetch_add(1, std::memory_order_relaxed);
          if (b >= nc)
            break;
          Plan pl = plan_from_k(kgrid.data() +
                                cand[static_cast<std::size_t>(base + b)] *
                                    MINE_N_FACTORS);
          build_holdings(pr, pl, hn, acc.data(), sel.data(),
                         chunk.data() + static_cast<std::size_t>(b) * hsz);
        }
      };
      std::vector<std::thread> ths;
      ths.reserve(n_threads);
      for (unsigned k = 0; k < n_threads; ++k)
        ths.emplace_back(worker);
      for (auto &th : ths)
        th.join();

      for (int b = 0; b < nc; ++b) {
        const std::int32_t *h = chunk.data() + static_cast<std::size_t>(b) * hsz;
        bool dup = false;
        for (std::size_t s = 0; s < styles.size(); ++s) {
          if (holdings_overlap(h, accepted.data() + s * hsz, n_d, hn) >=
              DEDUP_OVERLAP) {
            dup = true; // 前面那个分更高, 留它
            break;
          }
        }
        if (dup)
          continue;
        styles.push_back(cand[static_cast<std::size_t>(base + b)]);
        accepted.insert(accepted.end(), h, h + hsz);
      }
    }
    std::printf("[mine] 去重: 总分前 %lld 个候选 ⇒ %zu 个风格 "
                "(平均逐日重合 ≥ %.2f 视为同一风格)\n",
                static_cast<long long>(n_cand), styles.size(), DEDUP_OVERLAP);
  }

  // ---- Phase 4: 最终名单真回测 + per-window 明细 ----
  //   分层摘要重算并与 Phase 1 逐位对账 (同代码同输入 ⇒ 必须 bitwise 相同).
  const std::int64_t n_st = static_cast<std::int64_t>(styles.size());
  std::vector<Metrics> bt(static_cast<std::size_t>(n_st));
  std::vector<float> windet(static_cast<std::size_t>(n_st) *
                            static_cast<std::size_t>(lp.n_win) *
                            static_cast<std::size_t>(MINE_N_WINDOW_METRICS));
  {
    misc::Timer t4("[mine] 最终名单真回测");
    std::atomic<std::int64_t> next{0};
    auto worker = [&]() {
      std::vector<Engine> engs = make_engines(mk, spec);
      Scratch sc = make_scratch(pr, k_need);
      LayerScratch lsc = make_layer_scratch(pr, lp, BATCH);
      std::vector<float> chk(static_cast<std::size_t>(BATCH) *
                             static_cast<std::size_t>(MINE_N_POINT_METRICS));
      for (;;) {
        std::int64_t lo = next.fetch_add(BATCH, std::memory_order_relaxed);
        if (lo >= n_st)
          break;
        int np = static_cast<int>(std::min<std::int64_t>(BATCH, n_st - lo));
        for (int b = 0; b < np; ++b) {
          sc.plans[static_cast<std::size_t>(b)] = plan_from_k(
              kgrid.data() + styles[static_cast<std::size_t>(lo + b)] * MINE_N_FACTORS);
        }
        std::span<const Plan> pls(sc.plans.data(), static_cast<std::size_t>(np));
        run_batch(pr, k_need, pls, engs, sc,
                  std::span<Metrics>(bt.data() + lo, static_cast<std::size_t>(np)));
        run_layer_batch(pr, lp, pls, lsc, chk.data(),
                        windet.data() + static_cast<std::size_t>(lo) *
                                            static_cast<std::size_t>(lp.n_win) *
                                            static_cast<std::size_t>(MINE_N_WINDOW_METRICS));
        for (int b = 0; b < np; ++b) {
          for (int c = 0; c < 10; ++c) { // 前 10 列是 Phase 1 摘要
            assert(chk[static_cast<std::size_t>(b) *
                           static_cast<std::size_t>(MINE_N_POINT_METRICS) +
                       static_cast<std::size_t>(c)] ==
                       ptm[static_cast<std::size_t>(
                               styles[static_cast<std::size_t>(lo + b)]) *
                               static_cast<std::size_t>(MINE_N_POINT_METRICS) +
                           static_cast<std::size_t>(c)] &&
                   "Phase 4 重算与 Phase 1 不一致 — 非确定性?");
          }
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
                            static_cast<std::size_t>(MINE_N_POINT_METRICS)};
    misc::write_npy_f4(out / "point_metrics.npy",
                       std::span<const float>(ptm.data(), ptm.size()),
                       std::span<const std::size_t>(shape, 2));
  }
  {
    assert(n_p <= std::numeric_limits<std::int32_t>::max());
    std::vector<std::int32_t> st32;
    st32.reserve(static_cast<std::size_t>(n_st));
    for (std::int64_t i : styles)
      st32.push_back(static_cast<std::int32_t>(i));
    std::size_t shape[1] = {static_cast<std::size_t>(n_st)};
    misc::write_npy_i4(out / "styles.npy",
                       std::span<const std::int32_t>(st32.data(), st32.size()),
                       std::span<const std::size_t>(shape, 1));
  }
  {
    std::size_t shape[2] = {static_cast<std::size_t>(n_st),
                            static_cast<std::size_t>(MINE_N_BT_METRICS)};
    misc::write_npy_f4(
        out / "bt_metrics.npy",
        std::span<const float>(reinterpret_cast<const float *>(bt.data()),
                               static_cast<std::size_t>(n_st) *
                                   static_cast<std::size_t>(MINE_N_BT_METRICS)),
        std::span<const std::size_t>(shape, 2));
  }
  {
    std::size_t shape[3] = {static_cast<std::size_t>(n_st),
                            static_cast<std::size_t>(lp.n_win),
                            static_cast<std::size_t>(MINE_N_WINDOW_METRICS)};
    misc::write_npy_f4(out / "windows.npy",
                       std::span<const float>(windet.data(), windet.size()),
                       std::span<const std::size_t>(shape, 3));
  }
  {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    report::add_str(doc, root, "strategy", spec.name);
    yyjson_mut_obj_add_int(doc, root, "lattice_m", MINE_LATTICE_M);
    yyjson_mut_obj_add_int(doc, root, "n_points", n_p);
    yyjson_mut_obj_add_int(doc, root, "n_styles", n_st);
    yyjson_mut_obj_add_int(doc, root, "dedup_cand", MINE_DEDUP_CAND);
    yyjson_mut_obj_add_real(doc, root, "dedup_overlap", DEDUP_OVERLAP);

    std::vector<std::string> fnames, fcn;
    fnames.reserve(static_cast<std::size_t>(MINE_N_FACTORS));
    fcn.reserve(static_cast<std::size_t>(MINE_N_FACTORS));
    for (int f = 0; f < MINE_N_FACTORS; ++f) {
      fnames.emplace_back(MINE_FACTORS[f]->name);
      fcn.emplace_back(MINE_FACTORS[f]->cn_name);
    }
    report::add_str_arr(doc, root, "factor_names", fnames);
    report::add_str_arr(doc, root, "factor_cn_names", fcn);
    report::add_sv_arr(doc, root, "point_metric_names",
                       std::span<const std::string_view>(MINE_POINT_METRIC_NAMES,
                                                         MINE_N_POINT_METRICS));
    report::add_sv_arr(doc, root, "bt_metric_names",
                       std::span<const std::string_view>(MINE_BT_METRIC_NAMES,
                                                         MINE_N_BT_METRICS));
    report::add_sv_arr(doc, root, "window_metric_names",
                       std::span<const std::string_view>(MINE_WINDOW_METRIC_NAMES,
                                                         MINE_N_WINDOW_METRICS));

    yyjson_mut_val *win = report::add_obj(doc, root, "window");
    report::add_str(doc, win, "start", axes.dates[static_cast<std::size_t>(d_lo)]);
    report::add_str(doc, win, "end", axes.dates.back());
    yyjson_mut_obj_add_int(doc, win, "n_days", n_d);

    yyjson_mut_val *lay = report::add_obj(doc, root, "layer");
    yyjson_mut_obj_add_int(doc, lay, "n_bins", lp.n_bins);
    yyjson_mut_obj_add_int(doc, lay, "window_days", LAYER_WINDOW);
    yyjson_mut_obj_add_int(doc, lay, "step_days", LAYER_STEP);
    yyjson_mut_obj_add_int(doc, lay, "n_windows", lp.n_win);
    yyjson_mut_obj_add_real(doc, lay, "k_se", LAYER_K_SE);
    {
      std::vector<std::string> starts;
      starts.reserve(static_cast<std::size_t>(lp.n_win));
      for (int w = 0; w < lp.n_win; ++w)
        starts.push_back(
            axes.dates[static_cast<std::size_t>(d_lo + w * LAYER_STEP)]);
      report::add_str_arr(doc, lay, "window_starts", starts);
    }

    // 目标策略当前 weights + 其梳子分 (py 侧要在结果里标出"基线在哪儿")
    yyjson_mut_val *base = report::add_obj(doc, root, "baseline_weights");
    for (const strategy::FactorWeight &fw : spec.weights)
      yyjson_mut_obj_add_real(doc, base, fw.f->name, static_cast<double>(fw.w));
    yyjson_mut_val *blay = report::add_obj(doc, root, "baseline_metrics");
    for (int c = 0; c < 10; ++c) { // 前 10 列 = Phase 1 摘要 (后 5 列是全格派生)
      // string_view 均来自字面量 ⇒ data() 静态存储期且以 0 结尾, 可走
      //   yyjson 不拷贝 key 的 const char* 路径.
      report::add_f4(doc, blay, MINE_POINT_METRIC_NAMES[c].data(), base_pt[c]);
    }
    report::add_f4(doc, blay, "梳子均值分位", base_pct_comb);
    report::add_f4(doc, blay, "夏普均值分位", base_pct_sharpe);

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
