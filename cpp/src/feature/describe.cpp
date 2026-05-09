#include "feature/describe.hpp"

#include "feature/feature.hpp"
#include "config.hpp"
#include "misc/affinity.hpp"
#include "misc/fs.hpp"
#include "misc/timer.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace feature {

namespace fs = std::filesystem;

namespace {

// ---- 单个 (feature, year_bucket) 的统计输出 ----------------------------------
//   计数全部基于 n_total: n_zero + n_pos + n_neg + n_pinf + n_ninf + n_nan == n_total
//   n_finite (= n_zero + n_pos + n_neg) 不单独存, 输出时按 % 派生.
struct Stats {
  std::size_t n_total;
  std::size_t n_zero;
  std::size_t n_pos;   // finite & > 0
  std::size_t n_neg;   // finite & < 0
  std::size_t n_pinf;  // +inf
  std::size_t n_ninf;  // -inf
  std::size_t n_nan;   // NaN
  float mean;
  float std_;
  float min_;
  float p5;
  float p25;
  float p50;
  float p75;
  float p95;
  float max_;
};

inline Stats nan_stats(std::size_t n_total, std::size_t n_pinf,
                       std::size_t n_ninf, std::size_t n_nan) {
  float nan = std::nanf("");
  Stats z{};
  z.n_total = n_total;
  z.n_pinf  = n_pinf;
  z.n_ninf  = n_ninf;
  z.n_nan   = n_nan;
  z.mean = z.std_ = z.min_ = z.p5 = z.p25 = z.p50 = z.p75 = z.p95 = z.max_ = nan;
  return z;
}

// -ffast-math 下 std::isinf/isnan UB; 复用 IEEE-754 bit-pattern 判定:
//   exp 全 1 + mantissa==0 ⇒ ±inf (sign 决 +/-); exp 全 1 + mantissa!=0 ⇒ NaN
inline void classify_nonfinite(float v, std::size_t &n_pinf,
                               std::size_t &n_ninf, std::size_t &n_nan) {
  std::uint32_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
  if ((bits & 0x007fffffu) != 0) { ++n_nan; return; }
  if (bits & 0x80000000u)        { ++n_ninf; return; }
  ++n_pinf;
}

// 收集 [d_lo, d_hi) × all-A 的 finite 值, 排序后算分位数 / mean / std.
//   max 样本数 = n_a * (d_hi - d_lo); 41 feature × 11 year × 5847 a × ~245 d
//   单次 ~1.4M float / 5.6MB heap, scratch 由调用方传入复用 (并行下每线程私有).
Stats compute_stats(const std::vector<float> &mat, int n_a, int n_d, int d_lo,
                    int d_hi, std::vector<float> &scratch) {
  assert(d_lo >= 0 && d_hi <= n_d && d_lo <= d_hi);
  std::size_t span_d = static_cast<std::size_t>(d_hi - d_lo);
  std::size_t n_total = static_cast<std::size_t>(n_a) * span_d;

  std::size_t n_zero = 0, n_pos = 0, n_neg = 0;
  std::size_t n_pinf = 0, n_ninf = 0, n_nan = 0;

  scratch.clear();
  scratch.reserve(n_total);
  for (int a = 0; a < n_a; ++a) {
    const float *row = mat.data() +
                       static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d);
    for (int d = d_lo; d < d_hi; ++d) {
      float v = row[d];
      if (is_finite(v)) {
        scratch.push_back(v);
        if (v > 0.0f)      ++n_pos;
        else if (v < 0.0f) ++n_neg;
        else               ++n_zero; // 含 +0 / -0
      } else {
        classify_nonfinite(v, n_pinf, n_ninf, n_nan);
      }
    }
  }

  if (scratch.empty()) return nan_stats(n_total, n_pinf, n_ninf, n_nan);

