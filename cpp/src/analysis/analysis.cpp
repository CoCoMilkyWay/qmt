#include "analysis/analysis.hpp"

#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/affinity.hpp"
#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/timer.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <thread>
#include <vector>

namespace analysis {

namespace fs = std::filesystem;

namespace {

using feature::F;
using feature::FEATURES;
using feature::is_finite;
using feature::Kind;

inline int find_d(const feature::Axes &axes, std::string_view yyyymmdd,
                  bool floor) {
  if (floor) return axes.floor_date(yyyymmdd);
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), yyyymmdd);
  return (it == axes.dates.end())
             ? -1
             : static_cast<int>(std::distance(axes.dates.begin(), it));
}

// Pearson (跳 NaN 与非 mask), 返回 [-1, 1] 或 NaN
float pearson_masked(const float *x, const float *y, int n,
                     const float *pool_mask) {
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
  int cnt = 0;
  for (int i = 0; i < n; ++i) {
    if (pool_mask[i] <= 0.5f) continue;
    float xi = x[i], yi = y[i];
    if (!is_finite(xi) || !is_finite(yi)) continue;
    double xd = xi, yd = yi;
    sx += xd;
    sy += yd;
    sxx += xd * xd;
    syy += yd * yd;
    sxy += xd * yd;
    ++cnt;
  }
  if (cnt < 5) return std::nanf("");
  double mx = sx / cnt, my = sy / cnt;
  double cov = sxy / cnt - mx * my;
  double vx = sxx / cnt - mx * mx;
  double vy = syy / cnt - my * my;
  if (vx <= 0.0 || vy <= 0.0) return std::nanf("");
  return static_cast<float>(cov / std::sqrt(vx * vy));
}

// 当日 top-K (within pool, finite score) 集合 (升序 a 索引数组).
//   K = BT_HOLD_N; 不足 K 时返回所有可用. score 用于排序.
void compute_top_k(const float *score, const float *pool_mask, int n,
                   int K, std::vector<int> &out) {
  out.clear();
  std::vector<std::pair<float, int>> tmp;
  tmp.reserve(static_cast<std::size_t>(K) * 2);
  for (int a = 0; a < n; ++a) {
    if (pool_mask[a] <= 0.5f) continue;
    float s = score[a];
    if (!is_finite(s)) continue;
    tmp.emplace_back(s, a);
  }
  if (tmp.empty()) return;
  int k = std::min(K, static_cast<int>(tmp.size()));
  std::nth_element(tmp.begin(), tmp.begin() + k, tmp.end(),
                   [](const auto &x, const auto &y) {
                     return x.first > y.first;
                   });
  out.reserve(static_cast<std::size_t>(k));
  for (int i = 0; i < k; ++i) out.push_back(tmp[i].second);
  std::sort(out.begin(), out.end());
}

// Jaccard turnover: 1 - |A ∩ B| / max(|A|, |B|).
float jaccard_turnover(const std::vector<int> &a, const std::vector<int> &b) {
  if (a.empty() || b.empty()) return std::nanf("");
  int inter = 0;
  std::size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      ++inter;
      ++i;
      ++j;
    } else if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  int denom = std::max(static_cast<int>(a.size()), static_cast<int>(b.size()));
  return 1.0f - static_cast<float>(inter) / static_cast<float>(denom);
}

} // namespace

