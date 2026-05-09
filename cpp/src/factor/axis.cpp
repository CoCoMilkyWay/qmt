#include "factor/axis.hpp"

#include <cassert>

namespace factor {

int Axes::floor_date(std::string_view) const {
  assert(false && "factor::Axes::floor_date not implemented");
  return -1;
}

Axes load_axes() {
  assert(false && "factor::load_axes not implemented");
  return {};
}

StockMeta load_stock_meta(const Axes &) {
  assert(false && "factor::load_stock_meta not implemented");
  return {};
}

} // namespace factor
