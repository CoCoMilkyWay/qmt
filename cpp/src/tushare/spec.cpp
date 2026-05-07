#include "tushare/spec.hpp"
#include "config.hpp"
#include "misc/date.hpp"
#include "tushare/http.hpp"

#include <algorithm>
#include <chrono>

namespace tushare {

using std::chrono::days;
using std::chrono::sys_days;

// ============================================================================
// 通用 fetch：task.params 已是完整 query
// ============================================================================

yyjson_doc *FetchStrategy::fetch(Http &http, const FetchTask &task,
                                 const InterfaceSpec &spec) const {
  return http.call(spec.api, task.params);
}

// ============================================================================
// RangeStrategy
// ============================================================================

namespace {

// 把 missing (按字典序升序) 切成连续段，每段长度 ≤ max_days
std::vector<std::pair<std::string, std::string>>
split_segments(const std::vector<std::string> &missing, int max_days) {
  std::vector<std::pair<std::string, std::string>> segments;
  if (missing.empty()) return segments;

  sys_days seg_start = misc::parse_yyyymmdd(missing[0]);
  sys_days seg_prev = seg_start;
  auto flush = [&](sys_days end) {
    sys_days cur = seg_start;
    while (cur <= end) {
      sys_days block_end = std::min(cur + days{max_days - 1}, end);
      segments.emplace_back(misc::fmt_yyyymmdd(cur),
                            misc::fmt_yyyymmdd(block_end));
      cur = block_end + days{1};
    }
  };
  for (size_t i = 1; i < missing.size(); i++) {
    sys_days cur = misc::parse_yyyymmdd(missing[i]);
    if (cur == seg_prev + days{1}) {
      seg_prev = cur;
    } else {
      flush(seg_prev);
      seg_start = cur;
      seg_prev = cur;
    }
  }
  flush(seg_prev);
  return segments;
}

} // namespace

std::vector<FetchTask>
RangeStrategy::plan(const std::vector<std::string> &missing) const {
  auto segments = split_segments(missing, max_days_);

  std::vector<FetchTask> tasks;
  size_t n_variants = variant_values_.empty() ? 1 : variant_values_.size();
  tasks.reserve(segments.size() * n_variants);

  for (auto &[s, e] : segments) {
    if (variant_values_.empty()) {
      tasks.push_back({s, e, {{"start_date", s}, {"end_date", e}}});
    } else {
      for (auto &v : variant_values_) {
        tasks.push_back(
            {s, e, {{"start_date", s}, {"end_date", e}, {variant_key_, v}}});
      }
    }
  }
  return tasks;
}

// ============================================================================
// PerDayStrategy
// ============================================================================

std::vector<FetchTask>
PerDayStrategy::plan(const std::vector<std::string> &missing) const {
  std::vector<FetchTask> tasks;
  tasks.reserve(missing.size() * day_params_.size());
  for (auto &d : missing) {
    for (auto &p : day_params_) {
      tasks.push_back({d, d, {{p, d}}});
    }
  }
  return tasks;
}

// ============================================================================
// SPECS
// ============================================================================

const std::vector<InterfaceSpec> SPECS = {
    {"forecast",
     "forecast_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    {"express",
     "express_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    {"disclosure",
     "disclosure_date",
     {"actual_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(
         std::vector<std::string>{"actual_date"})},
    // ST 风险警示：同一天一只股可能有多种 ST 类型变更，PK 含 st_tpye (tushare 字段名)
    {"st",
     "st",
     {"pub_date"},
     {"ts_code", "pub_date", "imp_date", "st_tpye"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"pub_date"})},
    // 历史每日股票基础列表 (一天 ~5000 行，限额 7000 内)
    {"basic",
     "bak_basic",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 交易日历：每天每交易所仅 1 行，按 10 年/段切；变体笛卡尔积 SSE/SZSE
    // 北交所不在 trade_cal 输出枚举内 (文档仅 SSE/SZSE/CFFEX/SHFE/CZCE/DCE/INE)，
    // 实测 BSE/BJSE/BJEX/BJ/NEEQ 全部返回 items=[]；A 股节假日由证监会统一安排，
    // 北交所与 SSE/SZSE 一致，无需单拉。保留 SSE+SZSE 作为冗余交叉验证。
    {"calendar",
     "trade_cal",
     {"cal_date"},
     {"exchange", "cal_date"},
     std::make_shared<RangeStrategy>(
         3650, "exchange", std::vector<std::string>{"SSE", "SZSE"})},
    // 分红送股：每个 day=Y 双查询 (ann_date=Y) + (imp_ann_date=Y)
    // - ann_date=Y 抓当天预案/决议公告 (visible_date=ann_date=Y)
    // - imp_ann_date=Y 抓当天实施公告 (visible_date=imp_ann_date=Y)
    // visible_date 优先 imp_ann_date 否则 ann_date：实施阶段归实施公告日
    // PK 含 div_proc：同一笔分红的"预案/股东大会通过/实施"为多条记录
    {"dividend",
     "dividend",
     {"imp_ann_date", "ann_date"},
     {"ts_code", "end_date", "div_proc"},
     std::make_shared<PerDayStrategy>(
         std::vector<std::string>{"ann_date", "imp_ann_date"})},
};

} // namespace tushare
