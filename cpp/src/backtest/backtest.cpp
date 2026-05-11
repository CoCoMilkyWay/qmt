#include "backtest/backtest.hpp"

#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/timer.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace backtest {

namespace fs = std::filesystem;

namespace {

using feature::F;
using feature::is_finite;

// ---- helpers ---------------------------------------------------------------

// 读"契约 bool" feature: 必 finite + ∈ {0, 1}. 任一违背 → assert fail (定位污染源).
//   适用: tradable / pool / susp / limit_up / limit_dn 等 NO_NAN_FEATURES 子集.
//   raw / factor / daily_return 等可 NaN 列禁用此 helper.
inline bool read_bool(const feature::Tensor &T, F f, int a, int d) {
  float v = T.at(f, a, d);
  assert(is_finite(v) && "backtest::read_bool: NaN — feature should be 0/1");
  return v > 0.5f;
}

inline int find_d(const feature::Axes &axes, std::string_view yyyymmdd,
                  bool floor) {
  // floor=true: 找 ≤ yyyymmdd 的最大索引; false: 找 ≥ 的最小.
  if (floor) return axes.floor_date(yyyymmdd);
  auto it = std::lower_bound(axes.dates.begin(), axes.dates.end(), yyyymmdd);
  return (it == axes.dates.end())
             ? -1
             : static_cast<int>(std::distance(axes.dates.begin(), it));
}

// 写一个 1-D float npy
inline void wf(const fs::path &p, const std::vector<float> &v) {
  std::size_t shape[1] = {v.size()};
  misc::write_npy_f4(p, std::span<const float>(v.data(), v.size()),
                     std::span<const std::size_t>(shape, 1));
}

// 写一个 1-D int32 npy
inline void wi(const fs::path &p, const std::vector<std::int32_t> &v) {
  std::size_t shape[1] = {v.size()};
  misc::write_npy_i4(p, std::span<const std::int32_t>(v.data(), v.size()),
                     std::span<const std::size_t>(shape, 1));
}

} // namespace

