#include "report/metrics.hpp"

#include "misc/date.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>

namespace report {

namespace {

inline bool fin(float v) { return std::isfinite(v); }

// 分组累加器 — period_stats / win_rate 共用的"期内复利 + 期内风险"计算.
struct Bucket {
  std::string key;
  std::size_t lo = 0; // [lo, hi) 半开区间, 索引到 ret / bench_ret
  std::size_t hi = 0;
};

// dates 升序 ⇒ 顺序扫一遍即可切段. key_of 决定期次粒度.
template <class KeyFn>
std::vector<Bucket> bucketize(std::span<const std::string> dates, KeyFn key_of) {
  std::vector<Bucket> out;
  for (std::size_t i = 0; i < dates.size(); ++i) {
    std::string k = key_of(dates[i]);
    if (out.empty() || out.back().key != k) {
      out.push_back(Bucket{std::move(k), i, i + 1});
    } else {
      out.back().hi = i + 1;
    }
  }
  return out;
}

// 期内复利收益: Π(1 + r) − 1, NaN 当 0 收益.
float compound(std::span<const float> ret, std::size_t lo, std::size_t hi) {
  double acc = 1.0;
  for (std::size_t i = lo; i < hi; ++i) {
    float r = ret[i];
    acc *= 1.0 + (fin(r) ? static_cast<double>(r) : 0.0);
  }
  return static_cast<float>(acc - 1.0);
}

// 期内累计净值 (起点 1.0) → 最大回撤. NaN 当 0 收益.
float span_max_dd(std::span<const float> ret, std::size_t lo, std::size_t hi) {
  double nav = 1.0;
  double peak = 1.0;
  double worst = 0.0;
  for (std::size_t i = lo; i < hi; ++i) {
    float r = ret[i];
    nav *= 1.0 + (fin(r) ? static_cast<double>(r) : 0.0);
    peak = std::max(peak, nav);
    worst = std::min(worst, (nav - peak) / peak);
  }
  return static_cast<float>(worst);
}

// "YYYYMMDD" → 所属 pandas W-SUN 周的收尾周日, 格式 "YYYYMMDD".
//   iso_encoding: 周一=1 … 周日=7 ⇒ 距本周日 7 − iso 天.
std::string week_key(const std::string &yyyymmdd) {
  std::chrono::sys_days d = misc::parse_yyyymmdd(yyyymmdd);
  unsigned iso = std::chrono::weekday(d).iso_encoding();
  return misc::fmt_yyyymmdd(d + std::chrono::days(static_cast<int>(7 - iso)));
}

} // namespace

std::vector<float> daily_returns(std::span<const float> nav) {
  assert(!nav.empty() && "daily_returns: 空 nav");
  std::vector<float> out(nav.size(), 0.0f);
  for (std::size_t i = 1; i < nav.size(); ++i) {
    assert(nav[i - 1] > 0.0f && "daily_returns: nav 必须 > 0");
    out[i] = static_cast<float>(static_cast<double>(nav[i]) /
                                    static_cast<double>(nav[i - 1]) -
                                1.0);
  }
  return out;
}

std::vector<float> drawdown_curve(std::span<const float> nav) {
  std::vector<float> out(nav.size(), 0.0f);
  double peak = -std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < nav.size(); ++i) {
    double v = nav[i];
    peak = std::max(peak, v);
    assert(peak > 0.0 && "drawdown_curve: nav 峰值必须 > 0");
    out[i] = static_cast<float>((v - peak) / peak);
  }
  return out;
}

std::vector<float> cum_nav(std::span<const float> ret) {
  std::vector<float> out(ret.size(), 0.0f);
  double acc = 1.0;
  for (std::size_t i = 0; i < ret.size(); ++i) {
    float r = ret[i];
    acc *= 1.0 + (fin(r) ? static_cast<double>(r) : 0.0);
    out[i] = static_cast<float>(acc);
  }
  return out;
}

std::vector<float> nan_cumsum(std::span<const float> x) {
  std::vector<float> out(x.size(), 0.0f);
  double acc = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (fin(x[i]))
      acc += static_cast<double>(x[i]);
    out[i] = static_cast<float>(acc);
  }
  return out;
}

std::vector<float> rolling_mean(std::span<const float> x, int w) {
  assert(w > 0 && "rolling_mean: 窗口必须 > 0");
  int min_periods = std::max(1, w / 4);
  std::vector<float> out(x.size(), std::nanf(""));
  double sum = 0.0;
  int cnt = 0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (fin(x[i])) {
      sum += static_cast<double>(x[i]);
      ++cnt;
    }
    std::size_t w_sz = static_cast<std::size_t>(w);
    if (i >= w_sz && fin(x[i - w_sz])) {
      sum -= static_cast<double>(x[i - w_sz]);
      --cnt;
    }
    if (cnt >= min_periods)
      out[i] = static_cast<float>(sum / static_cast<double>(cnt));
  }
  return out;
}

