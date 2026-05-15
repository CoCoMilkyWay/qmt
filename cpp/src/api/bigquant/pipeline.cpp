#include "api/bigquant/pipeline.hpp"

#include "api/bigquant/dai.hpp"
#include "api/bigquant/spec.hpp"
#include "api/bigquant/store.hpp"
#include "config.hpp"
#include "misc/date.hpp"
#include "misc/store.hpp"

#include <arrow/table.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace bigquant {

namespace {

using std::chrono::days;
using std::chrono::sys_days;

// "YYYYMMDD" -> "YYYY-MM-DD" (DAI 接受格式)
std::string to_dashed(std::string_view yyyymmdd) {
  assert(yyyymmdd.size() == 8);
  std::string out;
  out.reserve(10);
  out.append(yyyymmdd.data(), 4);
  out.push_back('-');
  out.append(yyyymmdd.data() + 4, 2);
  out.push_back('-');
  out.append(yyyymmdd.data() + 6, 2);
  return out;
}

// 切连续段: missing (升序) → [{seg_start, seg_end}, ...], 每段 ≤ max_days
std::vector<std::pair<std::string, std::string>>
split_segments(const std::vector<std::string> &missing, int max_days) {
  std::vector<std::pair<std::string, std::string>> segments;
  if (missing.empty())
    return segments;
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
  for (size_t i = 1; i < missing.size(); ++i) {
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

// 分发 fetch 一段并落地
void fetch_and_write_segment(DaiClient &client, const TableSpec &spec,
                             const std::string &seg_start,
                             const std::string &seg_end) {
  std::string ds = to_dashed(seg_start);
  std::string de = to_dashed(seg_end);
  std::cout << "  " << seg_start;
  if (seg_end != seg_start)
    std::cout << "~" << seg_end;
  std::cout << " ... " << std::flush;

  auto t = fetch(client, spec, ds, de);
  int64_t n = t ? t->num_rows() : 0;
  store::write_table_by_visible_date(t, spec, seg_start, seg_end);
  std::cout << n << " rows" << std::endl;
}

} // namespace

void update(std::string_view start, std::string_view end,
            const std::vector<TableSpec> &specs, int lookback_days) {
  DaiClient client;

  std::cout << "[bigquant.update] " << start << " ~ " << end << " ("
            << specs.size() << " tables, lookback=" << lookback_days
            << "d, dedup=" << ::config::API_DEDUP_WINDOW_SECONDS
            << "s, max_seg=" << ::config::BIGQUANT_FETCH_MAX_DAYS_PER_CALL
            << "d)" << std::endl;

  for (const auto &spec : specs) {
    if (misc::store::should_skip_api(spec.name,
                                     ::config::API_DEDUP_WINDOW_SECONDS)) {
      std::cout << "\n[" << spec.name << "] skip (recently updated)"
                << std::endl;
      continue;
    }

    // ---- Static: 直接全量整刷到 _meta ----
    if (spec.kind == FetchKind::Static) {
      std::cout << "\n[" << spec.name << "] static refresh ..." << std::flush;
      auto t = fetch(client, spec);
      int64_t n = t ? t->num_rows() : 0;
      store::write_meta_table(t, spec);
      std::cout << " " << n << " rows -> _meta" << std::endl;
      misc::store::mark_api_updated(spec.name);
      continue;
    }

    // ---- Day 频率 (Partition + Day / Where + Day) ----
    if (spec.freq == FetchFreq::Day) {
      std::cout << "\n[" << spec.name << "] scan ..." << std::flush;
      auto missing =
          misc::store::scan_missing_days(spec.name, start, end, lookback_days);
      std::cout << " " << missing.size() << " day(s) to fetch" << std::endl;

      if (!missing.empty()) {
        auto segments = split_segments(
            missing, ::config::BIGQUANT_FETCH_MAX_DAYS_PER_CALL);
        std::cout << "[" << spec.name << "] plan -> " << segments.size()
                  << " fetch segment(s)" << std::endl;
        for (auto &[s, e] : segments) {
          fetch_and_write_segment(client, spec, s, e);
        }
      }
      misc::store::mark_api_updated(spec.name);
      continue;
    }

    // ---- Partition + MonthFirst ----
    assert(spec.freq == FetchFreq::MonthFirst);
    assert(spec.kind == FetchKind::Partition);
    std::cout << "\n[" << spec.name << "] scan months ..." << std::flush;
    auto month_segs =
        misc::store::scan_missing_months(spec.name, start, end, lookback_days);
    std::cout << " " << month_segs.size() << " month(s) to fetch" << std::endl;
    for (auto &seg : month_segs) {
      fetch_and_write_segment(client, spec, seg.start, seg.end);
    }
    misc::store::mark_api_updated(spec.name);
  }

  std::cout << "\n[bigquant.update] done" << std::endl;
}

} // namespace bigquant
