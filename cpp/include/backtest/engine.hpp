#pragma once

#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// ============================================================================
// 回测内核 — "候选序 → 持仓 → NAV" 这条决策+记账链的**唯一实现**.
//
// 两条调用路径共用本内核, 保证口径不可能漂移:
//   报告路径 backtest::run  — 候选序来自策略 rank 列, Recorder 记全套明细
//   挖掘路径 mine::run      — 候选序来自逐权重现算的 top-K, Recorder 空
// 两者只在 "候选序怎么来" 与 "记录什么" 上不同, 现金/仓位/成本/涨跌停约束
// 全部落在 Engine::step 里.
//
// 与拆分前的差异 (仅此一处, 语义等价): 持仓表由 unordered_map<int,double> 换成
//   插入序稳定的 vector<Pos> ⇒ 浮点累加顺序不再依赖哈希桶实现, 两条路径逐位
//   一致. 相对旧输出会有 ~1e-16 量级的累加顺序差 (数学上等价).
// ============================================================================
namespace backtest {

// 市场状态窗口 — per-(i, a) 只读快照, [i][a] 行主序 (i = 窗口内日索引).
//   close: last_close 语义 (窗口内 ffill 的最近收盘; NaN = 窗口内从未有价),
//          与逐日刷缓存完全等价, 但只算一次 ⇒ 挖掘路径跨权重共享.
//   flags: 4 个契约 bool 压成一字节.
struct MarketWindow {
  enum : std::uint8_t {
    SUSP = 1,
    LIM_UP = 2,
    LIM_DN = 4,
    DELISTED = 8, // delist_age finite ⇔ PIT 上当日已退市
  };

  int d_lo = 0; // 窗口左端 (axes D 全局索引)
  int n_d = 0;  // 窗口天数
  int n_a = 0;
  std::vector<float> close;
  std::vector<std::uint8_t> flags;

  const float *close_row(int i) const {
    assert(i >= 0 && i < n_d);
    return close.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(n_a);
  }
  const std::uint8_t *flag_row(int i) const {
    assert(i >= 0 && i < n_d);
    return flags.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(n_a);
  }
};

// 从 Tensor 抽出 [d_lo, d_lo + n_d) 窗口的市场状态.
MarketWindow load_market(const feature::Axes &axes, const feature::Tensor &T,
                         int d_lo, int n_d);

// 单个持仓. shares 为 float 仓位 (不取整); to_sell 是当日卖出意向标记.
struct Pos {
  int a;
  double shares;
  bool to_sell;
};

// 空 Recorder (挖掘路径) — 钩子全部编译期消掉, 零开销.
struct NullRecorder {
  void on_delist_sell(int, int, float, double) {}
  void on_sell(int, int, float) {}
  void on_buy(int, int, float) {}
};

// 由三个 callable 组装的 Recorder (报告路径用): lambda 直接捕获 run() 的局部
//   明细容器, 无分配无虚调用. 钩子参数统一 (a, i, 成交价[, 卖出股数]);
//   i 是窗口内日索引, 全局 D 由调用方自行加 d_lo.
template <class FD, class FS, class FB>
struct Recorder {
  FD delist;
  FS sell;
  FB buy;
  void on_delist_sell(int a, int i, float px, double shares) {
    delist(a, i, px, shares);
  }
  void on_sell(int a, int i, float px) { sell(a, i, px); }
  void on_buy(int a, int i, float px) { buy(a, i, px); }
};

template <class FD, class FS, class FB>
Recorder<FD, FS, FB> make_recorder(FD d, FS s, FB b) {
  return Recorder<FD, FS, FB>{d, s, b};
}

class Engine {
public:
  Engine(const MarketWindow &mk, int hn, float er)
      : hold_n(hn), exit_ratio(er), mk_(mk) {
    // hold_n 是目标持仓数, 但卖出可能被停牌挡住 ⇒ 实际持仓数可短暂超出
    // (实测上限 hold_n + 2). 预留一整个 hold_n 余量, 跑飞由 step 末尾 assert 兜住.
    holdings.reserve(cap());
    buy_.reserve(static_cast<std::size_t>(hn) + 1);
    defs_.reserve(cap());
    reset();
  }

  // 回到窗口首日之前的初态 (挖掘路径逐权重复用同一个 Engine, 免去重复分配).
  void reset() {
    cash = static_cast<double>(::config::BACKTEST_CAPITAL_BASE);
    holdings.clear();
    pv = mv_end = pv_end = turn_amt = 0.0;
    n_sell_intent = n_buy_intent = n_sell_ok = n_buy_ok = 0;
  }

