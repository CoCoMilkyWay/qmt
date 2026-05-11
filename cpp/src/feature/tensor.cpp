#include "feature/tensor.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace feature {

Tensor::Tensor(const Axes &ax) : axes(ax) {
  std::size_t n = static_cast<std::size_t>(ax.n_a()) * static_cast<std::size_t>(ax.n_d());
  mats.assign(static_cast<std::size_t>(F::COUNT), std::vector<float>(n, std::nanf("")));
}

std::span<float> Tensor::ts_row(F f, int a) {
  assert(a >= 0 && a < axes.n_a());
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d());
  return std::span<float>(mats[static_cast<std::size_t>(f)].data() + base,
                          static_cast<std::size_t>(axes.n_d()));
}

std::span<const float> Tensor::ts_row(F f, int a) const {
  assert(a >= 0 && a < axes.n_a());
  std::size_t base = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d());
  return std::span<const float>(mats[static_cast<std::size_t>(f)].data() + base,
                                static_cast<std::size_t>(axes.n_d()));
}

void Tensor::gather_cs_row(F f, int d, std::span<float> out) const {
  assert(d >= 0 && d < axes.n_d());
  int na = axes.n_a();
  int nd = axes.n_d();
  assert(static_cast<int>(out.size()) == na);
  const float *base = mats[static_cast<std::size_t>(f)].data();
  for (int a = 0; a < na; ++a) {
    out[static_cast<std::size_t>(a)] =
        base[static_cast<std::size_t>(a) * static_cast<std::size_t>(nd) +
             static_cast<std::size_t>(d)];
  }
}

void Tensor::scatter_cs_row(F f, int d, std::span<const float> in) {
  assert(d >= 0 && d < axes.n_d());
  int na = axes.n_a();
  int nd = axes.n_d();
  assert(static_cast<int>(in.size()) == na);
  float *base = mats[static_cast<std::size_t>(f)].data();
  for (int a = 0; a < na; ++a) {
    base[static_cast<std::size_t>(a) * static_cast<std::size_t>(nd) +
         static_cast<std::size_t>(d)] = in[static_cast<std::size_t>(a)];
  }
}

float Tensor::at(F f, int a, int d) const {
  assert(a >= 0 && a < axes.n_a() && d >= 0 && d < axes.n_d());
  std::size_t off = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d()) +
                    static_cast<std::size_t>(d);
  return mats[static_cast<std::size_t>(f)][off];
}

float &Tensor::at(F f, int a, int d) {
  assert(a >= 0 && a < axes.n_a() && d >= 0 && d < axes.n_d());
  std::size_t off = static_cast<std::size_t>(a) * static_cast<std::size_t>(axes.n_d()) +
                    static_cast<std::size_t>(d);
  return mats[static_cast<std::size_t>(f)][off];
}

void Tensor::assert_finite(F f) const {
  const auto &m = mats[static_cast<std::size_t>(f)];
  int n_a = axes.n_a();
  int n_d = axes.n_d();
  for (int a = 0; a < n_a; ++a) {
    for (int d = 0; d < n_d; ++d) {
      float v = m[static_cast<std::size_t>(a) * static_cast<std::size_t>(n_d) +
                  static_cast<std::size_t>(d)];
      assert(std::isfinite(v) && "Tensor::assert_finite: NaN/Inf detected");
    }
  }
}

} // namespace feature
