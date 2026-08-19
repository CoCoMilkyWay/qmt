#include "strategy/columns.hpp"

#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/industry_l1.hpp"
#include "feature/def/basic/is_margin.hpp"
#include "feature/def/basic/list_age.hpp"
#include "feature/def/basic/susp.hpp"
#include "feature/industry.hpp"
#include "misc/affinity.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace strategy {

using feature::Axes;
using feature::is_finite;
using feature::StockMeta;
using feature::SW2021_L1_COUNT;
using feature::Tensor;

namespace {

// 白名单查表. 未覆盖 (空串) 一律不命中 → pool_b 全期 0.
inline bool in_whitelist(std::string_view v,
                         std::span<const std::string_view> wl) {
  if (v.empty())
    return false;
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

inline bool in_int_whitelist(std::int8_t v, std::span<const std::int8_t> wl) {
  for (auto w : wl) {
    if (v == w)
      return true;
  }
  return false;
}

// industry_l1 ID 白名单 mask: spec 的中文 string_view 白名单转 array<bool, 32>,
//   mask[id]=true 表该 SW2021 一级行业 ID 在白名单内. 每策略启动期构建一次.
//   拼写不在 SW2021_L1_NAMES 内的名字 → id=0 被忽略 (industry_l1=0 = "未知"
//   永远不该入白名单).
std::array<bool, SW2021_L1_COUNT>
industry_l1_whitelist_mask(std::span<const std::string_view> wl) {
  std::array<bool, SW2021_L1_COUNT> m{};
  for (auto name : wl) {
    std::uint8_t id = feature::sw2021_l1_name_to_id(name);
    if (id != 0)
      m[id] = true;
  }
  return m;
}

// pool_b = exchange ∈ wl ∧ list_sector ∈ wl ∧ industry_l1 ∈ wl
//          ∧ 已上市 ∧ ¬susp ∧ ¬退市 ∧ margin_policy(is_margin):
//            Exclude → ¬is_margin; Include → 不动; Only → is_margin
//   exchange / list_sector 是 asset 静态 (全 D 同值, 启动期判一次);
//   industry_l1 是时变 (per-D 读 T.ts_row(industry_l1_spec, a) → ID → mask 查白名单).
//   industry_l1 ID 0 (未知) 不在 mask 任何位 → ¬ind_ok, 自然排除.
void ts_pool_b(int s, const StrategySpec &spec,
               const std::array<bool, SW2021_L1_COUNT> &mask, int a,
               const Axes &axes, const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto susp_ = T.ts_row(feature::def::susp_spec, a);
  auto is_marg_ = T.ts_row(feature::def::is_margin_spec, a);
  auto list_age_ = T.ts_row(feature::def::list_age_spec, a);
  auto delist_age_ = T.ts_row(feature::def::delist_age_spec, a);
  auto industry_l1_ = T.ts_row(feature::def::industry_l1_spec, a);
  auto out = T.strat_ts_row(slot(s, SF::pool_b), a);

  bool ex_ok = in_whitelist(meta.exchange[a], spec.pool.exchange_wl);
  bool sec_ok = in_int_whitelist(meta.list_sector[a], spec.pool.list_sector_wl);
  bool asset_ok = ex_ok && sec_ok;
  MarginPolicy mp = spec.pool.margin_policy;

  for (int d = 0; d < n_d; ++d) {
    bool ind_ok = false;
    if (asset_ok) {
      int id = static_cast<int>(industry_l1_[d]);
      if (id > 0 && id < static_cast<int>(SW2021_L1_COUNT))
        ind_ok = mask[static_cast<std::size_t>(id)];
    }
    bool b = asset_ok && ind_ok && is_finite(list_age_[d]) &&
             !(susp_[d] > 0.5f) &&
             !is_finite(delist_age_[d]);
    bool marg = is_marg_[d] > 0.5f;
    if (mp == MarginPolicy::Exclude)
      b = b && !marg;
    else if (mp == MarginPolicy::Only)
      b = b && marg;
    out[d] = b ? 1.0f : 0.0f;
  }
}

// pool: pool_b ∧ rank(rank_key within pool_b) ≤ universe_size.
//   rank_key 值须 finite 且 > 0 才参与 (mcap 等估值 raw 的 ≤0 是哨兵/脏值).
//   buf_a=pool_b, buf_b=rank_key, buf_c=输出.
void cs_pool(int s, const StrategySpec &spec, int d, Tensor &T,
             std::span<float> buf_a, std::span<float> buf_b,
             std::span<float> buf_c) {
  T.strat_gather_cs_row(slot(s, SF::pool_b), d, buf_a);
  T.gather_cs_row(*spec.pool.rank_key, d, buf_b);

  std::vector<std::pair<float, int>> cands; // (key, a)
  cands.reserve(buf_a.size());
  for (std::size_t a = 0; a < buf_a.size(); ++a) {
    if (!(buf_a[a] > 0.5f))
      continue;
    float key = buf_b[a];
    if (!is_finite(key) || key <= 0.0f)
      continue;
    cands.emplace_back(key, static_cast<int>(a));
  }

  int n = static_cast<int>(cands.size());
  int k = std::min(spec.pool.universe_size, n);
  // k == n 时 cands.begin()+k == cands.end(), std::nth_element 第二参传 end 是 UB;
  // 全部入选时无需分割, 跳过 nth_element.
  if (k > 0 && k < n) {
    if (spec.pool.rank_asc) {
      std::nth_element(cands.begin(), cands.begin() + k, cands.end(),
                       [](const auto &x, const auto &y) { return x.first < y.first; });
    } else {
      std::nth_element(cands.begin(), cands.begin() + k, cands.end(),
                       [](const auto &x, const auto &y) { return x.first > y.first; });
    }
  }

  std::fill(buf_c.begin(), buf_c.end(), 0.0f);
  for (int i = 0; i < k; ++i)
    buf_c[static_cast<std::size_t>(cands[i].second)] = 1.0f;
  T.strat_scatter_cs_row(slot(s, SF::pool), d,
                         std::span<const float>(buf_c.data(), buf_c.size()));
}

// tradable: pool ∧ ¬OR(spec.filters).
//   pool 是 cs (pct_rank / nth-smallest 母集), tradable 是策略实际 top-K 母集.
//   buf_a=pool→输出, buf_b=filter gather.
void cs_tradable(int s, const StrategySpec &spec, int d, Tensor &T,
                 std::span<float> buf_a, std::span<float> buf_b) {
  T.strat_gather_cs_row(slot(s, SF::pool), d, buf_a);
  std::size_t na = buf_a.size();

  for (const feature::FeatureSpec *src : spec.filters) {
    T.gather_cs_row(*src, d, buf_b);
    for (std::size_t a = 0; a < na; ++a) {
      if (buf_b[a] > 0.5f)
        buf_a[a] = 0.0f;
    }
  }

  T.strat_scatter_cs_row(slot(s, SF::tradable), d,
                         std::span<const float>(buf_a.data(), na));
}

// score: 配置因子 finite-加权平均 (全截面可算, 供 analysis IC). w 可正可负
//   (符号定义该因子的方向), 分母用 Σ|w| (非 Σw) 归一, 避免正负抵消导致
//   除零或整体符号被意外翻转.
//   factor_pipeline 已保证每个 factor 全 finite; pool/tradable 由下游另行 mask.
//   buf_a=acc→输出, buf_b=|权重|和, buf_c=factor gather.
void cs_score(int s, const StrategySpec &spec, int d, Tensor &T,
              std::span<float> buf_a, std::span<float> buf_b,
              std::span<float> buf_c) {
  std::size_t na = buf_a.size();
  std::fill(buf_a.begin(), buf_a.end(), 0.0f);
  std::fill(buf_b.begin(), buf_b.end(), 0.0f);

  for (const FactorWeight &fw : spec.weights) {
    T.gather_cs_row(*fw.f, d, buf_c);
    float abs_w = std::fabs(fw.w);
    for (std::size_t a = 0; a < na; ++a) {
      float v = buf_c[a];
      if (is_finite(v)) {
        buf_a[a] += fw.w * v;
        buf_b[a] += abs_w;
      }
    }
  }

  for (std::size_t a = 0; a < na; ++a) {
    assert(buf_b[a] > 0.0f && "score: no finite configured factor");
    buf_a[a] = buf_a[a] / buf_b[a];
  }
  T.strat_scatter_cs_row(slot(s, SF::score), d,
                         std::span<const float>(buf_a.data(), na));
}

// rank: score 在 tradable ∧ finite(score) 内的 1-based 降序排名, 0 = 不在母集.
//   并列 score 按 a 升序破序 ⇒ 排名确定 (回测 / 实盘 / dump 对账读同一列).
//   buf_a=tradable, buf_b=score, buf_c=输出.
void cs_rank(int s, const StrategySpec &, int d, Tensor &T,
             std::span<float> buf_a, std::span<float> buf_b,
             std::span<float> buf_c) {
  T.strat_gather_cs_row(slot(s, SF::tradable), d, buf_a);
  T.strat_gather_cs_row(slot(s, SF::score), d, buf_b);

  std::vector<std::pair<float, int>> ranked; // (score, a)
  ranked.reserve(buf_a.size());
  for (std::size_t a = 0; a < buf_a.size(); ++a) {
    if (!(buf_a[a] > 0.5f))
      continue;
    if (!is_finite(buf_b[a]))
      continue;
    ranked.emplace_back(buf_b[a], static_cast<int>(a));
  }
  std::sort(ranked.begin(), ranked.end(), [](const auto &x, const auto &y) {
    if (x.first != y.first)
      return x.first > y.first;
    return x.second < y.second;
  });

  std::fill(buf_c.begin(), buf_c.end(), 0.0f);
  for (std::size_t i = 0; i < ranked.size(); ++i)
    buf_c[static_cast<std::size_t>(ranked[i].second)] =
        static_cast<float>(i + 1);
  T.strat_scatter_cs_row(slot(s, SF::rank), d,
                         std::span<const float>(buf_c.data(), buf_c.size()));
}

} // namespace

void compute_ts_columns(const Axes &axes, const StockMeta &meta, Tensor &T) {
  // 每策略行业白名单 mask 启动期一次性构建, worker 只读.
  std::vector<std::array<bool, SW2021_L1_COUNT>> masks;
  masks.reserve(STRATEGIES.size());
  for (const StrategySpec *spec : STRATEGIES)
    masks.push_back(industry_l1_whitelist_mask(spec->pool.industry_l1_wl));

  int n_a = axes.n_a();
  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0)
    n_threads = 1;
  std::atomic<int> next{0};