double run(const feature::Axes &axes, const feature::Tensor &T) {
  misc::Timer t("[analysis] run");
  auto t0 = std::chrono::high_resolution_clock::now();

  int bt_d_lo = find_d(axes, ::config::BACKTEST_START_DATE, false);
  int bt_d_hi_inc = find_d(axes, ::config::BACKTEST_END_DATE, true);
  assert(bt_d_lo >= 0 && bt_d_hi_inc >= bt_d_lo &&
         bt_d_hi_inc < axes.n_d());
  int bt_d_hi = bt_d_hi_inc + 1;
  int n_d_bt = bt_d_hi - bt_d_lo;
  int n_a = axes.n_a();

  // ---- 收集 factor 列 (Kind::Factor) --------------------------------------
  std::vector<F> factors;
  std::vector<const char *> factor_names;
  for (std::size_t i = 0; i < FEATURES.size(); ++i) {
    if (FEATURES[i].kind == Kind::Factor) {
      factors.push_back(static_cast<F>(i));
      factor_names.push_back(FEATURES[i].name);
    }
  }
  int n_factor = static_cast<int>(factors.size());
  int Q = ::config::N_QUANTILES;
  int K = ::config::BT_HOLD_N;

  // ---- 输出缓冲 ------------------------------------------------------------
  std::vector<std::int32_t> dates_out(n_d_bt);
  for (int i = 0; i < n_d_bt; ++i) dates_out[i] = bt_d_lo + i;

  // factor_ic[f * n_d_bt + i], factor_turnover 同
  std::vector<float> factor_ic(static_cast<std::size_t>(n_factor) *
                                   static_cast<std::size_t>(n_d_bt),
                               std::nanf(""));
  std::vector<float> factor_turnover(static_cast<std::size_t>(n_factor) *
                                         static_cast<std::size_t>(n_d_bt),
                                     std::nanf(""));
  std::vector<float> score_ic(n_d_bt, std::nanf(""));
  std::vector<float> score_turnover(n_d_bt, std::nanf(""));
  std::vector<float> pool_ret(n_d_bt, std::nanf(""));
  std::vector<float> quantile_ret(static_cast<std::size_t>(Q) *
                                      static_cast<std::size_t>(n_d_bt),
                                  std::nanf(""));

  // factor_corr 用 atomic-合并的累加器: per-pair sx, sy, sxx, syy, sxy, n
  //   pool 全周期; 每对 (i, j) 一组累加器, 由 worker 局部累 + 末段合并.
  struct PairAcc {
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    long long n = 0;
  };
  std::vector<std::vector<PairAcc>> corr_per_thread; // [thread][i*nf + j]

  // ---- per-D 工作单元 ------------------------------------------------------
  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  corr_per_thread.assign(n_threads,
                         std::vector<PairAcc>(static_cast<std::size_t>(
                             n_factor * n_factor)));
  std::atomic<int> next{0};

  auto worker = [&](unsigned tid) {
    std::vector<float> buf_pool(static_cast<std::size_t>(n_a));
    std::vector<float> buf_dr_next(static_cast<std::size_t>(n_a));
    std::vector<float> buf_score(static_cast<std::size_t>(n_a));
    std::vector<float> buf_dr_today(static_cast<std::size_t>(n_a));
    std::vector<std::vector<float>> buf_factors(
        static_cast<std::size_t>(n_factor),
        std::vector<float>(static_cast<std::size_t>(n_a)));
    std::vector<int> top_today, top_prev;
    std::vector<std::vector<int>> top_today_per_factor(
        static_cast<std::size_t>(n_factor));
    std::vector<std::vector<int>> top_prev_per_factor(
        static_cast<std::size_t>(n_factor));

    auto &corr_local = corr_per_thread[tid];

    for (;;) {
      int i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= n_d_bt) break;
      int d = bt_d_lo + i;
      int d_next = d + 1;

      T.gather_cs_row(F::pool, d, std::span<float>(buf_pool));
      T.gather_cs_row(F::factor_score, d,
                      std::span<float>(buf_score));
      T.gather_cs_row(F::daily_return, d,
                      std::span<float>(buf_dr_today));
      if (d_next < axes.n_d()) {
        T.gather_cs_row(F::daily_return, d_next,
                        std::span<float>(buf_dr_next));
      } else {
        std::fill(buf_dr_next.begin(), buf_dr_next.end(), std::nanf(""));
      }
      for (int f = 0; f < n_factor; ++f) {
        T.gather_cs_row(factors[static_cast<std::size_t>(f)], d,
                        std::span<float>(buf_factors[
                            static_cast<std::size_t>(f)]));
      }

      // pool_ret: 等权 pool[d] 内 daily_return[d] (与 backtest pool_nav 同口径)
      double dr_sum = 0.0;
      int dr_n = 0;
      for (int a = 0; a < n_a; ++a) {
        if (buf_pool[a] <= 0.5f) continue;
        float r = buf_dr_today[a];
        if (!is_finite(r)) continue;
        dr_sum += r;
        ++dr_n;
      }
      pool_ret[i] = (dr_n > 0)
                        ? static_cast<float>(dr_sum / dr_n)
                        : std::nanf("");

      // per-factor IC vs t+1 收益 (forward predictive)
      for (int f = 0; f < n_factor; ++f) {
        factor_ic[static_cast<std::size_t>(f) *
                      static_cast<std::size_t>(n_d_bt) +
                  static_cast<std::size_t>(i)] =
            pearson_masked(buf_factors[static_cast<std::size_t>(f)].data(),
                           buf_dr_next.data(), n_a, buf_pool.data());
      }
      // 聚合 factor_score IC
      score_ic[i] =
          pearson_masked(buf_score.data(), buf_dr_next.data(), n_a,
                         buf_pool.data());

      // per-factor top-K turnover (vs i-1)
      for (int f = 0; f < n_factor; ++f) {
        compute_top_k(buf_factors[static_cast<std::size_t>(f)].data(),
                      buf_pool.data(), n_a, K,
                      top_today_per_factor[static_cast<std::size_t>(f)]);
      }
      // score top-K
      compute_top_k(buf_score.data(), buf_pool.data(), n_a, K, top_today);

      // turnover 需要前一日 top-K — worker 间无序: 这里只能是 same-thread 累计.
      //   解决: 让 worker 一次顺序处理多个 i (chunk)? 简化方案:
      //     turnover 在末段单线程补算, worker 仅写当日 top-K 列表到全局.
      //   为避免锁, 用 i_to_top buffer (容量随分配可能很大), 这里改用单线程后处理.

      // 因子相关性: 累加 (factor_i, factor_j) 在 pool 内的 (sum, sumsq, sumxy, n)
      for (int fi = 0; fi < n_factor; ++fi) {
        const float *xi = buf_factors[static_cast<std::size_t>(fi)].data();
        for (int fj = fi; fj < n_factor; ++fj) {
          const float *xj = buf_factors[static_cast<std::size_t>(fj)].data();
          PairAcc &acc =
              corr_local[static_cast<std::size_t>(fi) *
                             static_cast<std::size_t>(n_factor) +
                         static_cast<std::size_t>(fj)];
          for (int a = 0; a < n_a; ++a) {
            if (buf_pool[a] <= 0.5f) continue;
            float vi = xi[a], vj = xj[a];
            if (!is_finite(vi) || !is_finite(vj)) continue;
            double di = vi, dj = vj;
            acc.sx += di;
            acc.sy += dj;
            acc.sxx += di * di;
            acc.syy += dj * dj;
            acc.sxy += di * dj;
            ++acc.n;
          }
        }
      }

      // quantile_ret: 按 factor_score 在 pool 内 rank 分桶, 算桶内 daily_return[d+1] 等权均值
      std::vector<std::pair<float, int>> ranked;
      ranked.reserve(static_cast<std::size_t>(n_a));
      for (int a = 0; a < n_a; ++a) {
        if (buf_pool[a] <= 0.5f) continue;
        float s = buf_score[a];
        if (!is_finite(s)) continue;
        ranked.emplace_back(s, a);
      }
      if (!ranked.empty()) {
        std::sort(ranked.begin(), ranked.end(),
                  [](const auto &x, const auto &y) {
                    return x.first < y.first;
                  });
        int total = static_cast<int>(ranked.size());
        for (int q = 0; q < Q; ++q) {
          int lo = (total * q) / Q;
          int hi = (total * (q + 1)) / Q;
          double sum = 0.0;
          int n = 0;
          for (int k = lo; k < hi; ++k) {
            int a = ranked[static_cast<std::size_t>(k)].second;
            float r = buf_dr_next[a];
            if (!is_finite(r)) continue;
            sum += r;
            ++n;
          }
          quantile_ret[static_cast<std::size_t>(q) *
                           static_cast<std::size_t>(n_d_bt) +
                       static_cast<std::size_t>(i)] =
              n > 0 ? static_cast<float>(sum / n) : std::nanf("");
        }
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned tid = 0; tid < n_threads; ++tid)
    threads.emplace_back(worker, tid);
  for (auto &th : threads) th.join();

  // ---- factor_corr 合并 ----------------------------------------------------
  std::vector<float> factor_corr(static_cast<std::size_t>(n_factor) *
                                     static_cast<std::size_t>(n_factor),
                                 std::nanf(""));
  for (int fi = 0; fi < n_factor; ++fi) {
    for (int fj = fi; fj < n_factor; ++fj) {
      PairAcc agg;
      for (unsigned tid = 0; tid < n_threads; ++tid) {
        const PairAcc &p =
            corr_per_thread[tid][static_cast<std::size_t>(fi) *
                                     static_cast<std::size_t>(n_factor) +
                                 static_cast<std::size_t>(fj)];
        agg.sx += p.sx;
        agg.sy += p.sy;
        agg.sxx += p.sxx;
        agg.syy += p.syy;
        agg.sxy += p.sxy;
        agg.n += p.n;
      }
      float c = std::nanf("");
      if (agg.n >= 5) {
        double mx = agg.sx / static_cast<double>(agg.n);
        double my = agg.sy / static_cast<double>(agg.n);
        double cov = agg.sxy / static_cast<double>(agg.n) - mx * my;
        double vx = agg.sxx / static_cast<double>(agg.n) - mx * mx;
        double vy = agg.syy / static_cast<double>(agg.n) - my * my;
        if (vx > 0 && vy > 0)
          c = static_cast<float>(cov / std::sqrt(vx * vy));
      }
      factor_corr[static_cast<std::size_t>(fi) *
                      static_cast<std::size_t>(n_factor) +
                  static_cast<std::size_t>(fj)] = c;
      factor_corr[static_cast<std::size_t>(fj) *
                      static_cast<std::size_t>(n_factor) +
                  static_cast<std::size_t>(fi)] = c;
    }
  }

  // ---- top-K turnover 串行后处理 (依赖 i-1, 多线程不便) -------------------
  std::vector<float> buf_pool(static_cast<std::size_t>(n_a));
  std::vector<float> buf_score(static_cast<std::size_t>(n_a));
  std::vector<std::vector<float>> buf_factors_seq(
      static_cast<std::size_t>(n_factor),
      std::vector<float>(static_cast<std::size_t>(n_a)));
  std::vector<int> prev_score_top, today_score_top;
  std::vector<std::vector<int>> prev_factor_top(
      static_cast<std::size_t>(n_factor));
  std::vector<std::vector<int>> today_factor_top(
      static_cast<std::size_t>(n_factor));
  for (int i = 0; i < n_d_bt; ++i) {
    int d = bt_d_lo + i;
    T.gather_cs_row(F::pool, d, std::span<float>(buf_pool));
    T.gather_cs_row(F::factor_score, d, std::span<float>(buf_score));
    for (int f = 0; f < n_factor; ++f) {
      T.gather_cs_row(factors[static_cast<std::size_t>(f)], d,
                      std::span<float>(buf_factors_seq[
                          static_cast<std::size_t>(f)]));
    }
    compute_top_k(buf_score.data(), buf_pool.data(), n_a, K, today_score_top);
    for (int f = 0; f < n_factor; ++f) {
      compute_top_k(buf_factors_seq[static_cast<std::size_t>(f)].data(),
                    buf_pool.data(), n_a, K,
                    today_factor_top[static_cast<std::size_t>(f)]);
    }
    if (i > 0) {
      score_turnover[i] = jaccard_turnover(prev_score_top, today_score_top);
      for (int f = 0; f < n_factor; ++f) {
        factor_turnover[static_cast<std::size_t>(f) *
                            static_cast<std::size_t>(n_d_bt) +
                        static_cast<std::size_t>(i)] =
            jaccard_turnover(prev_factor_top[static_cast<std::size_t>(f)],
                             today_factor_top[static_cast<std::size_t>(f)]);
      }
    }
    prev_score_top.swap(today_score_top);
    for (int f = 0; f < n_factor; ++f) {
      prev_factor_top[static_cast<std::size_t>(f)].swap(
          today_factor_top[static_cast<std::size_t>(f)]);
    }
  }

  // ---- 写盘 ----------------------------------------------------------------
  fs::path out = misc::git_root() / "output" / "analysis";
  fs::create_directories(out);

  {
    std::size_t shape[1] = {static_cast<std::size_t>(n_d_bt)};
    misc::write_npy_i4(out / "dates.npy",
                       std::span<const std::int32_t>(dates_out.data(),
                                                     dates_out.size()),
                       std::span<const std::size_t>(shape, 1));
  }
  {
    std::size_t shape[2] = {static_cast<std::size_t>(n_factor),
                            static_cast<std::size_t>(n_d_bt)};
    misc::write_npy_f4(out / "factor_ic.npy",
                       std::span<const float>(factor_ic.data(),
                                              factor_ic.size()),
                       std::span<const std::size_t>(shape, 2));
    misc::write_npy_f4(out / "factor_turnover.npy",
                       std::span<const float>(factor_turnover.data(),
                                              factor_turnover.size()),
                       std::span<const std::size_t>(shape, 2));
  }
  {
    std::size_t shape[2] = {static_cast<std::size_t>(n_factor),
                            static_cast<std::size_t>(n_factor)};
    misc::write_npy_f4(out / "factor_corr.npy",
                       std::span<const float>(factor_corr.data(),
                                              factor_corr.size()),
                       std::span<const std::size_t>(shape, 2));
  }
  {
    std::size_t shape[1] = {static_cast<std::size_t>(n_d_bt)};
    misc::write_npy_f4(out / "score_ic.npy",
                       std::span<const float>(score_ic.data(),
                                              score_ic.size()),
                       std::span<const std::size_t>(shape, 1));
    misc::write_npy_f4(out / "score_turnover.npy",
                       std::span<const float>(score_turnover.data(),
                                              score_turnover.size()),
                       std::span<const std::size_t>(shape, 1));
    misc::write_npy_f4(out / "pool_ret.npy",
                       std::span<const float>(pool_ret.data(),
                                              pool_ret.size()),
                       std::span<const std::size_t>(shape, 1));
  }
  {
    std::size_t shape[2] = {static_cast<std::size_t>(Q),
                            static_cast<std::size_t>(n_d_bt)};
    misc::write_npy_f4(out / "quantile_ret.npy",
                       std::span<const float>(quantile_ret.data(),
                                              quantile_ret.size()),
                       std::span<const std::size_t>(shape, 2));
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = t1 - t0;
  return dur.count();
}

} // namespace analysis
