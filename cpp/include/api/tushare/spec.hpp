#pragma once

#include <string>
#include <vector>

namespace tushare {

// ============================================================================
// InterfaceSpec — Tushare 事件表元描述 (BigQuant 无等价的 3 张 fallback).
//
//   day_params 空  → range-capable: 月段 [s, e] 1 次调用
//                    params = {start_date: s, end_date: e}
//   day_params 非空 → per-day-only: 月内逐日 × 每 day_param 各 1 次调用
//                    (disclosure: {"ann_date"})
//   drop_fields    → 不持久化的列 (在 visible_date 之后才回填的未来信息,
//                    parse 阶段剥离; e.g. disclosure 的 actual_date/modify_date)
// ============================================================================
struct InterfaceSpec {
  std::string name; // 落盘表名 (data/YYYY-MM/<name>.parquet)
  std::string api;  // tushare api_name
  std::vector<std::string> day_params;
  std::vector<std::string> drop_fields;
};

extern const std::vector<InterfaceSpec> SPECS;

} // namespace tushare
