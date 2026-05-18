#include "misc/schedule.hpp"

#include "misc/date.hpp"
#include "misc/store.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace misc {

namespace {

// 该月 (YYYY, MM) 在 outer [start, end] 内的 clamped 闭区间.
//   返回 (clamp_first, clamp_last); 调用方保证该月与 outer 有交集.
std::pair<std::string, std::string>
clamped_month(const std::string &yyyymm, std::string_view outer_start,
              std::string_view outer_end) {
  assert(yyyymm.size() == 6);
  std::string yyyy = yyyymm.substr(0, 4);
  std::string mm = yyyymm.substr(4, 2);
  std::string mfirst = yyyy + mm + "01";
  std::string mlast = yyyy + mm + month_last_dd(yyyymm);
  return {std::max(mfirst, std::string(outer_start)),
          std::min(mlast, std::string(outer_end))};
}

} // namespace

std::vector<FetchSegment>
plan_fetch_segments(std::string_view name, std::string_view start,
                    std::string_view end, int lookback_days, bool can_range) {
  std::vector<std::string> missing =
      store::scan_missing_days(name, start, end, lookback_days);

  std::vector<FetchSegment> out;
  if (missing.empty())
    return out;

  // ---- per-day-only API: 每个 missing 日各自一段 ----
  if (!can_range) {
    out.reserve(missing.size());
    for (auto &d : missing)
      out.push_back({d, d});
    return out;
  }

  // ---- range-capable: 按月聚合, 整月空洞 → 月段, 否则 → 日段 ----
  // missing 已按 scan_missing_days 升序; 按月分桶维持升序.
  std::unordered_set<std::string> missing_set(missing.begin(), missing.end());

  std::vector<std::string> month_keys; // 升序 (按 missing 自然出现顺序)
  std::string prev_ym;
  for (auto &d : missing) {
    std::string ym = d.substr(0, 6);
    if (ym != prev_ym) {
      month_keys.push_back(ym);
      prev_ym = ym;
    }
  }

  for (const auto &ym : month_keys) {
    auto [cs, ce] = clamped_month(ym, start, end);
    // 月内 clamp 范围内是否每天都缺失
    bool all_missing = true;
    for (auto &d : iter_days(cs, ce)) {
      if (!missing_set.count(d)) {
        all_missing = false;
        break;
      }
    }
    if (all_missing) {
      out.push_back({cs, ce});
      continue;
    }
    // 否则该月每个缺失日单独一段
    for (auto &d : iter_days(cs, ce)) {
      if (missing_set.count(d))
        out.push_back({d, d});
    }
  }
  return out;
}

} // namespace misc
