#include "feature/build.hpp"

#include "feature/cs.hpp"
#include "feature/load.hpp"
#include "feature/pit.hpp"
#include "feature/ts.hpp"
#include "misc/timer.hpp"

namespace feature {

Tensor build(Axes &out_axes, StockMeta &out_meta) {
  // ---- Phase 0: axes + meta + tensor 分配 ----
  {
    misc::Timer t("[feature] phase 0 axes");
    out_axes = load_axes();
    out_meta = load_stock_meta(out_axes);
  }

  Tensor T(out_axes);

  // ---- Phase 1: PIT 并行加载 ----
  PitPool pool;
  {
    misc::Timer t("[feature] phase 1 load");
    load_pit(out_axes, pool);
  }

  // ---- Phase 2: per-A 时序 ----
  {
    misc::Timer t("[feature] phase 2 ts");
    compute_ts(out_axes, pool, out_meta, T);
  }

  // ---- Phase 3: per-D 截面 ----
  {
    misc::Timer t("[feature] phase 3 cs");
    compute_cs(out_axes, T);
  }

  // ---- 验证: 层层合理处理后, 应无 NaN ----
  T.assert_no_nan();

  return T;
}

} // namespace feature
