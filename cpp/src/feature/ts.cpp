#include "feature/ts.hpp"

#include "feature/registry.hpp"
#include "misc/affinity.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace feature {

// ============================================================================
// 通用 dispatcher: per-A 并行; 每 worker 按 TS_ORDER (编译期 topo 排出的计算
//   顺序, 见 registry.hpp) 串行调 compute_ts(a, ...). 不出现具体 feature 名 —
//   业务全部下沉 feature/def/. 后段 feature 可读已写就的 T.ts_row(prior_f, a).
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
      for (const FeatureSpec *f : TS_ORDER) {
        f->compute_ts(a, axes, pool, meta, T);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();
}

} // namespace feature
