#include "misc/schedule.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace misc {

namespace fs = std::filesystem;
using std::chrono::month;
using std::chrono::sys_days;
using std::chrono::year;
using std::chrono::year_month;
using std::chrono::year_month_day;
using std::chrono::last;

namespace {

// ============================================================================
// Day 级缺失扫描: file 不存在 ∧ 不在 _empty → missing; 进入 lookback → 强拉.
// ============================================================================

std::vector<std::string> scan_missing_days(std::string_view name,
                                           std::string_view start,
                                           std::string_view end,
                                           int lookback_days) {
  assert(lookback_days >= 0);
  auto all_days = iter_days(start, end);
  std::string lookback_from = lookback_days > 0
                                  ? add_days(end, -(lookback_days - 1))
                                  : std::string{};

  // 按月缓存 _empty.json 中本 itf 对应的 DD 集合 (key = "YYYYMM")
  std::unordered_map<std::string, store::EmptySet> empty_by_month;
  std::string name_s(name);
  auto get_empty_set = [&](const std::string &d) -> const store::EmptySet & {
    std::string ym = d.substr(0, 6);
    auto it = empty_by_month.find(ym);
    if (it == empty_by_month.end()) {
      store::EmptyMonth m = store::read_empty_month(d.substr(0, 4), d.substr(4, 2));
      it = empty_by_month.emplace(ym, std::move(m[name_s])).first;
    }
    return it->second;
  };

  std::vector<std::string> missing;
  missing.reserve(all_days.size());
  for (auto &d : all_days) {
    bool in_lookback = lookback_days > 0 && d >= lookback_from;
    if (in_lookback) {
      missing.push_back(d);
      continue;
    }
    if (fs::exists(store::day_data_path(d, name)))
      continue;
    if (get_empty_set(d).count(d.substr(6, 2)))
      continue;
    missing.push_back(d);
  }
  return missing;
}

// 该月内是否存在任一 day file (data/YYYY/MM/DD/<name>.json)
bool month_has_any_day(std::string_view yyyy, std::string_view mm,
                       std::string_view name) {
  fs::path dir = git_root() / "data" / std::string(yyyy) / std::string(mm);
  if (!fs::exists(dir))
    return false;
  std::string fname = std::string(name) + ".json";
  for (auto &entry : fs::directory_iterator(dir)) {
    if (!entry.is_directory())
      continue;
    if (fs::exists(entry.path() / fname))
      return true;
  }
  return false;
}

// (YYYY, MM) 升序闭区间; 不含日期成分.
struct YM { std::string yyyy; std::string mm; };
std::vector<YM> iter_months(std::string_view start, std::string_view end) {
  assert(start.size() == 8 && end.size() == 8);
  std::vector<YM> out;
  year_month_day ymd_s{parse_yyyymmdd(start)};
  year_month_day ymd_e{parse_yyyymmdd(end)};
  year_month cur{ymd_s.year(), ymd_s.month()};
  year_month last_ym{ymd_e.year(), ymd_e.month()};
  while (cur <= last_ym) {
    char yy[5], mm[3];
    std::snprintf(yy, sizeof(yy), "%04d", static_cast<int>(cur.year()));
    std::snprintf(mm, sizeof(mm), "%02d", static_cast<unsigned>(cur.month()));
    out.push_back({std::string(yy, 4), std::string(mm, 2)});
    cur += std::chrono::months{1};
  }
  return out;
}

// 该月 (YYYY, MM) 在 outer [start, end] 内的 clamped 闭区间.
std::pair<std::string, std::string>
clamped_month(const std::string &yyyy, const std::string &mm,
              std::string_view outer_start, std::string_view outer_end) {
  std::string mfirst = yyyy + mm + "01";
  std::string mlast = yyyy + mm + month_last_dd(yyyy + mm);
  return {std::max(mfirst, std::string(outer_start)),
          std::min(mlast, std::string(outer_end))};
}

} // namespace

// ============================================================================
// plan_day_segments — Day 频率 (bigquant Day + 所有 tushare strategy)
// ============================================================================

std::vector<FetchSegment>
plan_day_segments(std::string_view name, std::string_view start,
                  std::string_view end, int lookback_days, bool can_range) {
  std::vector<std::string> missing =
      scan_missing_days(name, start, end, lookback_days);

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
    auto [cs, ce] = clamped_month(ym.substr(0, 4), ym.substr(4, 2), start, end);
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
    for (auto &d : iter_days(cs, ce)) {
      if (missing_set.count(d))
        out.push_back({d, d});
    }
  }
  return out;
}

// ============================================================================
// plan_month_segments — MonthFirst 频率 (bigquant industry_component)
// ============================================================================

std::vector<FetchSegment>
plan_month_segments(std::string_view name, std::string_view start,
                    std::string_view end, int lookback_days) {
  assert(lookback_days >= 0);
  std::string lookback_from = lookback_days > 0
                                  ? add_days(end, -(lookback_days - 1))
                                  : std::string{};
  std::vector<FetchSegment> out;
  for (auto &ym : iter_months(start, end)) {
    auto [s, e] = clamped_month(ym.yyyy, ym.mm, start, end);

    bool in_lookback = lookback_days > 0 && e >= lookback_from;
    if (in_lookback) {
      out.push_back({s, e});
      continue;
    }
    if (month_has_any_day(ym.yyyy, ym.mm, name))
      continue;
    out.push_back({s, e});
  }
  return out;
}

} // namespace misc
