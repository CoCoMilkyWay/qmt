#pragma once

#include "misc/schedule.hpp"
#include "package/yyjson/yyjson.h"
#include <string>
#include <utility>
#include <vector>

namespace tushare {

class Http;
struct InterfaceSpec;

// 每个 task 描述单次 API 调用:
//   - start/end: 该次 fetch 服务的 visible_date 过滤范围 (store 据此分桶 + 范围裁剪 + 写空 [])
//   - params: 完整 query 参数 (含 start_date/end_date 或 day_param=day 等, 按 strategy 决定)
struct FetchTask {
  std::string start;
  std::string end;
  std::vector<std::pair<std::string, std::string>> params;
};

// 调度 ↔ API 调用之间的桥: 由 misc::plan_day_segments 给出的 (start, end) 段
// 拼装成具体 API task. 单一判别式: day_params 是否为空.
//
//   day_params 空  → range-capable: 段 [s, e] → 1 task, params=[(start_date,s),(end_date,e)]
//                                   (forecast / express)
//   day_params 非空 → per-day:      段必为 [d, d], 段 → N tasks (按 day_params[i] 各一次)
//                                   (disclosure: {"ann_date"}; 双查询如 dividend: {"ann_date","imp_ann_date"})
struct FetchStrategy {
  std::vector<std::string> day_params;

  bool can_range() const { return day_params.empty(); }
  std::vector<FetchTask> segment_to_tasks(const misc::FetchSegment &seg) const;
};

struct InterfaceSpec {
  std::string name;
  std::string api;
  // visible_date 字段按优先级排列: 第一个 "存在 + 非 null + 非空字符串" 的字段值
  // 即视为该记录的 visible_date (落到 data/YYYY/MM/DD/<name>.json).
  std::vector<std::string> visible_date_fields;
  std::vector<std::string> pk;
  FetchStrategy strategy;
  // 不持久化的字段 (防未来信息泄漏): API 响应中存在但视作 visible_date 之后才填入
  // 的字段, 写盘前剥离. 例: disclosure (visible=ann_date) 剥离 actual_date/modify_date.
  std::vector<std::string> drop_fields;
};

extern const std::vector<InterfaceSpec> SPECS;

// ============================================================================
// fetch — 一步式 HTTP 查询入口 (与 bigquant::fetch 对仗).
//   task.params 已是完整 query; 内部直接转发到 Http::call. 返回 yyjson_doc* 由 caller free.
// ============================================================================
yyjson_doc *fetch(Http &http, const InterfaceSpec &spec, const FetchTask &task);

} // namespace tushare