  auto worker = [&]() {
    for (;;) {
      int a = next.fetch_add(1, std::memory_order_relaxed);
      if (a >= n_a)
        break;
      for (int s = 0; s < N_STRATEGIES; ++s) {
        ts_pool_b(s, *STRATEGIES[static_cast<std::size_t>(s)],
                  masks[static_cast<std::size_t>(s)], a, axes, meta, T);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t)
    threads.emplace_back(worker);
  for (auto &th : threads)
    th.join();
}

void compute_cs_columns(const Axes &axes, Tensor &T) {
  int n_d = axes.n_d();
  int n_a = axes.n_a();
  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0)
    n_threads = 1;
  std::atomic<int> next{0};

  auto worker = [&]() {
    std::vector<float> buf_a(static_cast<std::size_t>(n_a));
    std::vector<float> buf_b(static_cast<std::size_t>(n_a));
    std::vector<float> buf_c(static_cast<std::size_t>(n_a));
    std::span<float> a(buf_a.data(), buf_a.size());
    std::span<float> b(buf_b.data(), buf_b.size());
    std::span<float> c(buf_c.data(), buf_c.size());
    for (;;) {
      int d = next.fetch_add(1, std::memory_order_relaxed);
      if (d >= n_d)
        break;
      for (int s = 0; s < N_STRATEGIES; ++s) {
        const StrategySpec &spec = *STRATEGIES[static_cast<std::size_t>(s)];
        cs_pool(s, spec, d, T, a, b, c);
        cs_tradable(s, spec, d, T, a, b);
        cs_score(s, spec, d, T, a, b, c);
        cs_rank(s, spec, d, T, a, b, c);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t)
    threads.emplace_back(worker);
  for (auto &th : threads)
    th.join();
}

} // namespace strategy
