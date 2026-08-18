#include "feature/cs.hpp"

#include "feature/feature.hpp"
#include "feature/industry.hpp"
#include "misc/affinity.hpp"

#include <algorithm>
#include <array>
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
    if (is_finite(v))
      tmp.push_back(v);
  }
  if (tmp.size() < 2)
    return;

  float med = median_in_place(tmp);

  std::vector<float> dev;
  dev.reserve(tmp.size());
  for (float v : tmp)
    dev.push_back(std::fabs(v - med));
  float mad = median_in_place(dev);
  if (mad == 0.0f)
    return;

  float lo = med - k * mad;
  float hi = med + k * mad;
  for (float &v : x) {
    if (!is_finite(v))
      continue;
    if (v < lo)
      v = lo;
    else if (v > hi)
      v = hi;
  }
}

void z(std::span<float> x) {
  double sum = 0.0, sum2 = 0.0;
  std::size_t cnt = 0;
  for (float v : x) {
    if (!is_finite(v))
      continue;
    sum += v;
    sum2 += static_cast<double>(v) * v;
    ++cnt;
  }
  if (cnt < 2)
    return;
  double mean = sum / cnt;
  double var = sum2 / cnt - mean * mean;
  if (var <= 0.0)
    return;
  double sd = std::sqrt(var);
  for (float &v : x) {
    if (!is_finite(v))
      continue;
    v = static_cast<float>((static_cast<double>(v) - mean) / sd);
  }
}

void pct_rank(std::span<float> x) {
  std::size_t n = x.size();
  std::vector<std::size_t> idx;
  idx.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (is_finite(x[i]))
      idx.push_back(i);
  }
  if (idx.empty())
    return;

  std::sort(idx.begin(), idx.end(),
            [&](std::size_t a, std::size_t b) { return x[a] < x[b]; });

  std::size_t m = idx.size();
  std::size_t i = 0;
  while (i < m) {
    std::size_t j = i + 1;
    while (j < m && x[idx[j]] == x[idx[i]])
      ++j;
    float avg_rank = static_cast<float>(i + 1 + j) * 0.5f;
    float pct = (m > 1) ? (avg_rank - 1.0f) / static_cast<float>(m - 1)
                        : 0.5f;
    for (std::size_t k = i; k < j; ++k)
      x[idx[k]] = pct;
    i = j;
  }
}

void winsorize_quantile(std::span<float> x, float lo_pct, float hi_pct) {
  std::vector<float> tmp;
  tmp.reserve(x.size());
  for (float v : x)
    if (is_finite(v))
      tmp.push_back(v);
  std::size_t n = tmp.size();
  if (n < 2)
    return;
  std::sort(tmp.begin(), tmp.end());
  std::size_t lo_k = static_cast<std::size_t>(std::floor(lo_pct * static_cast<double>(n - 1)));
  std::size_t hi_k = static_cast<std::size_t>(std::ceil(hi_pct * static_cast<double>(n - 1)));
  if (lo_k > n - 1)
    lo_k = n - 1;
  if (hi_k > n - 1)
    hi_k = n - 1;
  float lo = tmp[lo_k], hi = tmp[hi_k];
  for (float &v : x) {
    if (!is_finite(v))
      continue;
    if (v < lo)
      v = lo;
    else if (v > hi)
      v = hi;
  }
}

// 行业 + log(mcap) 截面中性化 (Frisch-Waugh-Lovell 等价):
//   y ~ 1 + log(mcap) + 行业 dummy 的残差 == 先行业内 demean(y, logmc),
//   再 demean 后 y 对 demean 后 logmc 做标量回归取残差. O(n) 且残差恒等.
//   y/logmc 任一 NaN, 或 industry 越界 → 该样本不参与, 残差留 NaN (下游均值填充).
//   industry id 0 (未知) 作为独立分组参与 (等价 full OLS 里 0=baseline 的残差).
void neutralize(std::span<float> y, std::span<const float> logmc,
                std::span<const float> industry) {
  assert(y.size() == logmc.size());
  assert(y.size() == industry.size());
  constexpr int K = static_cast<int>(SW2021_L1_COUNT);
  std::array<double, SW2021_L1_COUNT> sy{}, sl{}, cnt{};
  for (std::size_t a = 0; a < y.size(); ++a) {
    if (!is_finite(y[a]) || !is_finite(logmc[a]))
      continue;
    int id = static_cast<int>(industry[a]);
    if (id < 0 || id >= K)
      continue;
    sy[id] += static_cast<double>(y[a]);
    sl[id] += static_cast<double>(logmc[a]);
    cnt[id] += 1.0;
  }
  std::array<float, SW2021_L1_COUNT> my{}, ml{};
  for (int i = 0; i < K; ++i) {
    if (cnt[i] > 0.0) {
      my[i] = static_cast<float>(sy[i] / cnt[i]);
      ml[i] = static_cast<float>(sl[i] / cnt[i]);
    }
  }
  // demean 后标量回归: b = Σ(y_dm·lm_dm) / Σ(lm_dm²)
  double num = 0.0, den = 0.0;
  for (std::size_t a = 0; a < y.size(); ++a) {
    if (!is_finite(y[a]) || !is_finite(logmc[a]))
      continue;
    int id = static_cast<int>(industry[a]);
    if (id < 0 || id >= K)
      continue;
    if (cnt[id] <= 0.0)
      continue;
    double ydm = static_cast<double>(y[a]) - my[id];
    double ldm = static_cast<double>(logmc[a]) - ml[id];
    num += ydm * ldm;
    den += ldm * ldm;
  }
  float b = (den > 0.0) ? static_cast<float>(num / den) : 0.0f;
  for (std::size_t a = 0; a < y.size(); ++a) {
    if (!is_finite(y[a]) || !is_finite(logmc[a])) {
      y[a] = std::nanf("");
      continue;
    }
    int id = static_cast<int>(industry[a]);
    if (id < 0 || id >= K || cnt[id] <= 0.0) {
      y[a] = std::nanf("");
      continue;
    }
    float ydm = y[a] - my[id];
    float ldm = logmc[a] - ml[id];
    y[a] = ydm - b * ldm;
  }
}

