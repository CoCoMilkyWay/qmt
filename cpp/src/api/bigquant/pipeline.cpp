#include "api/bigquant/pipeline.hpp"

#include "api/bigquant/dai.hpp"
#include "api/bigquant/spec.hpp"
#include "api/bigquant/store.hpp"
#include "config.hpp"
#include "misc/date.hpp"
#include "misc/schedule.hpp"
#include "misc/store.hpp"

#include <arrow/table.h>

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bigquant {

namespace {

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
            << "d, dedup=" << ::config::PIPELINE_DEDUP_WINDOW_SECONDS << "s)"
            << std::endl;

  for (const auto &spec : specs) {
    if (misc::store::should_skip_api(spec.name,
                                     ::config::PIPELINE_DEDUP_WINDOW_SECONDS)) {
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
      std::cout << "\n[" << spec.name << "] plan ..." << std::flush;
      auto segments = misc::plan_fetch_segments(spec.name, start, end,
                                                lookback_days, /*can_range=*/true);
      std::cout << " " << segments.size() << " segment(s)" << std::endl;
      for (auto &seg : segments) {
        fetch_and_write_segment(client, spec, seg.start, seg.end);
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