  // 走窗口内第 i 天.
  //   cands = 当日候选, 按 (score 降序, 并列 a 升序) — 与策略 rank 列同序.
  //           内核只读前 max(n_top, n_top_exit) 项与 cands.size(),
  //           故挖掘路径只需算出 top-(hold_n × exit_ratio) 即可.
  template <class Rec>
  void step(int i, std::span<const std::pair<float, int>> cands, Rec &rec);

  // ---- 状态 (跨日强 causal) ----
  int hold_n;
  float exit_ratio;
  double cash = 0.0;
  std::vector<Pos> holdings; // 插入序稳定

  // ---- step 后可读的当日量 ----
  double pv = 0.0;       // 决策时点组合市值 (换手率分母)
  double mv_end = 0.0;   // 执行后持仓市值 (仓位分子)
  double pv_end = 0.0;   // 当日终值 = NAV = cash + mv_end
  double turn_amt = 0.0; // 当日买卖额 (元)
  int n_sell_intent = 0;
  int n_buy_intent = 0;
  int n_sell_ok = 0;
  int n_buy_ok = 0;

  const Pos *find(int a) const {
    for (const Pos &p : holdings) {
      if (p.a == a)
        return &p;
    }
    return nullptr;
  }
  Pos *find(int a) {
    for (Pos &p : holdings) {
      if (p.a == a)
        return &p;
    }
    return nullptr;
  }

private:
  struct Deficit {
    double def;
    int a;
    float c;
  };

  std::size_t cap() const { return static_cast<std::size_t>(hold_n) * 2 + 8; }