// 截面参与集裁剪: 把 D 当日不在市 (未上市 ∨ 已退市) 的 a 置 NaN.
//   bar1d / shares 网格是 per-A ffill 的, 退市后 close 与 total_shares 永久冻结在
//   最后一笔 ⇒ mcap_raw / pe_raw 等估值 raw 退市后依旧 finite, 是陈旧僵尸值.
//   实测 2024 年后每日约 150-200 只这类僵尸股, EP 低到 -26 (退市前市值只剩几千万
//   而亏损照旧), 数量超过 1% 缩尾名额 ⇒ 1% 分位从在市集合的 -0.43 被拉到 -3.18,
//   OLS 斜率与行业均值随之失真: 中性EP 对果仁日均秩相关 0.996 → 0.568.
//   用 list_age / delist_age 而非 pool_b: 后者还含交易所/板块/行业白名单与两融口径,
//   属策略选股范围, 不该缩小因子的统计母集.
namespace {
void mask_offmarket(int d, Tensor &T, std::span<float> y, std::span<float> tmp) {
  T.gather_cs_row(F::list_age, d, tmp);
  for (std::size_t a = 0; a < y.size(); ++a)
    if (!is_finite(tmp[a]))
      y[a] = std::nanf("");
  T.gather_cs_row(F::delist_age, d, tmp);
  for (std::size_t a = 0; a < y.size(); ++a)
    if (is_finite(tmp[a]))
      y[a] = std::nanf("");
}
} // namespace

void factor_pipeline(int d, F src, F dst, bool invert, Tensor &T, CsBufs &bufs) {
  std::span<float> buf = bufs.a;
  T.gather_cs_row(src, d, buf);
  mask_offmarket(d, T, buf, bufs.b);
  if (invert) {
    for (float &v : buf) {
      if (!is_finite(v) || v == 0.0f)
        v = std::nanf("");
      else
        v = 1.0f / v;
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

// 中性化因子流水: raw → [1/x] → winsorize_quantile(1%,99%) → neutralize(行业+log mcap)
//   → z → pct_rank → 坪值填充 → scatter. 输出 ∈[0,1], 与 factor_pipeline 下游兼容.
//   buf 分配: a=y(残差), b=log mcap, c=industry.
void neutral_pipeline(int d, F src, F dst, bool invert, Tensor &T, CsBufs &b) {
  std::span<float> y = b.a;
  std::span<float> lm = b.b;
  std::span<float> ind = b.c;

  T.gather_cs_row(src, d, y);
  mask_offmarket(d, T, y, lm);
  if (invert) {
    for (float &v : y) {
      if (!is_finite(v) || v == 0.0f)
        v = std::nanf("");
      else
        v = 1.0f / v;
    }
  }
  winsorize_quantile(y, 0.01f, 0.99f);

  T.gather_cs_row(F::mcap_raw, d, lm);
  for (float &v : lm) {
    if (!is_finite(v) || v <= 0.0f)
      v = std::nanf("");
    else
      v = std::log(v);
  }
  T.gather_cs_row(F::industry_l1, d, ind);

  neutralize(y, lm, ind);
  z(y);
  pct_rank(y);

  double sum = 0.0;
  std::size_t cnt = 0;
  for (float v : y) {
    if (!is_finite(v))
      continue;
    sum += v;
    ++cnt;
  }
  if (cnt == 0) {
    std::fill(y.begin(), y.end(), 0.0f);
    T.scatter_cs_row(dst, d, std::span<const float>(y.data(), y.size()));
    return;
  }
  float mean = static_cast<float>(sum / static_cast<double>(cnt));
  for (float &v : y)
    if (!is_finite(v))
      v = mean;

  T.scatter_cs_row(dst, d, std::span<const float>(y.data(), y.size()));
}

// ============================================================================
// 通用 dispatcher: per-D 并行; 每 worker 拿 thread-local 3 buf (length=n_a),
//   串行调 FEATURES[] 内 axis==CrossSection 的 compute_cs(d, ...).
// ============================================================================
void compute_cs(const Axes &axes, Tensor &T) {
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
    CsBufs bufs{
        std::span<float>(buf_a.data(), buf_a.size()),
        std::span<float>(buf_b.data(), buf_b.size()),
        std::span<float>(buf_c.data(), buf_c.size()),
    };
    for (;;) {
      int d = next.fetch_add(1, std::memory_order_relaxed);
      if (d >= n_d)
        break;
      for (std::size_t f = 0; f < FEATURES.size(); ++f) {
        const FeatureMeta &fm = FEATURES[f];
        if (fm.axis != Axis::CrossSection)
          continue;
        if (!fm.compute_cs)
          continue;
        fm.compute_cs(d, axes, T, bufs);
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

} // namespace feature
