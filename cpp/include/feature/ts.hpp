#pragma once

#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace feature {

// ============================================================================
// Phase 2 入口: per-A 并行, 对每个 a 串行调用 FEATURES[] 中 axis==TimeSeries 的
//   compute_ts(a, axes, pool, meta, T). 不涉及具体 feature 名 — 业务逻辑全部
//   下沉到 feature.cpp 的 per-feature 函数.
// ============================================================================
void compute_ts(const Axes &, const PitPool &, const StockMeta &, Tensor &);

// ============================================================================
// 通用 TS kernel (供 feature.cpp 的 per-feature compute fn 复用)
//
// 1) ttm4_ytd_compute<Ev>:
//      按 v 升序回放 events, 维护 map<end_date, value> (latest version per end_date,
//      可选 report_type=='1' 过滤; fina_indicator 不带 type → get_rt 返回空串接受全部);
//      d_target = v + 1 (offset = -1, 财报 itf 全部 visible_date=ann_date).
//      自动降级: 完整 X(t)+X(Y-1,12)-X(Y-1,M) → 缺同期 X(t)+X(Y-1,12)*(12-M)/12 → 缺年报 X(t)*12/M.
//      M==12 退化为 X(t).
//
// 2) state_machine_intervals<TEv>:
//      按 v 升序遍历 trigger_events, 每 trigger 用 find_off(trigger) 求终止 d,
//      区间 [trigger.v+1, off_d) 写 1.0; 区间外 0.0; 多 trigger OR (重叠取并集).
// ============================================================================

template <class Ev, class GetReportType, class GetValue>
void ttm4_ytd_compute(const std::vector<Ev> &events, int n_d,
                      GetReportType get_rt, GetValue get_val,
                      std::span<float> out) {
  std::fill(out.begin(), out.end(), std::nanf(""));

  std::map<std::string, float> latest;
  std::size_t ev_ptr = 0;
  for (int d = 0; d < n_d; ++d) {
    while (ev_ptr < events.size() && (events[ev_ptr].v + 1) <= d) {
      const Ev &e = events[ev_ptr++];
      const std::string &rt = get_rt(e);
      if (!rt.empty() && rt != "1") continue; // 仅合并报表 (空串 = 不过滤)
      float v = get_val(e);
      if (!is_finite(v)) continue;
      if (e.end_date.empty()) continue;
      latest[e.end_date] = v;
    }
    if (latest.empty()) continue;

    auto it_t = latest.rbegin();
    const std::string &t = it_t->first;
    float x_t = it_t->second;
    int Y = year_of(t);
    int M = month_of(t);
    if (Y == 0 || M == 0) continue;

    // M==12 (年报): 直接是 TTM
    if (M == 12) { out[d] = x_t; continue; }

    char buf_y_dec[16], buf_y_m[16];
    std::snprintf(buf_y_dec, sizeof(buf_y_dec), "%04d1231", Y - 1);
    std::snprintf(buf_y_m, sizeof(buf_y_m), "%04d%s", Y - 1, t.substr(4).c_str());
    auto it_y_dec = latest.find(buf_y_dec);
    auto it_y_m = latest.find(buf_y_m);

    if (it_y_dec != latest.end() && it_y_m != latest.end()) {
      // 完整 TTM4: X(t) + X(Y-1, 12) - X(Y-1, M)
      out[d] = x_t + it_y_dec->second - it_y_m->second;
    } else if (it_y_dec != latest.end()) {
      // 降级: 缺去年同期，用 X(t) + X(Y-1,12) * (12-M)/12 近似
      float f = static_cast<float>(12 - M) / 12.0f;
      out[d] = x_t + it_y_dec->second * f;
    } else {
      // 降级: 缺去年年报，用 X(t) * 12/M 年化
      out[d] = x_t * 12.0f / static_cast<float>(M);
    }
  }
}

template <class TEv, class FindOff>
void state_machine_intervals(const std::vector<TEv> &triggers, int n_d,
                             FindOff find_off, std::span<float> dst) {
  std::fill(dst.begin(), dst.end(), 0.0f);
  for (const TEv &e : triggers) {
    int on_d  = e.v + 1; // offset=-1
    int off_d = find_off(e);
    if (on_d < 0) on_d = 0;
    if (off_d > n_d) off_d = n_d;
    if (off_d <= on_d) continue;
    for (int d = on_d; d < off_d; ++d) dst[d] = 1.0f;
  }
}

} // namespace feature