  const MarketWindow &mk_;
  std::vector<int> buy_;
  std::vector<Deficit> defs_;
};

template <class Rec>
void Engine::step(int i, std::span<const std::pair<float, int>> cands, Rec &rec) {
  using feature::is_finite;
  const float *close = mk_.close_row(i);
  const std::uint8_t *flag = mk_.flag_row(i);
  turn_amt = 0.0;

  // (1) 已退市持仓强平 (按 last_close).
  //   PIT 安全: delist_age 只在 D ≥ delist_date 写值 ⇒ DELISTED 位即"今日已退市".
  //   触发说明上游 filter 未能在退市前卖出 (可能一直停牌 / 数据源缺 ST 标记);
  //   不 assert, 由 Recorder 决定是否告警.
  for (std::size_t k = 0; k < holdings.size();) {
    int a = holdings[k].a;
    if ((flag[a] & MarketWindow::DELISTED) == 0) {
      ++k;
      continue;
    }
    float c = close[a];
    assert(is_finite(c) && "delisted holding has no last_close");
    double sh = holdings[k].shares;
    turn_amt += sh * static_cast<double>(c);
    cash += sh * static_cast<double>(c) * (1.0 - ::config::BACKTEST_SELL_COST);
    rec.on_delist_sell(a, i, c, sh);
    holdings.erase(holdings.begin() + static_cast<std::ptrdiff_t>(k));
  }

  // (2) 组合市值 (mark-to-market)
  double mv_holdings = 0.0;
  for (const Pos &p : holdings) {
    float c = close[p.a];
    assert(is_finite(c) && "holding without close — bought before list_date?");
    mv_holdings += p.shares * static_cast<double>(c);
  }
  pv = cash + mv_holdings;
  assert(pv > 0.0 && "portfolio value <= 0");

  // (3) top-N / top-exit 界 (cands 已是 rank 序 ⇒ 集合就是前缀)
  int n_c = static_cast<int>(cands.size());
  int n_top = std::min(hold_n, n_c);
  int n_top_exit =
      std::min(static_cast<int>(static_cast<float>(hold_n) * exit_ratio), n_c);
  auto in_prefix = [&](int a, int n) {
    for (int k = 0; k < n; ++k) {
      if (cands[static_cast<std::size_t>(k)].second == a)
        return true;
    }
    return false;
  };

  // (4) 卖出意向: 持仓中掉出 top_exit, 且不被涨跌停主动排除.
  //     limit_up = 想留 (赌 T+1); limit_dn = 卖不出 (物理) — 二者都不卖.
  n_sell_intent = 0;
  int kept = 0;
  for (Pos &p : holdings) {
    bool blocked = (flag[p.a] & (MarketWindow::LIM_UP | MarketWindow::LIM_DN)) != 0;
    p.to_sell = !in_prefix(p.a, n_top_exit) && !blocked;
    if (p.to_sell)
      ++n_sell_intent;
    else
      ++kept;
  }

  // 买入意向: 补满 hold_n - kept 个空槽, 按 rank 序取.
  int slots = hold_n - kept;
  buy_.clear();
  if (slots > 0) {
    for (int k = 0; k < n_c; ++k) {
      if (slots <= 0)
        break;
      int a = cands[static_cast<std::size_t>(k)].second;
      const Pos *hp = find(a);
      if (hp != nullptr && !hp->to_sell)
        continue; // 已持仓且未卖
      if (k >= n_top)
        break; // 已超 top N 范围 (cands 按 rank 序)
      if ((flag[a] & (MarketWindow::LIM_UP | MarketWindow::LIM_DN)) != 0)
        continue;
      buy_.push_back(a);
      --slots;
    }
  }
  n_buy_intent = static_cast<int>(buy_.size());

  // (5) 执行卖出 — 停牌持仓订单失败 (无价同理).
  n_sell_ok = 0;
  for (std::size_t k = 0; k < holdings.size();) {
    if (!holdings[k].to_sell) {
      ++k;
      continue;
    }
    int a = holdings[k].a;
    float c = close[a];
    if ((flag[a] & MarketWindow::SUSP) != 0 || !is_finite(c)) {
      ++k;
      continue;
    }
    double sh = holdings[k].shares;
    turn_amt += sh * static_cast<double>(c);
    cash += sh * static_cast<double>(c) * (1.0 - ::config::BACKTEST_SELL_COST);
    rec.on_sell(a, i, c);
    holdings.erase(holdings.begin() + static_cast<std::ptrdiff_t>(k));
    ++n_sell_ok;
  }

  // (6) 执行买入 + 再平衡 (单一 target_per_slot 口径, 不设门槛 — 后续接大盘
  //   择时只需在此处缩放 target_per_slot / 跳过整段).
  //   1. pv_after = cash + mv_kept (sells 后市值; buy_ 尚未执行).
  //      target_per_slot = pv_after / hold_n (含费总支出, 持仓权重 ≈ 1/hold_n).
  //   2. initial buy: 每个 buy_ 至多花 target_per_slot, 不强行用完 cash.
  //   3. 再平衡 = 现金清扫二合一: 现有持仓 (kept + 新 buy) 中 deficit > 0 的按
  //      deficit 降序补到 target 直到 cash 耗尽 — 剩余 cash 当天全部投出去,
  //      逼近满仓. 跳过 susp/limit_up/limit_dn (买不进属物理约束).
  //      加仓不产生 fill/trade 记录, 不计入 n_buy_ok; 换手按成交额计.
  double mv_kept = 0.0;
  for (const Pos &p : holdings)
    mv_kept += p.shares * static_cast<double>(close[p.a]);
  double pv_after = cash + mv_kept;
  double target_per_slot = pv_after / static_cast<double>(hold_n);

  n_buy_ok = 0;
  for (int a : buy_) {
    if (cash <= 0.0)
      break;
    float c = close[a];
    if (!is_finite(c) || c <= 0.0f)
      continue;
    double cost_money = std::min(target_per_slot, cash);
    double sh = cost_money / (1.0 + ::config::BACKTEST_BUY_COST) /
                static_cast<double>(c);
    if (sh <= 0.0)
      continue;
    cash -= cost_money;
    turn_amt += cost_money;
    holdings.push_back(Pos{a, sh, false}); // buy_ ∉ holdings (见 (4))
    rec.on_buy(a, i, c);
    ++n_buy_ok;
  }

  if (cash > 0.0 && !holdings.empty()) {
    defs_.clear();
    for (const Pos &p : holdings) {
      float c = close[p.a];
      if (!is_finite(c) || c <= 0.0f)
        continue;
      if ((flag[p.a] & (MarketWindow::SUSP | MarketWindow::LIM_UP |
                        MarketWindow::LIM_DN)) != 0)
        continue;
      double def = target_per_slot - p.shares * static_cast<double>(c);
      if (def <= 0.0)
        continue;
      defs_.push_back(Deficit{def, p.a, c});
    }
    std::sort(defs_.begin(), defs_.end(),
              [](const Deficit &x, const Deficit &y) { return x.def > y.def; });
    for (const Deficit &dd : defs_) {
      if (cash <= 0.0)
        break;
      double cost_money = std::min(dd.def, cash);
      double sh_add = cost_money / (1.0 + ::config::BACKTEST_BUY_COST) /
                      static_cast<double>(dd.c);
      if (sh_add <= 0.0)
        continue;
      cash -= cost_money;
      turn_amt += cost_money;
      find(dd.a)->shares += sh_add; // 加仓; Recorder 不记
    }
  }

  // (7) 当日终值
  mv_end = 0.0;
  for (const Pos &p : holdings)
    mv_end += p.shares * static_cast<double>(close[p.a]);
  pv_end = cash + mv_end;
  assert(holdings.size() <= cap() &&
         "持仓数远超 hold_n — 卖出被长期阻塞, 空槽仍在补, 说明约束链有问题");
}

} // namespace backtest
