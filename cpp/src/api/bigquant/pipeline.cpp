#include "api/bigquant/pipeline.hpp"

#include "api/bigquant/dai.hpp"
#include "api/bigquant/spec.hpp"
#include "api/bigquant/store.hpp"
#include "config.hpp"
#include "misc/schedule.hpp"
#include "misc/store.hpp"

#include <arrow/table.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace bigquant {

namespace {

// 单段 fetch + 落盘 (Day / MonthFirst 共用): 行式 day file + _empty 维护.
void fetch_and_write(DaiClient &client, const TableSpec &spec,
                     std::string_view seg_start, std::string_view seg_end) {
  std::cout << "  " << seg_start;
  if (seg_end != seg_start)
    std::cout << "~" << seg_end;
  std::cout << " ... " << std::flush;
  auto t = fetch(client, spec, seg_start, seg_end);
  int64_t n = t ? t->num_rows() : 0;
  store::write_by_visible_date(t, spec, seg_start, seg_end);
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
    // Static 单文件输出: lastupdate + 实际 _meta 文件双保险, 任一丢失即重抓.
    //   防止 lastupdate mark 完但 _meta 文件丢失 (磁盘/中断/外部 rm) 后永久跳过.
    //   emit_meta 表的 _meta 是末尾从 day file 聚合的产物, 不需要 verify 兜底
    //   (丢了下轮 update 自动重建).
    std::filesystem::path verify =
        (spec.kind == FetchKind::Static)
            ? misc::store::meta_data_path(spec.name)
            : std::filesystem::path{};
    if (misc::store::should_skip_api(
            spec.name, ::config::PIPELINE_DEDUP_WINDOW_SECONDS, verify)) {
      std::cout << "\n[" << spec.name << "] skip (recently updated)";
      // emit_meta 即使 dedup 跳过 API, 也无脑重建 _meta (day file 是真实源, 廉价).
      if (spec.emit_meta) {
        store::aggregate_meta(spec);
        std::cout << " (meta rebuilt)";
      }
      std::cout << std::endl;
      continue;
    }

    // ---- Static: DAI 一次响应直写 _meta (无 date 维, 不走调度) ----
    if (spec.kind == FetchKind::Static) {
      std::cout << "\n[" << spec.name << "] meta refresh ..." << std::flush;
      auto t = fetch(client, spec, start, end); // Static 内部忽略 start/end
      int64_t n = t ? t->num_rows() : 0;
      store::write_meta(t, spec);
      std::cout << " " << n << " rows -> _meta" << std::endl;
      misc::store::mark_api_updated(spec.name);
      continue;
    }

    // ---- Day / MonthFirst: 走对仗的两个 planner, 同形 fetch_and_write 消费 ----
    std::cout << "\n[" << spec.name << "] plan ..." << std::flush;
    auto segments = (spec.freq == FetchFreq::Day)
                        ? misc::plan_day_segments(spec.name, start, end,
                                                  lookback_days,
                                                  /*can_range=*/true)
                        : misc::plan_month_segments(spec.name, start, end,
                                                    lookback_days);
    std::cout << " " << segments.size() << " segment(s)" << std::endl;
    for (auto &seg : segments) {
      fetch_and_write(client, spec, seg.start, seg.end);
    }
    // emit_meta 表: 写完 day file 后无脑聚合 _meta (axis.cpp 的 D 轴/holidays 单文件来源).
    if (spec.emit_meta) {
      std::cout << "[" << spec.name << "] aggregate meta ..." << std::flush;
      store::aggregate_meta(spec);
      std::cout << " done" << std::endl;
    }
    misc::store::mark_api_updated(spec.name);
  }

  std::cout << "\n[bigquant.update] done" << std::endl;
}

} // namespace bigquant