double run(const feature::Axes &axes, const feature::StockMeta & /*meta*/,
           const feature::Tensor &T) {
  // 注: meta.delist_date 是 ex-post (build 时拉的最新表), `delist_age < 0` 含未来信息
  //     (今日已知未来某日退市), 不可用作 filter; 但 `delist_age >= 0` 是当下事实 (今日已退市),
  //     PIT 安全, 仅用于持仓兜底强制平仓.
  misc::Timer t("[backtest] run");
  auto t0 = std::chrono::high_resolution_clock::now();

  // ---- 解析回测窗口 (闭区间) ----------------------------------------------
  int bt_d_lo = find_d(axes, ::config::BACKTEST_START_DATE, /*floor=*/false);
  int bt_d_hi_inc = find_d(axes, ::config::BACKTEST_END_DATE, /*floor=*/true);
  assert(bt_d_lo >= 0 && bt_d_hi_inc >= bt_d_lo &&
         bt_d_hi_inc < axes.n_d() && "backtest window invalid");
  int bt_d_hi = bt_d_hi_inc + 1; // half-open
  int n_d_bt = bt_d_hi - bt_d_lo;
  int n_a = axes.n_a();

  // ---- 状态 ----------------------------------------------------------------
  std::unordered_map<int, double> holdings; // a → shares (float 仓位, 不取整)
  double cash = ::config::BT_CAPITAL_BASE;
  std::vector<float> last_close(static_cast<std::size_t>(n_a),
                                std::nanf("")); // mark-to-market 兜底

  // 每日输出
  std::vector<std::int32_t> dates_out(n_d_bt);
  std::vector<float> strat_nav(n_d_bt), pool_nav(n_d_bt);
  std::vector<std::int32_t> pos_count(n_d_bt);
  std::vector<float> pos_pct(n_d_bt), turnover(n_d_bt);
  std::vector<float> susp_pct(n_d_bt), exec_pct(n_d_bt);

  // CSR 持仓 (按 d 顺序; 每 d 一段)
  std::vector<std::int32_t> hold_off(static_cast<std::size_t>(n_d_bt) + 1, 0);
  std::vector<std::int32_t> hold_codes;
  std::vector<float> hold_weights;
  hold_codes.reserve(static_cast<std::size_t>(n_d_bt) * ::config::BT_HOLD_N);
  hold_weights.reserve(static_cast<std::size_t>(n_d_bt) * ::config::BT_HOLD_N);

  // 成交 (open-close 配对)
  struct OpenRec {
    int open_d;
    float open_px;
    float buy_value;        // 实际买入金额 (含费)
    float pv_at_open;       // 开仓时组合市值
  };
  std::unordered_map<int, OpenRec> open_recs;

  std::vector<std::int32_t> tr_inst, tr_open_d, tr_close_d;
  std::vector<float> tr_open_px, tr_close_px, tr_buy_value, tr_pv_at_open;

  // pool benchmark NAV
  double pool_nav_d = ::config::BT_CAPITAL_BASE;

  // ---- 主循环 --------------------------------------------------------------
  for (int i = 0; i < n_d_bt; ++i) {
    int d = bt_d_lo + i;
    dates_out[i] = d;

    // (1) 更新 last_close 缓存 (T 的 close_raw 已 ffill, finite 保留)
    for (int a = 0; a < n_a; ++a) {
      float c = T.at(F::close_raw, a, d);
      if (is_finite(c)) last_close[a] = c;
    }

    // (1.5) 持仓兜底: 已退市股 (delist_age >= 0) 强制按 last_close 平仓.
    //   PIT 安全: 仅在退市当日及之后命中, delist_age < 0 不读 (不用未来信息).
    //   触发说明 上游 filter 未能在退市前卖出该持仓 (可能因停牌一直卡住 / 数据源缺 ST 标记).
    //   不 assert, 打印 [WARN] 后继续, 与 sell 逻辑一致地关闭 trade record.
    for (auto it = holdings.begin(); it != holdings.end();) {
      int a = it->first;
      float da = T.at(F::delist_age, a, d);
      if (!is_finite(da) || da < 0.0f) {
        ++it;
        continue;
      }
      float c = last_close[static_cast<std::size_t>(a)];
      assert(is_finite(c) && "delisted holding has no last_close");
      std::printf("[WARN] backtest d=%s 持仓 %s 已退市 (delist_age=%.0f), "
                  "强制按 last_close=%.4f 平仓\n",
                  axes.dates[static_cast<std::size_t>(d)].c_str(),
                  axes.codes[static_cast<std::size_t>(a)].c_str(),
                  da, c);
      double proceeds = it->second * static_cast<double>(c) *
                        (1.0 - ::config::BT_SELL_COST);
      cash += proceeds;

      auto rec_it = open_recs.find(a);
      assert(rec_it != open_recs.end() &&
             "delisted holding without open record");
      tr_inst.push_back(static_cast<std::int32_t>(a));
      tr_open_d.push_back(static_cast<std::int32_t>(rec_it->second.open_d));
      tr_close_d.push_back(static_cast<std::int32_t>(d));
      tr_open_px.push_back(rec_it->second.open_px);
      tr_close_px.push_back(c);
      tr_buy_value.push_back(rec_it->second.buy_value);
      tr_pv_at_open.push_back(rec_it->second.pv_at_open);
      open_recs.erase(rec_it);

      it = holdings.erase(it);
    }

    // (2) 组合市值 (mark-to-market)
    double mv_holdings = 0.0;
    for (auto &kv : holdings) {
      float c = last_close[static_cast<std::size_t>(kv.first)];
      assert(is_finite(c) && "holding without close — bought before list_date?");
      mv_holdings += kv.second * static_cast<double>(c);
    }
    double pv = cash + mv_holdings;
    assert(pv > 0.0 && "portfolio value <= 0");

    // (3) 候选 universe: tradable ∧ finite(factor_score)
    //   factor_score 在 pool 外是 NaN (按 cs_factor_score 契约), 跳过即可;
    //   pool 内全 factor 缺时也可能 NaN (理论上 pool 已保 mcap_raw finite → 不应触发).
    std::vector<std::pair<float, int>> cands;
    cands.reserve(::config::UNIVERSE_SIZE);
    for (int a = 0; a < n_a; ++a) {
      if (!read_bool(T, F::tradable, a, d)) continue;
      float s = T.at(F::factor_score, a, d);
      if (!is_finite(s)) continue;
      cands.emplace_back(s, a);
    }
    std::sort(cands.begin(), cands.end(),
              [](const auto &x, const auto &y) { return x.first > y.first; });

    int hold_n = ::config::BT_HOLD_N;
    int n_top = std::min(hold_n, static_cast<int>(cands.size()));
    int n_top_exit =
        std::min(static_cast<int>(static_cast<float>(hold_n) *
                                  ::config::BT_EXIT_RATIO),
                 static_cast<int>(cands.size()));
    if (n_top_exit > static_cast<int>(cands.size()))
      n_top_exit = static_cast<int>(cands.size());

    std::unordered_set<int> top_n_set, top_exit_set;
    top_n_set.reserve(static_cast<std::size_t>(n_top));
    top_exit_set.reserve(static_cast<std::size_t>(n_top_exit));
    for (int k = 0; k < n_top; ++k) top_n_set.insert(cands[k].second);
    for (int k = 0; k < n_top_exit; ++k) top_exit_set.insert(cands[k].second);

    // (4) 决定 to_sell / to_buy ------------------------------------------------
    // sell: holdings 中不在 top_exit, 且不被 limit_up/limit_dn 主动排除.
    //       limit_up = 想留 (赌 T+1); limit_dn = 卖不出 (物理) — 二者都不卖.
    std::vector<int> intended_sell, intended_buy;
    intended_sell.reserve(holdings.size());
    for (auto &kv : holdings) {
      int a = kv.first;
      if (top_exit_set.count(a)) continue;
      if (read_bool(T, F::limit_up, a, d) || read_bool(T, F::limit_dn, a, d))
        continue;
      intended_sell.push_back(a);
    }
    // 决定空槽 — 仍持有 (not in to_sell) 占的槽
    std::unordered_set<int> sold_set(intended_sell.begin(),
                                     intended_sell.end());
    int kept = 0;
    for (auto &kv : holdings) {
      if (!sold_set.count(kv.first)) ++kept;
    }
    int slots = hold_n - kept;
    if (slots > 0) {
      for (const auto &p : cands) {
        if (slots <= 0) break;
        int a = p.second;
        if (holdings.count(a) && !sold_set.count(a)) continue; // 已持仓且未卖
        if (!top_n_set.count(a)) break; // 已超 top N 范围 (排序后)
        if (read_bool(T, F::limit_up, a, d) || read_bool(T, F::limit_dn, a, d))
          continue;
        intended_buy.push_back(a);
        --slots;
      }
    }

    // (5) 执行: sell 受 susp 阻挡 — 停牌持仓订单失败.
    int n_sell_ok = 0, n_buy_ok = 0;
    int n_sell_intent = static_cast<int>(intended_sell.size());
    int n_buy_intent = static_cast<int>(intended_buy.size());

    for (int a : intended_sell) {
      bool susp = read_bool(T, F::susp, a, d);
      float c = last_close[static_cast<std::size_t>(a)];
      if (susp || !is_finite(c)) continue; // 失败: 停牌或无价
      double sh = holdings[a];
      double proceeds = sh * static_cast<double>(c) *
                        (1.0 - ::config::BT_SELL_COST);
      cash += proceeds;
      holdings.erase(a);
      ++n_sell_ok;

      // 关 trade
      auto it = open_recs.find(a);
      assert(it != open_recs.end() && "sell without open record");
      tr_inst.push_back(static_cast<std::int32_t>(a));
      tr_open_d.push_back(static_cast<std::int32_t>(it->second.open_d));
      tr_close_d.push_back(static_cast<std::int32_t>(d));
      tr_open_px.push_back(it->second.open_px);
      tr_close_px.push_back(c);
      tr_buy_value.push_back(it->second.buy_value);
      tr_pv_at_open.push_back(it->second.pv_at_open);
      open_recs.erase(it);
    }

    // 重算 mv (sells 后 cash 变了, mv 不变 — 但持仓减少)
    // buys: 把可用资金按 intended_buy 数等分 (与 strategy.py 一致)
    double pv_after_sell = cash;
    for (auto &kv : holdings) {
      pv_after_sell += kv.second *
                       static_cast<double>(last_close[
                           static_cast<std::size_t>(kv.first)]);
    }
    double used_value = pv_after_sell - cash; // 仍持仓市值
    double available = pv_after_sell - used_value;
    if (n_buy_intent > 0 && available > 0.0) {
      double per_slot = available / static_cast<double>(n_buy_intent);
      for (int a : intended_buy) {
        float c = last_close[static_cast<std::size_t>(a)];
        if (!is_finite(c) || c <= 0.0f) continue; // 无价无法买
        double cost_money = per_slot;             // 含费总支出
        double net = cost_money / (1.0 + ::config::BT_BUY_COST);
        double sh = net / static_cast<double>(c);
        if (sh <= 0.0) continue;
        cash -= cost_money;
        holdings[a] = holdings.count(a) ? holdings[a] + sh : sh;
        ++n_buy_ok;

        OpenRec rec;
        rec.open_d = d;
        rec.open_px = c;
        rec.buy_value = static_cast<float>(cost_money);
        rec.pv_at_open = static_cast<float>(pv);
        open_recs[a] = rec;
      }
    }

    // (6) 收尾: 当日终值 (执行后 mv) ----------------------------------------
    double mv_end = 0.0;
    for (auto &kv : holdings) {
      mv_end += kv.second *
                static_cast<double>(last_close[
                    static_cast<std::size_t>(kv.first)]);
    }
    double pv_end = cash + mv_end;
    strat_nav[i] = static_cast<float>(pv_end);

    // pool benchmark: pool[a, d] 内等权 daily_return[a, d].
    //   口径与 strategy 对齐 — day 0 起点 = capital_base (假设当日盘后买入 pool),
    //   daily_return 仅 i > 0 才累积 (i=1 用 daily_return[d_lo+1] 实现 close→close 收益).
    double dr_sum = 0.0;
    int dr_n = 0;
    for (int a = 0; a < n_a; ++a) {
      if (!read_bool(T, F::pool, a, d)) continue;
      float r = T.at(F::daily_return, a, d);
      if (!is_finite(r)) continue; // daily_return 预期可 NaN (d==0 / close_raw 缺)
      dr_sum += static_cast<double>(r);
      ++dr_n;
    }
    double dr = dr_n > 0 ? dr_sum / static_cast<double>(dr_n) : 0.0;
    if (i > 0) pool_nav_d *= (1.0 + dr);
    pool_nav[i] = static_cast<float>(pool_nav_d);

    // 持仓数 / 仓位 / 换手 / 停牌占比 / 可执行率
    pos_count[i] = static_cast<std::int32_t>(holdings.size());
    pos_pct[i] = static_cast<float>(mv_end / pv_end);
    int trades_today = n_sell_ok + n_buy_ok;
    turnover[i] = static_cast<float>(trades_today) /
                  static_cast<float>(::config::BT_HOLD_N);
    int n_susp_h = 0;
    for (auto &kv : holdings) {
      if (read_bool(T, F::susp, kv.first, d)) ++n_susp_h;
    }
    susp_pct[i] = holdings.empty()
                      ? 0.0f
                      : static_cast<float>(n_susp_h) /
                            static_cast<float>(holdings.size());
    int intent = n_sell_intent + n_buy_intent;
    exec_pct[i] = (intent == 0)
                      ? 1.0f
                      : static_cast<float>(n_sell_ok + n_buy_ok) /
                            static_cast<float>(intent);

    // CSR 持仓 (按 d 写一段)
    std::vector<std::pair<int, double>> sorted_hold(holdings.begin(),
                                                    holdings.end());
    std::sort(sorted_hold.begin(), sorted_hold.end(),
              [](const auto &x, const auto &y) { return x.first < y.first; });
    for (auto &kv : sorted_hold) {
      int a = kv.first;
      double sh = kv.second;
      double mv = sh * static_cast<double>(last_close[
                            static_cast<std::size_t>(a)]);
      hold_codes.push_back(static_cast<std::int32_t>(a));
      hold_weights.push_back(static_cast<float>(mv / pv_end));
    }
    hold_off[i + 1] = static_cast<std::int32_t>(hold_codes.size());
  }

  // ---- 写盘 ----------------------------------------------------------------
  fs::path out = misc::git_root() / "output" / "backtest";
  fs::create_directories(out);

  wi(out / "dates.npy", dates_out);
  wf(out / "strategy_nav.npy", strat_nav);
  wf(out / "pool_nav.npy", pool_nav);
  wi(out / "position_count.npy", pos_count);
  wf(out / "position_pct.npy", pos_pct);
  wf(out / "turnover.npy", turnover);
  wf(out / "susp_pct.npy", susp_pct);
  wf(out / "executable_pct.npy", exec_pct);

  wi(out / "holdings_offsets.npy", hold_off);
  wi(out / "holdings_codes.npy", hold_codes);
  wf(out / "holdings_weights.npy", hold_weights);

  wi(out / "trades_inst.npy", tr_inst);
  wi(out / "trades_open_d.npy", tr_open_d);
  wi(out / "trades_close_d.npy", tr_close_d);
  wf(out / "trades_open_px.npy", tr_open_px);
  wf(out / "trades_close_px.npy", tr_close_px);
  wf(out / "trades_buy_value.npy", tr_buy_value);
  wf(out / "trades_pv_at_open.npy", tr_pv_at_open);

  auto t1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = t1 - t0;
  return dur.count();
}

} // namespace backtest
