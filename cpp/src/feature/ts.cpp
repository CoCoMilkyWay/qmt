#include "feature/ts.hpp"

#include "feature/feature.hpp"
#include "misc/affinity.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace feature {

// ============================================================================
// 通用 dispatcher: per-A 并行; 每 worker 串行调 FEATURES[] 内 axis==TimeSeries 的
//   compute_ts(a, ...). 不出现具体 feature 名 — 业务全部下沉 feature.cpp.
//
// 顺序依赖: FEATURES[] 索引顺序 = 计算顺序 = F 枚举顺序 (见 feature.hpp). 后段
//   feature 可读已写就的 T.ts_row(prior_f, a), 无需 topo sort.
// ============================================================================
void compute_ts(const Axes &axes, const PitPool &pool, const StockMeta &meta,
                Tensor &T) {
  int n_a = axes.n_a();
  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  std::atomic<int> next{0};

  auto worker = [&]() {
    for (;;) {
      int a = next.fetch_add(1, std::memory_order_relaxed);
      if (a >= n_a) break;
      for (std::size_t f = 0; f < FEATURES.size(); ++f) {
        const FeatureMeta &fm = FEATURES[f];
        if (fm.axis != Axis::TimeSeries) continue;
        if (!fm.compute_ts) continue;
        fm.compute_ts(a, axes, pool, meta, T);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();
}

} // namespace feature
