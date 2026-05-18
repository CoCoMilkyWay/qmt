#pragma once

#include "misc/schedule.hpp"
#include "package/yyjson/yyjson.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tushare {

class Http;
struct InterfaceSpec;

// 每个 task 描述单次 API 调用：
//   - start/end: 该次 fetch 服务的 visible_date 过滤范围 (store 据此分桶 + 范围裁剪 + 写空 [])
//   - params: 完整 query 参数 (含 start_date/end_date 或 visible_param=day 等，按 strategy 决定)
struct FetchTask {
  std::string start;
  std::string end;
  std::vector<std::pair<std::string, std::string>> params;
};

// 调度 ↔ API 调用之间的桥: 由 misc::plan_fetch_segments 给出的 (start, end) 段
// 拼装成具体 API task. 调度规则统一在 misc 层, strategy 只描述 "我支持什么形状的段".
class FetchStrategy {
public:
  virtual ~FetchStrategy() = default;
  // true  → scheduler 允许向本 strategy 发送 [first, last] 月段; segment_to_tasks 收到的
  //         段可能是 [d, d] 也可能是 [m_first, m_last].
  // false → scheduler 必定只发 [d, d] 单日段 (per-day API 强制); 收到 start != end 即 assert.
  virtual bool can_range() const = 0;
  // 把 segment 映射为一个或多个 FetchTask. PerDay 类按 day_params 数量倍增 task.
  virtual std::vector<FetchTask>
  segment_to_tasks(const misc::FetchSegment &seg) const = 0;
  // 通用 fetch：所有 strategy 共用 (task.params 已是完整 query)
  yyjson_doc *fetch(Http &http, const FetchTask &task,
                    const InterfaceSpec &spec) const;
};

// Range-capable API: 段 [s, e] → 1 个 task, params = [(start_date, s), (end_date, e)]
//   - forecast/express: MonthStrategy
class MonthStrategy : public FetchStrategy {
public:
  bool can_range() const override { return true; }
  std::vector<FetchTask>
  segment_to_tasks(const misc::FetchSegment &seg) const override;
};

// Per-day-only API: 段必为 [d, d], 一段 → N tasks (按 day_params[i] 各一次)
//   - disclosure:                          PerDayStrategy({"ann_date"})
//   - report:                              PerDayStrategy({"actual_date"})
//   - st:                                  PerDayStrategy({"imp_date"})
//   - daily_basic/adj_factor/stk_limit:    PerDayStrategy({"trade_date"})
//   - margin_secs/margin_detail/stock_st:  PerDayStrategy({"trade_date"})
//   - fina_indicator:                      PerDayStrategy({"ann_date"})
//     (start_date/end_date 文档语义=报告期, 非 ann_date, 不能 Range)
//   - dividend:                            PerDayStrategy({"ann_date","imp_ann_date"})
//                                          双查询：当天预案/决议 + 当天实施
class PerDayStrategy : public FetchStrategy {
public:
  explicit PerDayStrategy(std::vector<std::string> day_params)
      : day_params_(std::move(day_params)) {}
  bool can_range() const override { return false; }
  std::vector<FetchTask>
  segment_to_tasks(const misc::FetchSegment &seg) const override;

private:
  std::vector<std::string> day_params_;
};

struct InterfaceSpec {
  std::string name;
  std::string api;
  // visible_date 字段按优先级排列：第一个 "存在 + 非 null + 非空字符串" 的字段值
  // 即视为该记录的 visible_date (落到 data/YYYY/MM/DD/<name>.json)
  // 单一字段接口配置一个；dividend 用 ["imp_ann_date", "ann_date"] 等
  std::vector<std::string> visible_date_fields;
  std::vector<std::string> pk;
  std::shared_ptr<FetchStrategy> strategy;
  // 不持久化的字段 (防未来信息泄漏): API 响应中存在但视作 visible_date 之后才填入
  // 的字段, 写盘前剥离. 例: disclosure (visible=ann_date) 剥离 actual_date/modify_date
  // (这两字段由 tushare 在 ann_date 之后回填, 持久化会污染 PIT 回放).
  // 默认 {} 表示无字段需剥离.
  std::vector<std::string> drop_fields;
};

extern const std::vector<InterfaceSpec> SPECS;

} // namespace tushare