NavStats nav_stats(std::span<const float> nav) {
  NavStats s;
  s.n_days = static_cast<int>(nav.size());
  if (nav.empty())
    return s;
  assert(nav.front() > 0.0f && "nav_stats: 起点必须 > 0");

  std::vector<float> ret = daily_returns(nav);

  double years = static_cast<double>(nav.size()) / TRADING_DAYS;
  double total = static_cast<double>(nav.back()) / static_cast<double>(nav.front());
  s.ann_return = (nav.size() < 2)
                     ? std::nanf("")
                     : static_cast<float>(std::pow(total, 1.0 / years) - 1.0);

  s.ann_vol = (ret.size() < 2)
                  ? std::nanf("")
                  : static_cast<float>(nan_std(ret) * std::sqrt(TRADING_DAYS));

  float sd = nan_std(ret);
  s.sharpe = (fin(sd) && sd != 0.0f)
                 ? static_cast<float>(nan_mean(ret) / sd * std::sqrt(TRADING_DAYS))
                 : std::nanf("");

  std::vector<float> dd = drawdown_curve(nav);
  s.max_drawdown = *std::min_element(dd.begin(), dd.end());

  double peak = -std::numeric_limits<double>::infinity();
  int cur = 0;
  for (float v : nav) {
    if (static_cast<double>(v) > peak) {
      peak = v;
      cur = 0;
    } else {
      ++cur;
      s.longest_no_new_high = std::max(s.longest_no_new_high, cur);
    }
  }
  return s;
}

RelStats rel_stats(std::span<const float> ret, std::span<const float> bench_ret) {
  assert(ret.size() == bench_ret.size() && "rel_stats: 两序列长度不一致");
  RelStats s;

  std::vector<float> diff(ret.size(), std::nanf(""));
  for (std::size_t i = 0; i < ret.size(); ++i) {
    if (fin(ret[i]) && fin(bench_ret[i]))
      diff[i] = ret[i] - bench_ret[i];
  }
  float sd = nan_std(diff);
  s.info_ratio = (fin(sd) && sd != 0.0f)
                     ? static_cast<float>(nan_mean(diff) / sd *
                                          std::sqrt(TRADING_DAYS))
                     : std::nanf("");
  s.tracking_error = fin(sd)
                         ? static_cast<float>(sd * std::sqrt(TRADING_DAYS))
                         : std::nanf("");

  // Beta / Alpha: 双侧 finite 子集上的 OLS (population 协方差, ddof=0).
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int n = 0;
  for (std::size_t i = 0; i < ret.size(); ++i) {
    if (!fin(ret[i]) || !fin(bench_ret[i]))
      continue;
    double x = bench_ret[i], y = ret[i];
    sx += x;
    sy += y;
    sxx += x * x;
    sxy += x * y;
    ++n;
  }
  if (n < 5) {
    s.beta = std::nanf("");
    s.alpha = std::nanf("");
    return s;
  }
  double mx = sx / n, my = sy / n;
  double var_x = sxx / n - mx * mx;
  if (var_x <= 0.0) {
    s.beta = std::nanf("");
    s.alpha = std::nanf("");
    return s;
  }
  double beta = (sxy / n - mx * my) / var_x;
  s.beta = static_cast<float>(beta);
  s.alpha = static_cast<float>((my - beta * mx) * TRADING_DAYS);
  return s;
}

float cagr_from_nav(std::span<const float> nav) {
  if (nav.empty() || !(nav.back() > 0.0f))
    return std::nanf("");
  double n = static_cast<double>(nav.size());
  return static_cast<float>(
      std::pow(static_cast<double>(nav.back()), TRADING_DAYS / n) - 1.0);
}

std::vector<PeriodStats> period_stats(std::span<const std::string> dates,
                                      std::span<const float> ret,
                                      std::span<const float> bench_ret,
                                      bool by_year) {
  assert(dates.size() == ret.size() && dates.size() == bench_ret.size() &&
         "period_stats: 三序列长度不一致");

  std::vector<Bucket> buckets = bucketize(dates, [by_year](const std::string &d) {
    assert(d.size() == 8 && "period_stats: 日期非 YYYYMMDD");
    return by_year ? d.substr(0, 4) : d.substr(0, 4) + "-" + d.substr(4, 2);
  });

  std::vector<PeriodStats> out;
  out.reserve(buckets.size());
  for (const Bucket &b : buckets) {
    std::span<const float> rs = ret.subspan(b.lo, b.hi - b.lo);
    std::span<const float> bs = bench_ret.subspan(b.lo, b.hi - b.lo);
    PeriodStats p;
    p.period = b.key;
    p.strat_return = compound(ret, b.lo, b.hi);
    p.bench_return = compound(bench_ret, b.lo, b.hi);
    p.strat_max_dd = span_max_dd(ret, b.lo, b.hi);
    p.bench_max_dd = span_max_dd(bench_ret, b.lo, b.hi);
    RelStats rel = rel_stats(rs, bs);
    p.tracking_error = rel.tracking_error;
    p.info_ratio = rel.info_ratio;
    float sd = nan_std(rs);
    p.ann_vol = (rs.size() < 2 || !fin(sd))
                    ? std::nanf("")
                    : static_cast<float>(sd * std::sqrt(TRADING_DAYS));
    p.sharpe = (fin(sd) && sd != 0.0f)
                   ? static_cast<float>(nan_mean(rs) / sd *
                                        std::sqrt(TRADING_DAYS))
                   : std::nanf("");
    out.push_back(std::move(p));
  }
  return out;
}

