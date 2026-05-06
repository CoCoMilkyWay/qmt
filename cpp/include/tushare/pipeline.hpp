#pragma once

#include "tushare/spec.hpp"
#include <string_view>
#include <vector>

namespace tushare {

void update(std::string_view start, std::string_view end,
            const std::vector<InterfaceSpec> &specs);

}
