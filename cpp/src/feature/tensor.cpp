#include "feature/tensor.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace feature {

namespace {

// mats / strat_mats 共用的按列访问原语 (布局一致, 仅索引空间不同).

std::span<float> mat_ts_row(std::vector<float> &m, const Axes &axes, int a) {
  assert(a >= 0 && a < axes.n_a());
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d());
  return std::span<float>(m.data() + base, static_cast<std::size_t>(axes.n_d()));
}

std::span<const float> mat_ts_row(const std::vector<float> &m, const Axes &axes,
                                  int a) {
  assert(a >= 0 && a < axes.n_a());
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d());
  return std::span<const float>(m.data() + base,
                                static_cast<std::size_t>(axes.n_d()));
}

void mat_gather(const std::vector<float> &m, const Axes &axes, int d,
                std::span<float> out) {
  assert(d >= 0 && d < axes.n_d());
  int na = axes.n_a();
  int nd = axes.n_d();
  assert(static_cast<int>(out.size()) == na);
  const float *base = m.data();
  for (int a = 0; a < na; ++a) {
    out[static_cast<std::size_t>(a)] =
        base[static_cast<std::size_t>(a) * static_cast<std::size_t>(nd) +
             static_cast<std::size_t>(d)];
  }
}

void mat_scatter(std::vector<float> &m, const Axes &axes, int d,
                 std::span<const float> in) {
  assert(d >= 0 && d < axes.n_d());
  int na = axes.n_a();
  int nd = axes.n_d();
  assert(static_cast<int>(in.size()) == na);
  float *base = m.data();
  for (int a = 0; a < na; ++a) {
    base[static_cast<std::size_t>(a) * static_cast<std::size_t>(nd) +
         static_cast<std::size_t>(d)] = in[static_cast<std::size_t>(a)];
  }
}

std::size_t mat_off(const Axes &axes, int a, int d) {
  assert(a >= 0 && a < axes.n_a() && d >= 0 && d < axes.n_d());
  return static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d()) +
         static_cast<std::size_t>(d);
}

void mat_assert_finite(const std::vector<float> &m, const Axes &axes) {
  int n_a = axes.n_a();
  int n_d = axes.n_d();
  for (int a = 0; a < n_a; ++a) {
    for (int d = 0; d < n_d; ++d) {
      float v = m[static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d) +
                  static_cast<std::size_t>(d)];
      assert(std::isfinite(v) && "Tensor::assert_finite: NaN/Inf detected");
      (void)v;
    }
  }
}

} // namespace

Tensor::Tensor(const Axes &ax, std::span<const FeatureSpec *const> order,
              int n_strat_slots)
    : axes(ax) {
  assert(n_strat_slots >= 0);
  std::size_t n = static_cast<std::size_t>(ax.n_a()) * static_cast<std::size_t>(ax.n_d());
  mats.assign(order.size(), std::vector<float>(n, std::nanf("")));
  index_.reserve(order.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    index_[order[i]] = static_cast<int>(i);
  strat_mats.assign(static_cast<std::size_t>(n_strat_slots),
                    std::vector<float>(n, std::nanf("")));
}

int Tensor::index_of(const FeatureSpec &f) const {
  auto it = index_.find(&f);
  assert(it != index_.end() &&
        "Tensor: feature not in compiled graph order (未被任何策略/框架根引用?)");
  return it->second;
}

std::span<float> Tensor::ts_row(const FeatureSpec &f, int a) {
  return mat_ts_row(mats[static_cast<std::size_t>(index_of(f))], axes, a);
}

std::span<const float> Tensor::ts_row(const FeatureSpec &f, int a) const {
  return mat_ts_row(mats[static_cast<std::size_t>(index_of(f))], axes, a);
}

void Tensor::gather_cs_row(const FeatureSpec &f, int d, std::span<float> out) const {
  mat_gather(mats[static_cast<std::size_t>(index_of(f))], axes, d, out);
}

void Tensor::scatter_cs_row(const FeatureSpec &f, int d, std::span<const float> in) {
  mat_scatter(mats[static_cast<std::size_t>(index_of(f))], axes, d, in);
}

float Tensor::at(const FeatureSpec &f, int a, int d) const {
  return mats[static_cast<std::size_t>(index_of(f))][mat_off(axes, a, d)];
}

float &Tensor::at(const FeatureSpec &f, int a, int d) {
  return mats[static_cast<std::size_t>(index_of(f))][mat_off(axes, a, d)];
}

std::span<float> Tensor::strat_ts_row(int slot, int a) {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  return mat_ts_row(strat_mats[static_cast<std::size_t>(slot)], axes, a);
}

std::span<const float> Tensor::strat_ts_row(int slot, int a) const {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  return mat_ts_row(strat_mats[static_cast<std::size_t>(slot)], axes, a);
}

void Tensor::strat_gather_cs_row(int slot, int d, std::span<float> out) const {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  mat_gather(strat_mats[static_cast<std::size_t>(slot)], axes, d, out);
}

void Tensor::strat_scatter_cs_row(int slot, int d, std::span<const float> in) {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  mat_scatter(strat_mats[static_cast<std::size_t>(slot)], axes, d, in);
}

float Tensor::strat_at(int slot, int a, int d) const {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  return strat_mats[static_cast<std::size_t>(slot)][mat_off(axes, a, d)];
}

void Tensor::assert_finite(const FeatureSpec &f) const {
  mat_assert_finite(mats[static_cast<std::size_t>(index_of(f))], axes);
}

void Tensor::assert_finite_strat(int slot) const {
  assert(slot >= 0 && slot < static_cast<int>(strat_mats.size()));
  mat_assert_finite(strat_mats[static_cast<std::size_t>(slot)], axes);
}

} // namespace feature
