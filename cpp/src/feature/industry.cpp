#include "feature/industry.hpp"

namespace feature {

uint8_t sw2021_l1_name_to_id(std::string_view name) {
  if (name.empty())
    return 0;
  for (std::size_t i = 1; i < SW2021_L1_COUNT; ++i) {
    if (SW2021_L1_NAMES[i] == name)
      return static_cast<uint8_t>(i);
  }
  return 0;
}

} // namespace feature
