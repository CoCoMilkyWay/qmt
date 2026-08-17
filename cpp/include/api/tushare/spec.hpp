#pragma once

#include <string>
#include <vector>

namespace tushare {

// ============================================================================
// InterfaceSpec — Tushare 事件表元描述 (BigQuant 无等价的 3 张 fallback).
//
//   visible_date   → 因果安全可见日列名 ("ann_date"; misc::plan_months 水位增量用)
//   avail_hour     → day X 数据于 X 日该小时后完整; 公告全天涓流 → 24
//                    (次日才完整, horizon 恒昨天, 永不吃半天)
//   day_params 空  → range-capable: 月段 [s, e] 1 次调用
//                    params = {start_date: s, end_date: e}
//   day_params 非空 → per-day-only: 月内逐日 × 每 day_param 各 1 次调用
//                    (disclosure: {"ann_date"})
//   drop_fields    → 不持久化的列 (在 visible_date 之后才回填的未来信息,
//                    parse 阶段剥离; e.g. disclosure 的 actual_date/modify_date)
// ============================================================================
struct InterfaceSpec {
  std::string name;         // 落盘表名 (data/YYYY-MM/<name>.parquet)
  std::string api;          // tushare api_name
  std::string visible_date; // "ann_date"
  int avail_hour;
  std::vector<std::string> day_params;
  std::vector<std::string> drop_fields;
};

extern const std::vector<InterfaceSpec> SPECS;

} // namespace tushare
