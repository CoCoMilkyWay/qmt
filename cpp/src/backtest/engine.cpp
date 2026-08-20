#include "backtest/engine.hpp"

#include "feature/def/basic/close_raw.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/def/basic/limit_dn.hpp"
#include "feature/def/basic/limit_up.hpp"
#include "feature/def/basic/susp.hpp"
#include "misc/timer.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace backtest {

namespace {

// 契约 bool feature: 必 finite 且 ∈ {0, 1}. 任一违背 → assert fail (定位污染源).
//   适用 susp / limit_up / limit_dn (must_be_finite=true); raw / factor 禁用.
inline bool read_bool(const feature::Tensor &T, const feature::FeatureSpec &f,
                      int a, int d) {
  float v = T.at(f, a, d);
  assert(feature::is_finite(v) && "engine::read_bool: NaN — feature should be 0/1");
  return v > 0.5f;
}

} // namespace

MarketWindow load_market(const feature::Axes &axes, const feature::Tensor &T,
                         int d_lo, int n_d) {
  misc::Timer t("[backtest] load_market");
  int n_a = axes.n_a();
  assert(d_lo >= 0 && n_d > 0 && d_lo + n_d <= axes.n_d());

  MarketWindow mk;
  mk.d_lo = d_lo;
  mk.n_d = n_d;
  mk.n_a = n_a;
  std::size_t n = static_cast<std::size_t>(n_d) * static_cast<std::size_t>(n_a);
  mk.close.assign(n, std::nanf(""));
  mk.flags.assign(n, 0);

  // last_close 语义: 窗口内 (从 d_lo 起) 沿 D 前向填充最近一个 finite 收盘.
  //   close_raw 本身已在 feature 层 ffill, 这里只兜退市后转 NaN 的尾段 —
  //   与"逐日刷 last_close 缓存"逐位等价, 但只算一次.
  for (int a = 0; a < n_a; ++a) {
    float last = std::nanf("");
    for (int i = 0; i < n_d; ++i) {
      int d = d_lo + i;
      float c = T.at(feature::def::close_raw_spec, a, d);
      if (feature::is_finite(c))
        last = c;
      std::size_t off = static_cast<std::size_t>(i) * static_cast<std::size_t>(n_a) +
                        static_cast<std::size_t>(a);
      mk.close[off] = last;

      std::uint8_t f = 0;
      if (read_bool(T, feature::def::susp_spec, a, d))
        f |= MarketWindow::SUSP;
      if (read_bool(T, feature::def::limit_up_spec, a, d))
        f |= MarketWindow::LIM_UP;
      if (read_bool(T, feature::def::limit_dn_spec, a, d))
        f |= MarketWindow::LIM_DN;
      // delist_age 是 ex-post 表拉的最新退市日, 但 feature 层已做 PIT 截断 —
      //   仅 D ≥ delist_date 写值 (≥ 0), 否则 NaN ⇒ finite 即"今日已退市".
      float da = T.at(feature::def::delist_age_spec, a, d);
      if (feature::is_finite(da)) {
        assert(da >= 0.0f && "delist_age finite ⇒ ≥ 0 (PIT contract)");
        f |= MarketWindow::DELISTED;
      }
      mk.flags[off] = f;
    }
  }

  return mk;
}

} // namespace backtest
