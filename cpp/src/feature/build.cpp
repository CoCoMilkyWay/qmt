#include "feature/build.hpp"

#include "feature/cs.hpp"
#include "feature/load.hpp"
#include "feature/pit.hpp"
#include "feature/registry.hpp"
#include "feature/ts.hpp"
#include "misc/timer.hpp"
#include "strategy/columns.hpp"
#include "strategy/registry.hpp"

namespace feature {

Tensor build(Axes &out_axes, StockMeta &out_meta) {
  // ---- Phase 0: axes + meta + tensor 分配 ----
  {
    misc::Timer t("[feature] phase 0 axes");
    out_axes = load_axes();
    out_meta = load_stock_meta(out_axes);
  }

  Tensor T(out_axes, ALL_NODES, strategy::N_STRAT_SLOTS);

  // ---- Phase 1: PIT 并行加载 ----
  PitPool pool;
  {
    misc::Timer t("[feature] phase 1 load");
    load_pit(out_axes, pool);
  }

  // ---- Phase 2: per-A 时序 (共享图, TS_ORDER) ----
  {
    misc::Timer t("[feature] phase 2 ts");
    compute_ts(out_axes, pool, out_meta, T);
  }

  // ---- Phase 2s: per-A 策略 TS 列 (各策略 pool_b) ----
  {
    misc::Timer t("[strategy] phase 2s pool_b");
    strategy::compute_ts_columns(out_axes, out_meta, T);
  }

  // ---- Phase 3: per-D 截面 (共享图, CS_ORDER) ----
  {
    misc::Timer t("[feature] phase 3 cs");
    compute_cs(out_axes, T);
  }

  // ---- Phase 3s: per-D 策略 CS 列 (pool → score → rank) ----
  {
    misc::Timer t("[strategy] phase 3s columns");
    strategy::compute_cs_columns(out_axes, T);
  }

  // ---- 验证: 契约级 bool feature + 全部策略列必须全 finite (raw/factor 列可 NaN) ----
  //   契约来源 = 每节点自己声明的 FeatureSpec::must_be_finite (无中心清单).
  {
    misc::Timer t("[feature] phase 4 assert_finite");
    for (const FeatureSpec *f : ALL_NODES)
      if (f->must_be_finite)
        T.assert_finite(*f);
    for (int slot = 0; slot < strategy::N_STRAT_SLOTS; ++slot)
      T.assert_finite_strat(slot);
  }

  return T;
}

} // namespace feature