  Stats s;
  s.n_total = n_total;
  s.n_zero  = n_zero;
  s.n_pos   = n_pos;
  s.n_neg   = n_neg;
  s.n_pinf  = n_pinf;
  s.n_ninf  = n_ninf;
  s.n_nan   = n_nan;

  std::sort(scratch.begin(), scratch.end());
  s.min_ = scratch.front();
  s.max_ = scratch.back();

  auto pct = [&](float p) -> float {
    float idx = p * static_cast<float>(scratch.size() - 1);
    std::size_t lo = static_cast<std::size_t>(idx);
    std::size_t hi = std::min(lo + 1, scratch.size() - 1);
    float frac = idx - static_cast<float>(lo);
    return scratch[lo] * (1.0f - frac) + scratch[hi] * frac;
  };
  s.p5  = pct(0.05f);
  s.p25 = pct(0.25f);
  s.p50 = pct(0.50f);
  s.p75 = pct(0.75f);
  s.p95 = pct(0.95f);

  // double 累加避 -O2 下 float kahan 不稳定 (mean/var 后被 cast 回 float)
  double sum = 0.0, sum2 = 0.0;
  for (float v : scratch) {
    double d = static_cast<double>(v);
    sum  += d;
    sum2 += d * d;
  }
  double n = static_cast<double>(scratch.size());
  double mean = sum / n;
  double var = sum2 / n - mean * mean;
  s.mean = static_cast<float>(mean);
  s.std_ = static_cast<float>(std::sqrt(std::max(var, 0.0)));
  return s;
}

inline float pct_of(std::size_t k, std::size_t n_total) {
  return n_total > 0
             ? 100.0f * static_cast<float>(k) / static_cast<float>(n_total)
             : 0.0f;
}

// ---- 年桶切片 ---------------------------------------------------------------
struct YearBucket {
  std::string label; // "all" / "2015" / ...
  int d_lo;
  int d_hi;
};

std::vector<YearBucket> build_buckets(const Axes &axes) {
  std::vector<YearBucket> out;
  out.push_back({"all", 0, axes.n_d()});
  if (!::config::DESCRIBE_BY_YEAR) return out;

  int n_d = axes.n_d();
  int i = 0;
  while (i < n_d) {
    assert(axes.dates[i].size() >= 4);
    std::string y = axes.dates[i].substr(0, 4);
    int j = i;
    while (j < n_d && axes.dates[j].compare(0, 4, y) == 0) ++j;
    out.push_back({y, i, j});
    i = j;
  }
  return out;
}

// ---- 输出: stdout 等宽 + tsv -----------------------------------------------
//   旧 n_finite / nan% 替换为 7 个百分比列: %0 %+ %- %+inf %-inf %nan %finite
//   (前 6 列两两互斥, 总和 = 100%; %finite = %0 + %+ + %-)
constexpr const char *COLS[] = {
    "feature", "year", "n_total",
    "%0",      "%+",   "%-",     "%+inf", "%-inf", "%nan", "%finite",
    "mean",    "std",  "min",    "5%",    "25%",   "50%",  "75%", "95%", "max",
};
constexpr int N_COL = sizeof(COLS) / sizeof(COLS[0]);

void emit_header_stdout() {
  std::printf("\n%-14s %-6s %12s "
              "%7s %7s %7s %7s %7s %7s %7s "
              "%12s %12s %12s %12s %12s %12s %12s %12s %12s\n",
              COLS[0], COLS[1], COLS[2], COLS[3], COLS[4], COLS[5], COLS[6],
              COLS[7], COLS[8], COLS[9], COLS[10], COLS[11], COLS[12],
              COLS[13], COLS[14], COLS[15], COLS[16], COLS[17], COLS[18]);
}

