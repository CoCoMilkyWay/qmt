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

void Tensor::gather_cs_row(F, int, std::span<float>) const {
  assert(false && "feature::Tensor::gather_cs_row not implemented");
}

void Tensor::scatter_cs_row(F, int, std::span<const float>) {
  assert(false && "feature::Tensor::scatter_cs_row not implemented");
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

} // namespace feature
