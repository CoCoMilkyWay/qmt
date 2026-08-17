#include "api/tushare/spec.hpp"

namespace tushare {

// ============================================================================
// SPECS — BigQuant 无等价的事件型 fallback (3 张)
//   forecast    业绩预告:     visible=ann_date (pit.cpp 直接读该列)
//   express     业绩快报:     visible=ann_date
//   disclosure  财报披露计划: visible=ann_date, per-day API;
//               drop actual_date/modify_date (ann_date 之后回填 = 未来信息)
// ============================================================================
const std::vector<InterfaceSpec> SPECS = {
    {"forecast", "forecast_vip", {}, {}},
    {"express", "express_vip", {}, {}},
    {"disclosure", "disclosure_date", {"ann_date"}, {"actual_date", "modify_date"}},
};

} // namespace tushare