void emit_row_stdout(const char *name, const std::string &year,
                     const Stats &s) {
  std::size_t n_finite = s.n_zero + s.n_pos + s.n_neg;
  std::printf("%-14s %-6s %12zu "
              "%7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f "
              "%12.4g %12.4g %12.4g %12.4g %12.4g %12.4g %12.4g %12.4g %12.4g\n",
              name, year.c_str(), s.n_total,
              pct_of(s.n_zero, s.n_total), pct_of(s.n_pos, s.n_total),
              pct_of(s.n_neg, s.n_total),  pct_of(s.n_pinf, s.n_total),
              pct_of(s.n_ninf, s.n_total), pct_of(s.n_nan, s.n_total),
              pct_of(n_finite, s.n_total),
              s.mean, s.std_, s.min_, s.p5, s.p25, s.p50, s.p75, s.p95, s.max_);
}

void emit_header_tsv(std::ofstream &os) {
  for (int i = 0; i < N_COL; ++i) {
    os << COLS[i];
    os << (i + 1 < N_COL ? '\t' : '\n');
  }
}

void emit_row_tsv(std::ofstream &os, const char *name, const std::string &year,
                  const Stats &s) {
  std::size_t n_finite = s.n_zero + s.n_pos + s.n_neg;
  os << name << '\t' << year << '\t' << s.n_total << '\t'
     << pct_of(s.n_zero, s.n_total) << '\t' << pct_of(s.n_pos, s.n_total) << '\t'
     << pct_of(s.n_neg, s.n_total)  << '\t' << pct_of(s.n_pinf, s.n_total) << '\t'
     << pct_of(s.n_ninf, s.n_total) << '\t' << pct_of(s.n_nan, s.n_total) << '\t'
     << pct_of(n_finite, s.n_total) << '\t'
     << s.mean << '\t' << s.std_ << '\t' << s.min_ << '\t'
     << s.p5 << '\t' << s.p25 << '\t' << s.p50 << '\t' << s.p75 << '\t'
     << s.p95 << '\t' << s.max_ << '\n';
}

} // namespace

void describe(const Axes &axes, const Tensor &T) {
  misc::Timer t("[feature] phase 4 describe");

  std::vector<YearBucket> buckets = build_buckets(axes);
  int n_a = axes.n_a();
  int n_d = axes.n_d();

  fs::path out_dir = misc::git_root() / "output";
  fs::create_directories(out_dir);
  fs::path tsv_path = out_dir / "feature_describe.tsv";
  std::ofstream ofs(tsv_path);
  assert(ofs.is_open());

  emit_header_stdout();
  emit_header_tsv(ofs);

  // 并行: 按 feature 派发; 每线程私有 scratch (复用跨 bucket).
  //   先把全部 (feature × bucket) 的 Stats 算到 all_stats[f][bi], 再串行 emit
  //   按 feature 顺序输出 (与 FEATURES[] / 历史日志一致).
  std::vector<std::vector<Stats>> all_stats(
      FEATURES.size(), std::vector<Stats>(buckets.size()));
  std::atomic<std::size_t> next{0};

  auto worker = [&]() {
    std::vector<float> scratch;
    for (;;) {
      std::size_t f = next.fetch_add(1, std::memory_order_relaxed);
      if (f >= FEATURES.size()) break;
      const std::vector<float> &mat = T.mats[f];
      for (std::size_t bi = 0; bi < buckets.size(); ++bi) {
        const YearBucket &b = buckets[bi];
        all_stats[f][bi] =
            compute_stats(mat, n_a, n_d, b.d_lo, b.d_hi, scratch);
      }
    }
  };

  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  if (n_threads > FEATURES.size())
    n_threads = static_cast<unsigned>(FEATURES.size());

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();

  for (std::size_t f = 0; f < FEATURES.size(); ++f) {
    const FeatureMeta &fm = FEATURES[f];
    for (std::size_t bi = 0; bi < buckets.size(); ++bi) {
      const YearBucket &b = buckets[bi];
      const Stats &s = all_stats[f][bi];
      emit_row_stdout(fm.name, b.label, s);
      emit_row_tsv(ofs, fm.name, b.label, s);
    }
  }
}

} // namespace feature
