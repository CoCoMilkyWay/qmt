#include "feature/cs.hpp"

#include "feature/feature.hpp"
#include "misc/affinity.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <span>
#include <thread>
#include <vector>

namespace feature {

// ============================================================================
// 通用 kernel (跳 NaN, 原地修改)
// ============================================================================

namespace {

float median_in_place(std::vector<float> &tmp) {
  assert(!tmp.empty());
  std::size_t n = tmp.size();
  std::nth_element(tmp.begin(), tmp.begin() + n / 2, tmp.end());
  float m = tmp[n / 2];
  if (n % 2 == 0) {
    auto max_lo = std::max_element(tmp.begin(), tmp.begin() + n / 2);
    m = (m + *max_lo) * 0.5f;
  }
  return m;
}

} // namespace

void winsor_mad(std::span<float> x, float k) {
  std::vector<float> tmp;
  tmp.reserve(x.size());
  for (float v : x) {
    if (is_finite(v)) tmp.push_back(v);
  }
  if (tmp.size() < 2) return;

  float med = median_in_place(tmp);

  std::vector<float> dev;
  dev.reserve(tmp.size());
  for (float v : tmp) dev.push_back(std::fabs(v - med));
  float mad = median_in_place(dev);
  if (mad == 0.0f) return;

  float lo = med - k * mad;
  float hi = med + k * mad;
  for (float &v : x) {
    if (!is_finite(v)) continue;
    if (v < lo) v = lo;
    else if (v > hi) v = hi;
  }
}

void z(std::span<float> x) {
  double sum = 0.0, sum2 = 0.0;
  std::size_t cnt = 0;
  for (float v : x) {
    if (!is_finite(v)) continue;
    sum += v;
    sum2 += static_cast<double>(v) * v;
    ++cnt;
  }
  if (cnt < 2) return;
  double mean = sum / cnt;
  double var = sum2 / cnt - mean * mean;
  if (var <= 0.0) return;
  double sd = std::sqrt(var);
  for (float &v : x) {
    if (!is_finite(v)) continue;
    v = static_cast<float>((static_cast<double>(v) - mean) / sd);
  }
}

void pct_rank(std::span<float> x) {
  std::size_t n = x.size();
  std::vector<std::size_t> idx;
  idx.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (is_finite(x[i])) idx.push_back(i);
  }
  if (idx.empty()) return;

  std::sort(idx.begin(), idx.end(),
            [&](std::size_t a, std::size_t b) { return x[a] < x[b]; });

  std::size_t m = idx.size();
  std::size_t i = 0;
  while (i < m) {
    std::size_t j = i + 1;
    while (j < m && x[idx[j]] == x[idx[i]]) ++j;
    float avg_rank = static_cast<float>(i + 1 + j) * 0.5f;
    float pct = (m > 1) ? (avg_rank - 1.0f) / static_cast<float>(m - 1)
                        : 0.5f;
    for (std::size_t k = i; k < j; ++k) x[idx[k]] = pct;
    i = j;
  }
}

void factor_pipeline(int d, F src, F dst, bool invert, Tensor &T,
                     std::span<float> buf) {
  T.gather_cs_row(src, d, buf);
  if (invert) {
    for (float &v : buf) {
      if (!is_finite(v) || v == 0.0f) v = std::nanf("");
      else v = 1.0f / v;
    }
  }
  winsor_mad(buf, 3.0f);
  z(buf);
  pct_rank(buf);

  double sum = 0.0;
  std::size_t cnt = 0;
  for (float v : buf) {
    if (!is_finite(v))
      continue;
    sum += v;
    ++cnt;
  }
  if (cnt == 0) {
    // 全截面都缺失时没有均值可算; 填同一常数只表达"无横截面信息",
    // 不制造排序差异. 局部缺失仍走真实截面均值.
    std::fill(buf.begin(), buf.end(), 0.0f);
    T.scatter_cs_row(dst, d, std::span<const float>(buf.data(), buf.size()));
    return;
  }
  float mean = static_cast<float>(sum / static_cast<double>(cnt));
  for (float &v : buf)
    if (!is_finite(v))
      v = mean;

  T.scatter_cs_row(dst, d, std::span<const float>(buf.data(), buf.size()));
}

// ============================================================================
// 通用 dispatcher: per-D 并行; 每 worker 拿 thread-local 3 buf (length=n_a),
//   串行调 FEATURES[] 内 axis==CrossSection 的 compute_cs(d, ...).
// ============================================================================
void compute_cs(const Axes &axes, Tensor &T) {
  int n_d = axes.n_d();
  int n_a = axes.n_a();

  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  std::atomic<int> next{0};

  auto worker = [&]() {
    std::vector<float> buf_a(static_cast<std::size_t>(n_a));
    std::vector<float> buf_b(static_cast<std::size_t>(n_a));
    std::vector<float> buf_c(static_cast<std::size_t>(n_a));
    CsBufs bufs{
        std::span<float>(buf_a.data(), buf_a.size()),
        std::span<float>(buf_b.data(), buf_b.size()),
        std::span<float>(buf_c.data(), buf_c.size()),
    };
    for (;;) {
      int d = next.fetch_add(1, std::memory_order_relaxed);
      if (d >= n_d) break;
      for (std::size_t f = 0; f < FEATURES.size(); ++f) {
        const FeatureMeta &fm = FEATURES[f];
        if (fm.axis != Axis::CrossSection) continue;
        if (!fm.compute_cs) continue;
        fm.compute_cs(d, axes, T, bufs);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();
}

} // namespace feature