float win_rate(std::span<const std::string> dates, std::span<const float> ret,
               int unit) {
  assert(dates.size() == ret.size() && "win_rate: 两序列长度不一致");
  assert(unit >= 0 && unit <= 2 && "win_rate: unit ∈ {0 日, 1 周, 2 月}");
  if (dates.empty())
    return std::nanf("");

  if (unit == 0) {
    int win = 0;
    for (float r : ret) {
      if (fin(r) && r > 0.0f)
        ++win;
    }
    return static_cast<float>(win) / static_cast<float>(ret.size());
  }

  std::vector<Bucket> buckets =
      (unit == 1)
          ? bucketize(dates, [](const std::string &d) { return week_key(d); })
          : bucketize(dates,
                      [](const std::string &d) { return d.substr(0, 6); });
  int win = 0;
  for (const Bucket &b : buckets) {
    if (compound(ret, b.lo, b.hi) > 0.0f)
      ++win;
  }
  return static_cast<float>(win) / static_cast<float>(buckets.size());
}

float pearson(std::span<const float> x, std::span<const float> y) {
  assert(x.size() == y.size() && "pearson: 两序列长度不一致");
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
  int n = 0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (!fin(x[i]) || !fin(y[i]))
      continue;
    double a = x[i], b = y[i];
    sx += a;
    sy += b;
    sxx += a * a;
    syy += b * b;
    sxy += a * b;
    ++n;
  }
  if (n < 5)
    return std::nanf("");
  double mx = sx / n, my = sy / n;
  double cov = sxy / n - mx * my;
  double vx = sxx / n - mx * mx;
  double vy = syy / n - my * my;
  if (vx <= 0.0 || vy <= 0.0)
    return std::nanf("");
  return static_cast<float>(cov / std::sqrt(vx * vy));
}

std::vector<float> gaussian_kde(std::span<const float> samples,
                                std::span<const float> grid) {
  std::vector<float> out(grid.size(), std::nanf(""));
  std::vector<double> xs;
  xs.reserve(samples.size());
  double sum = 0.0, sumsq = 0.0;
  for (float v : samples) {
    if (!fin(v))
      continue;
    xs.push_back(static_cast<double>(v));
    sum += v;
    sumsq += static_cast<double>(v) * v;
  }
  int n = static_cast<int>(xs.size());
  if (n < 2)
    return out;
  double mean = sum / n;
  double var = sumsq / n - mean * mean; // population (ddof=0)
  if (!(var > 0.0))
    return out;
  double sd = std::sqrt(var);
  // Scott's rule (1D): h = std × n^(-1/5)
  double h = sd * std::pow(static_cast<double>(n), -0.2);
  if (!(h > 0.0))
    return out;
  double norm = 1.0 / (n * h * std::sqrt(2.0 * std::numbers::pi));
  for (std::size_t g = 0; g < grid.size(); ++g) {
    double x = grid[g];
    double acc = 0.0;
    for (double xi : xs) {
      double z = (x - xi) / h;
      acc += std::exp(-0.5 * z * z);
    }
    out[g] = static_cast<float>(acc * norm);
  }
  return out;
}

float nan_mean(std::span<const float> x) {
  double sum = 0.0;
  int n = 0;
  for (float v : x) {
    if (fin(v)) {
      sum += v;
      ++n;
    }
  }
  return n > 0 ? static_cast<float>(sum / n) : std::nanf("");
}

float nan_sum(std::span<const float> x) {
  double sum = 0.0;
  for (float v : x) {
    if (fin(v))
      sum += v;
  }
  return static_cast<float>(sum);
}

float nan_std(std::span<const float> x) {
  double sum = 0.0, sumsq = 0.0;
  int n = 0;
  for (float v : x) {
    if (fin(v)) {
      sum += v;
      sumsq += static_cast<double>(v) * v;
      ++n;
    }
  }
  if (n == 0)
    return std::nanf("");
  double m = sum / n;
  double var = sumsq / n - m * m;
  return static_cast<float>(std::sqrt(std::max(0.0, var)));
}

} // namespace report
