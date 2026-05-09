#include "feature/axis.hpp"

#include <cassert>

namespace feature {

int Axes::floor_date(std::string_view) const {
  assert(false && "feature::Axes::floor_date not implemented");
  return -1;
}

Axes load_axes() {
  assert(false && "feature::load_axes not implemented");
  return {};
}

StockMeta load_stock_meta(const Axes &) {
  assert(false && "feature::load_stock_meta not implemented");
  return {};
}

} // namespace feature
