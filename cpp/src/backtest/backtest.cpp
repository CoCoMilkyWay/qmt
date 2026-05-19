#include "backtest/backtest.hpp"

#include "config.hpp"
#include "feature/axis.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/timer.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
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
//   适用: tradable / pool / susp / limit_up / limit_dn 等 BUILD_NO_NAN_FEATURES 子集.
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

inline std::string json_str(yyjson_val *obj, const char *key) {
  yyjson_val *v = yyjson_obj_get(obj, key);
  assert(v && yyjson_is_str(v));
  return std::string(yyjson_get_str(v), yyjson_get_len(v));
}

struct NameInterval {
  int lo;
  int hi;
  std::string name;
};

struct NameTimeline {
  std::vector<std::vector<NameInterval>> by_a;
};

std::vector<fs::path> enumerate_name_change_files() {
  std::vector<fs::path> files;
  fs::path data_root = misc::git_root() / "data";
  assert(fs::exists(data_root));

  for (auto &y_ent : fs::directory_iterator(data_root)) {
    if (!y_ent.is_directory()) continue;
    std::string y = y_ent.path().filename().string();
    if (y.size() != 4) continue;
    for (auto &m_ent : fs::directory_iterator(y_ent.path())) {
      if (!m_ent.is_directory()) continue;
      std::string m = m_ent.path().filename().string();
      if (m.size() != 2) continue;
      for (auto &d_ent : fs::directory_iterator(m_ent.path())) {
        if (!d_ent.is_directory()) continue;
        fs::path p = d_ent.path() / "cn_stock_name_change.json";
        if (fs::exists(p)) files.push_back(std::move(p));
      }
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

void add_name_interval(const feature::Axes &axes, std::vector<NameInterval> &v,
                       std::string_view start, std::string_view end,
                       std::string name) {
  assert(start.size() == 8 && end.size() == 8 && !name.empty());
  int lo = find_d(axes, start, /*floor=*/false);
  int hi = find_d(axes, end, /*floor=*/true);
  if (lo < 0 || hi < lo) return;
  v.push_back(NameInterval{lo, hi, std::move(name)});
}

NameTimeline load_name_timeline(const feature::Axes &axes) {
  int n_a = axes.n_a();
  NameTimeline tl;
  tl.by_a.resize(static_cast<std::size_t>(n_a));
  std::vector<std::string> last_end(static_cast<std::size_t>(n_a));

  for (const fs::path &p : enumerate_name_change_files()) {
    std::string buf = misc::read_file_all(p);
    assert(!buf.empty());
    yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
    assert(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    assert(yyjson_is_arr(root));

    std::size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(root, i, n, item) {
      std::string ins = json_str(item, "instrument");
      auto it = axes.code_idx.find(ins);
      if (it == axes.code_idx.end()) continue;
      int a = it->second;
      std::string start = json_str(item, "start_date");
      std::string end = json_str(item, "end_date");
      std::string name = json_str(item, "name");
      add_name_interval(axes, tl.by_a[static_cast<std::size_t>(a)], start, end,
                        std::move(name));
      std::string &mx = last_end[static_cast<std::size_t>(a)];
      if (mx.empty() || end > mx) mx = std::move(end);
    }
    yyjson_doc_free(doc);
  }

  fs::path static_path =
      misc::git_root() / "data" / "_meta" / "cn_stock_static_data.json";
  assert(fs::exists(static_path));
  std::string static_buf = misc::read_file_all(static_path);
  assert(!static_buf.empty());
  yyjson_doc *doc = yyjson_read(static_buf.data(), static_buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_arr(root));

  std::size_t i, n;
  yyjson_val *item;
  yyjson_arr_foreach(root, i, n, item) {
    std::string ins = json_str(item, "instrument");
    auto it = axes.code_idx.find(ins);
    if (it == axes.code_idx.end()) continue;
    int a = it->second;
    const std::string &mx = last_end[static_cast<std::size_t>(a)];
    std::string start = mx.empty() ? axes.dates.front() : misc::add_days(mx, 1);
    add_name_interval(axes, tl.by_a[static_cast<std::size_t>(a)], start,
                      axes.dates.back(), json_str(item, "name"));
  }
  yyjson_doc_free(doc);

  for (auto &v : tl.by_a) {
    std::sort(v.begin(), v.end(), [](const NameInterval &x,
                                     const NameInterval &y) {
      return x.lo < y.lo;
    });
  }
  return tl;
}

inline std::string_view name_of(const NameTimeline &tl,
                                const feature::StockMeta &meta, int a, int d) {
  const auto &v = tl.by_a[static_cast<std::size_t>(a)];
  for (auto it = v.rbegin(); it != v.rend(); ++it) {
    if (it->lo <= d && d <= it->hi) return it->name;
  }
  return meta.name[static_cast<std::size_t>(a)];
}

} // namespace

double run(const feature::Axes &axes, const feature::StockMeta &meta,
           const feature::Tensor &T) {
  // 注: meta.delist_date 是 ex-post (build 时拉的最新表), 但 `delist_age` 已在
  //     feature 层做 PIT 截断 — 仅 D ≥ delist_date 写值 (≥ 0), 否则 NaN. 此处兜底
  //     用 is_finite 判 "今日已退市" 即可, 不需也不应去读"距退市天数".
  misc::Timer t("[backtest] run");
  auto t0 = std::chrono::high_resolution_clock::now();

  // ---- 解析回测窗口 (闭区间; 右端点固定为最新日) --------------------------
  int bt_d_lo = find_d(axes, ::config::BACKTEST_START_DATE, /*floor=*/false);
  int bt_d_hi_inc = axes.n_d() - 1;
  assert(bt_d_lo >= 0 && bt_d_hi_inc >= bt_d_lo &&
         bt_d_hi_inc < axes.n_d() && "backtest window invalid");
  int bt_d_hi = bt_d_hi_inc + 1; // half-open
  int n_d_bt = bt_d_hi - bt_d_lo;
  int n_a = axes.n_a();
  NameTimeline name_timeline = load_name_timeline(axes);

  // ---- 状态 ----------------------------------------------------------------
  std::unordered_map<int, double> holdings; // a → shares (float 仓位, 不取整)
  double cash = ::config::BACKTEST_CAPITAL_BASE;
  std::vector<float> last_close(static_cast<std::size_t>(n_a),
                                std::nanf("")); // mark-to-market 兜底

  // 每日输出
  std::vector<std::int32_t> dates_out(n_d_bt);
  std::vector<float> strat_nav(n_d_bt), pool_nav(n_d_bt);
  std::vector<std::int32_t> pos_count(n_d_bt);
  std::vector<float> pos_pct(n_d_bt), turnover(n_d_bt);
  std::vector<float> susp_pct(n_d_bt), exec_pct(n_d_bt);

  // CSR 持仓 (按 d 顺序; 每 d 一段; 段内按权重降序写入 — 便于 py 直接显示)
  std::vector<std::int32_t> hold_off(static_cast<std::size_t>(n_d_bt) + 1, 0);
  std::vector<std::int32_t> hold_codes;
  std::vector<float> hold_weights;
  std::vector<std::string> hold_names; // 与 hold_codes 同序, 按当日历史简称切段
  hold_codes.reserve(static_cast<std::size_t>(n_d_bt) * ::config::BACKTEST_HOLD_N);
  hold_weights.reserve(static_cast<std::size_t>(n_d_bt) * ::config::BACKTEST_HOLD_N);
  hold_names.reserve(static_cast<std::size_t>(n_d_bt) * ::config::BACKTEST_HOLD_N);

  // 成交 (open-close 配对)
  struct OpenRec {
    int open_d;
    float open_px;
  };
  std::unordered_map<int, OpenRec> open_recs;

  std::vector<std::int32_t> tr_inst, tr_open_d, tr_close_d;
  std::vector<float> tr_open_px, tr_close_px;
  std::vector<std::string> tr_open_names, tr_close_names; // 开/平仓当日历史简称

  // 关 trade 公用辅助 (强平 / 正常卖出 共用)
  auto close_trade = [&](int a, int d, const OpenRec &rec, float close_px) {
    tr_inst.push_back(static_cast<std::int32_t>(a));
    tr_open_d.push_back(static_cast<std::int32_t>(rec.open_d));
    tr_close_d.push_back(static_cast<std::int32_t>(d));
    tr_open_px.push_back(rec.open_px);
    tr_close_px.push_back(close_px);
    tr_open_names.emplace_back(name_of(name_timeline, meta, a, rec.open_d));
    tr_close_names.emplace_back(name_of(name_timeline, meta, a, d));
  };

  // pool benchmark NAV
  double pool_nav_d = ::config::BACKTEST_CAPITAL_BASE;

  // ---- 主循环 --------------------------------------------------------------
  for (int i = 0; i < n_d_bt; ++i) {
    int d = bt_d_lo + i;
    dates_out[i] = d;

    // (1) 更新 last_close 缓存 (T 的 close_raw 已 ffill, finite 保留)
    for (int a = 0; a < n_a; ++a) {
      float c = T.at(F::close_raw, a, d);
      if (is_finite(c)) last_close[a] = c;
    }

    // (1.5) 持仓兜底: 已退市股 (delist_age finite) 强制按 last_close 平仓.
    //   PIT 安全: feature 层已保证 delist_age 只在 D ≥ delist_date 写值, 否则 NaN.
    //   触发说明 上游 filter 未能在退市前卖出该持仓 (可能因停牌一直卡住 / 数据源缺 ST 标记).
    //   不 assert, 打印 [WARN] 后继续, 与 sell 逻辑一致地关闭 trade record.
    for (auto it = holdings.begin(); it != holdings.end();) {
      int a = it->first;
      float da = T.at(F::delist_age, a, d);
      if (!is_finite(da)) {
        ++it;
        continue;
      }
      assert(da >= 0.0f && "delist_age finite ⇒ ≥ 0 (PIT contract)");
      float c = last_close[static_cast<std::size_t>(a)];
      assert(is_finite(c) && "delisted holding has no last_close");

      auto rec_it = open_recs.find(a);
      assert(rec_it != open_recs.end() &&
             "delisted holding without open record");
      float open_px = rec_it->second.open_px;
      int hold_days = d - rec_it->second.open_d; // 交易日数 (axes D 索引差)
      float pnl_pct = (open_px > 0.0f)
                          ? (c / open_px - 1.0f) * 100.0f
                          : 0.0f;
      std::printf("[WARN] backtest d=%s 持仓 %s (%s) 已退市 "
                  "(delist_age=%.0f), 持有 %d 交易日, "
                  "开仓 %s@%.4f → 强平 @%.4f, 盈亏 %+.2f%%\n",
                  axes.dates[static_cast<std::size_t>(d)].c_str(),
                  axes.codes[static_cast<std::size_t>(a)].c_str(),
                  meta.name[static_cast<std::size_t>(a)].c_str(),
                  da, hold_days,
                  axes.dates[static_cast<std::size_t>(rec_it->second.open_d)].c_str(),
                  open_px, c, pnl_pct);
      double proceeds = it->second * static_cast<double>(c) *
                        (1.0 - ::config::BACKTEST_SELL_COST);
      cash += proceeds;
      close_trade(a, d, rec_it->second, c);
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
    //   factor_score 表达因子是否可算; universe 边界由 tradable/pool mask 表达.
    std::vector<std::pair<float, int>> cands;
    cands.reserve(::config::POOL_UNIVERSE_SIZE);
    for (int a = 0; a < n_a; ++a) {
      if (!read_bool(T, F::tradable, a, d)) continue;
      float s = T.at(F::factor_score, a, d);
      if (!is_finite(s)) continue;
      cands.emplace_back(s, a);
    }
    std::sort(cands.begin(), cands.end(),
              [](const auto &x, const auto &y) { return x.first > y.first; });

    int hold_n = ::config::BACKTEST_HOLD_N;
    int n_top = std::min(hold_n, static_cast<int>(cands.size()));
    int n_top_exit =
        std::min(static_cast<int>(static_cast<float>(hold_n) *
                                  ::config::BACKTEST_EXIT_RATIO),
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
                        (1.0 - ::config::BACKTEST_SELL_COST);
      cash += proceeds;
      holdings.erase(a);
      ++n_sell_ok;

      auto it = open_recs.find(a);
      assert(it != open_recs.end() && "sell without open record");
      close_trade(a, d, it->second, c);
      open_recs.erase(it);
    }

    // buys: sells 后再平衡逻辑 (合理降低交易次数).
    //   1. pv_after = cash + mv_kept (sells 后总组合市值; intended_buy 此时尚未执行).
    //      target_per_slot = pv_after / HOLD_N (作为含费总支出, 持仓权重 ≈ 1/HOLD_N,
    //      由 BUY_COST 折损; 可忽略). rebal_thd = BACKTEST_REBALANCE_THRESHOLD × pv_after.
    //   2. initial buy: 每个 intended_buy 至多花 target_per_slot, 不强行用完 cash.
    //      原 intended_buy 已在 (4) 过滤 limit_up/dn; close 兜底.
    //   3. 再平衡: 现有持仓 (kept + 新 buy) 中 deficit ≥ rebal_thd 的, 按 deficit
    //      降序逐个补到 target. 跳过 susp/limit_up/limit_dn (与 initial buy 同口径).
    //      加仓不创建 trade record、不更新 open_recs、不计入 n_buy_ok/exec_pct;
    //      单独累计 n_rebal_add 进 turnover (年换手).
    int n_rebal_add = 0;
    {
      double mv_kept = 0.0;
      for (auto &kv : holdings) {
        mv_kept += kv.second * static_cast<double>(last_close[
            static_cast<std::size_t>(kv.first)]);
      }
      double pv_after = cash + mv_kept;
      double target_per_slot = pv_after / static_cast<double>(hold_n);
      double rebal_thd =
          static_cast<double>(::config::BACKTEST_REBALANCE_THRESHOLD) * pv_after;

      for (int a : intended_buy) {
        if (cash <= 0.0) break;
        float c = last_close[static_cast<std::size_t>(a)];
        if (!is_finite(c) || c <= 0.0f) continue;
        double cost_money = std::min(target_per_slot, cash);
        double net = cost_money / (1.0 + ::config::BACKTEST_BUY_COST);
        double sh = net / static_cast<double>(c);
        if (sh <= 0.0) continue;
        cash -= cost_money;
        holdings[a] = sh; // intended_buy ∉ holdings (见 (4))
        open_recs[a] = OpenRec{d, c};
        ++n_buy_ok;
      }

      if (cash > 0.0 && !holdings.empty()) {
        struct Deficit {
          double def;
          int a;
          float c;
        };
        std::vector<Deficit> defs;
        defs.reserve(holdings.size());
        for (auto &kv : holdings) {
          int a = kv.first;
          float c = last_close[static_cast<std::size_t>(a)];
          if (!is_finite(c) || c <= 0.0f) continue;
          if (read_bool(T, F::susp, a, d) ||
              read_bool(T, F::limit_up, a, d) ||
              read_bool(T, F::limit_dn, a, d))
            continue;
          double cur_mv = kv.second * static_cast<double>(c);
          double def = target_per_slot - cur_mv;
          if (def < rebal_thd) continue;
          defs.push_back({def, a, c});
        }
        std::sort(defs.begin(), defs.end(),
                  [](const Deficit &x, const Deficit &y) {
                    return x.def > y.def;
                  });
        for (const auto &dd : defs) {
          if (cash <= 0.0) break;
          double cost_money = std::min(dd.def, cash);
          double net = cost_money / (1.0 + ::config::BACKTEST_BUY_COST);
          double sh_add = net / static_cast<double>(dd.c);
          if (sh_add <= 0.0) continue;
          cash -= cost_money;
          holdings[dd.a] += sh_add; // 加仓; open_recs 不动
          ++n_rebal_add;
        }
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

    // pool benchmark: "等权持有 pool 的影子策略" — 假装真有钱在跑.
    //   时点: D-1 日 close 按 pool[a, d-1] 等权再平衡 → 持有到 D 日 close
    //         获得 daily_return[a, d] = close[d]/close[d-1] - 1.
    //         pool 动态调入调出 → 每日按新 pool 等权重平衡 (等权下数学等价于均值,
    //         无需显式建仓/卖出簿记).
    //   universe: pool 已排 susp + 已退市 (pool_b); 此处额外排 risk_warn (ST/*ST).
    //         不排 profit_st / revenue_st / dividend_st / trading_st / new_list —
    //         这些是策略自定义风控信号, 不应假设 benchmark 也具备.
    //   PIT: 用 pool[d-1] 和 risk_warn[d-1] — D-1 收盘才能据此决策.
    //   NaN 处理: daily_return[d] NaN (退市/缺价) ⇒ 该持仓静默退出当日均值池.
    //   i=0: NAV = capital_base (尚未建仓); i>=1: 累乘.
    if (i > 0) {
      double dr_sum = 0.0;
      int dr_n = 0;
      int d_prev = d - 1;
      for (int a = 0; a < n_a; ++a) {
        if (!read_bool(T, F::pool, a, d_prev)) continue;
        float rw = T.at(F::risk_warn, a, d_prev);
        assert(is_finite(rw) && "risk_warn NaN — should be 0/1/2");
        if (rw > 0.5f) continue; // ST (1) / *ST (2) 排除
        float r = T.at(F::daily_return, a, d);
        if (!is_finite(r)) continue;
        dr_sum += static_cast<double>(r);
        ++dr_n;
      }
      double dr = dr_n > 0 ? dr_sum / static_cast<double>(dr_n) : 0.0;
      pool_nav_d *= (1.0 + dr);
    }
    pool_nav[i] = static_cast<float>(pool_nav_d);

    // 持仓数 / 仓位 / 换手 / 停牌占比 / 可执行率
    pos_count[i] = static_cast<std::int32_t>(holdings.size());
    pos_pct[i] = static_cast<float>(mv_end / pv_end);
    int trades_today = n_sell_ok + n_buy_ok + n_rebal_add;
    turnover[i] = static_cast<float>(trades_today) /
                  static_cast<float>(::config::BACKTEST_HOLD_N);
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

    // CSR 持仓 (按 d 写一段; 段内权重降序 — 便于 py 直接显示)
    std::vector<std::pair<int, double>> sorted_hold; // (a, weight)
    sorted_hold.reserve(holdings.size());
    for (auto &kv : holdings) {
      double mv = kv.second * static_cast<double>(last_close[
                                  static_cast<std::size_t>(kv.first)]);
      sorted_hold.emplace_back(kv.first, mv / pv_end);
    }
    std::sort(sorted_hold.begin(), sorted_hold.end(),
              [](const auto &x, const auto &y) { return x.second > y.second; });
    for (auto &kv : sorted_hold) {
      hold_codes.push_back(static_cast<std::int32_t>(kv.first));
      hold_weights.push_back(static_cast<float>(kv.second));
      hold_names.emplace_back(name_of(name_timeline, meta, kv.first, d));
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

  // labels.json: 3 个标的名字符串数组, 与对应 npy 同长 (按 trades / hold_codes 顺序).
  //   yyjson 落盘, 与 BigQuant / Tushare store 共享 PRETTY_TWO_SPACES 风格.
  {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    auto add_str_arr = [&](const char *key,
                           const std::vector<std::string> &v) {
      yyjson_mut_val *arr = yyjson_mut_arr(doc);
      for (const std::string &s : v) {
        yyjson_mut_arr_add_strn(doc, arr, s.data(), s.size());
      }
      yyjson_mut_obj_add_val(doc, root, key, arr);
    };
    add_str_arr("trades_open_names", tr_open_names);
    add_str_arr("trades_close_names", tr_close_names);
    add_str_arr("holdings_names", hold_names);

    misc::atomic_write_json(out / "labels.json", doc);
    yyjson_mut_doc_free(doc);
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = t1 - t0;
  return dur.count();
}

} // namespace backtest
