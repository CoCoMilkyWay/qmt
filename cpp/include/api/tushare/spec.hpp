#pragma once

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

class FetchStrategy {
public:
  virtual ~FetchStrategy() = default;
  // 唯一需要派生的语义：missing days → tasks
  virtual std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const = 0;
  // 通用 fetch：所有 strategy 共用 (task.params 已是完整 query)
  yyjson_doc *fetch(Http &http, const FetchTask &task,
                    const InterfaceSpec &spec) const;
};

// 切连续段 + 可选 (key, [values]) 单键变体笛卡尔积
//   - forecast/express/suspend_d:  RangeStrategy(31)
//   - income/cashflow:             RangeStrategy(7)  (财报记录密度高, 按周保守切)
//   - trade_cal:                   RangeStrategy(3650, "exchange", {"SSE","SZSE"})
// task.params = [(start_date, S), (end_date, E)] (+ (variant_key, v) 若有)
class RangeStrategy : public FetchStrategy {
public:
  RangeStrategy(int max_days, std::string variant_key = {},
                std::vector<std::string> variant_values = {})
      : max_days_(max_days), variant_key_(std::move(variant_key)),
        variant_values_(std::move(variant_values)) {}
  std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const override;

private:
  int max_days_;
  std::string variant_key_;
  std::vector<std::string> variant_values_;
};

// 每天 N 个 task，每个 task 以 (day_params[i], day) 为唯一 query
//   - disclosure:                          PerDayStrategy({"ann_date"})
//   - report:                              PerDayStrategy({"actual_date"})
//   - st:                                  PerDayStrategy({"imp_date"})
//   - daily_basic/adj_factor/stk_limit:    PerDayStrategy({"trade_date"})
//   - margin_secs/margin_detail/stock_st:  PerDayStrategy({"trade_date"})
//   - fina_indicator:                      PerDayStrategy({"ann_date"})
//     (start_date/end_date 文档语义=报告期, 非 ann_date, 不能 Range)
//   - dividend:                            PerDayStrategy({"ann_date","imp_ann_date"})
//                                          双查询：当天预案/决议 + 当天实施
// task.params = [(day_params[i], day)]
class PerDayStrategy : public FetchStrategy {
public:
  explicit PerDayStrategy(std::vector<std::string> day_params)
      : day_params_(std::move(day_params)) {}
  std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const override;

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
