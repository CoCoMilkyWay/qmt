#include "api/tushare/spec.hpp"

namespace tushare {

// ============================================================================
// SPECS — BigQuant 无等价的事件型 fallback (3 张)
//   forecast    业绩预告:     visible=ann_date (pit.cpp 直接读该列)
//   express     业绩快报:     visible=ann_date
//   disclosure  财报披露计划: visible=ann_date, per-day API;
//               drop actual_date/modify_date (ann_date 之后回填 = 未来信息)
// ============================================================================
// avail_hour=24: 公告当日全天涓流发布, 次日才视为完整 (增量水位永不吃半天).
const std::vector<InterfaceSpec> SPECS = {
    {"forecast", "forecast_vip", "ann_date", 24, {}, {}},
    {"express", "express_vip", "ann_date", 24, {}, {}},
    {"disclosure", "disclosure_date", "ann_date", 24, {"ann_date"}, {"actual_date", "modify_date"}},
};

} // namespace tushare
