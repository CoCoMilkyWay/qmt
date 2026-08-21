#include "backtest/backtest.hpp"

#include "backtest/engine.hpp"
#include "config_main.hpp"
#include "feature/axis.hpp"
#include "feature/def/basic/daily_return.hpp"
#include "feature/def/basic/delist_age.hpp"
#include "feature/feature.hpp"
#include "feature/tensor.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/npy.hpp"
#include "misc/parquet.hpp"
#include "misc/timer.hpp"
#include "package/yyjson/yyjson.h"
#include "report/json.hpp"
#include "report/metrics.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
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

using feature::is_finite;

// ---- helpers ---------------------------------------------------------------

// 策略块契约 bool (pool): 必 finite + ∈ {0, 1}, F 换扁平 slot.
inline bool strat_read_bool(const feature::Tensor &T, int slot, int a, int d) {
  float v = T.strat_at(slot, a, d);
  assert(is_finite(v) && "backtest::strat_read_bool: NaN — column should be 0/1");
  return v > 0.5f;
}

inline int find_d(const feature::Axes &axes, std::string_view yyyymmdd,
                  bool floor) {
  // floor=true: 找 ≤ yyyymmdd 的最大索引; false: 找 ≥ 的最小.
  if (floor)
    return axes.floor_date(yyyymmdd);
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

// yyyymmdd int32 → "YYYYMMDD"; <= 0 → 空串.
inline std::string ymd_str(std::int32_t v) {
  if (v <= 0)
    return {};
  char buf[9];
  std::snprintf(buf, sizeof(buf), "%08d", v);
  return std::string(buf, 8);
}

// 一组"实体 × 指标"的绝对指标 (策略 / pool指数 共用形状);
//   返回新建的子对象, 便于调用方 (策略行) 继续追加相对指标.
yyjson_mut_val *add_nav_stats(yyjson_mut_doc *doc, yyjson_mut_val *parent,
                              const char *key, const report::NavStats &s) {
  yyjson_mut_val *o = report::add_obj(doc, parent, key);
  yyjson_mut_obj_add_int(doc, o, "天数", s.n_days);
  report::add_f4(doc, o, "年化", s.ann_return);
  report::add_f4(doc, o, "波动率", s.ann_vol);
  report::add_f4(doc, o, "夏普", s.sharpe);
  report::add_f4(doc, o, "最大回撤", s.max_drawdown);
  yyjson_mut_obj_add_int(doc, o, "创新高最长天数", s.longest_no_new_high);
  return o;
}

void add_name_interval(const feature::Axes &axes, std::vector<NameInterval> &v,
                       std::string_view start, std::string_view end,
                       std::string name) {
  assert(start.size() == 8 && end.size() == 8 && !name.empty());
  int lo = find_d(axes, start, /*floor=*/false);
  int hi = find_d(axes, end, /*floor=*/true);
  if (lo < 0 || hi < lo)
    return;
  v.push_back(NameInterval{lo, hi, std::move(name)});
}

inline std::string_view name_of(const NameTimeline &tl,
                                const feature::StockMeta &meta, int a, int d) {
  const auto &v = tl.by_a[static_cast<std::size_t>(a)];
  for (auto it = v.rbegin(); it != v.rend(); ++it) {
    if (it->lo <= d && d <= it->hi)
      return it->name;
  }
  return meta.name[static_cast<std::size_t>(a)];
}

} // namespace

NameTimeline load_name_timeline(const feature::Axes &axes) {
  misc::Timer t("[backtest] load_name_timeline");
  int n_a = axes.n_a();
  NameTimeline tl;
  tl.by_a.resize(static_cast<std::size_t>(n_a));
  std::vector<std::string> last_end(static_cast<std::size_t>(n_a));

  // 历史简称区间: cn_stock_name_change 月度 parquet 全扫.
  for (auto &[ym, path] : misc::pq::list_month_files("cn_stock_name_change")) {
    misc::pq::TableView v(misc::pq::read_table(path));
    if (v.rows() == 0)
      continue;
    misc::pq::Col ins = v.col("instrument"), nm = v.col("name");
    misc::pq::Col sd = v.col("start_date"), ed = v.col("end_date");
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      auto it = axes.code_idx.find(std::string(ins.str(i)));
      if (it == axes.code_idx.end())
        continue;
      int a = it->second;
      std::string start = ymd_str(sd.yyyymmdd(i));
      std::string end = ymd_str(ed.yyyymmdd(i));
      if (start.empty() || end.empty())
        continue;
      add_name_interval(axes, tl.by_a[static_cast<std::size_t>(a)], start, end,
                        std::string(nm.str(i)));
      std::string &mx = last_end[static_cast<std::size_t>(a)];
      if (mx.empty() || end > mx)
        mx = std::move(end);
    }
  }

  // 当前简称: 真盘前快照兜住 [last_end+1, 最新日] 尾段.
  auto static_path = misc::pq::meta_path("cn_stock_static_data");
  assert(fs::exists(static_path) &&
         "data/_meta/cn_stock_static_data.parquet missing — 先跑 bigquant::update");
  misc::pq::TableView sv(misc::pq::read_table(static_path));
  misc::pq::Col ins = sv.col("instrument"), nm = sv.col("name");
  for (std::int64_t i = 0, nr = sv.rows(); i < nr; ++i) {
    auto it = axes.code_idx.find(std::string(ins.str(i)));
    if (it == axes.code_idx.end())
      continue;
    int a = it->second;
    const std::string &mx = last_end[static_cast<std::size_t>(a)];
    std::string start = mx.empty() ? axes.dates.front() : misc::add_days(mx, 1);
    add_name_interval(axes, tl.by_a[static_cast<std::size_t>(a)], start,
                      axes.dates.back(), std::string(nm.str(i)));
  }

  for (auto &v : tl.by_a) {
    std::sort(v.begin(), v.end(), [](const NameInterval &x, const NameInterval &y) {
      return x.lo < y.lo;
    });
  }
  return tl;
}

std::vector<std::string> last_names(const feature::Axes &axes,
                                    const feature::StockMeta &meta,
                                    const NameTimeline &tl) {
  int d = axes.n_d() - 1;
  std::vector<std::string> out(static_cast<std::size_t>(axes.n_a()));
  for (int a = 0; a < axes.n_a(); ++a)
    out[static_cast<std::size_t>(a)] = std::string(name_of(tl, meta, a, d));
  return out;
}

Result run(const feature::Axes &axes, const feature::StockMeta &meta,
           const feature::Tensor &T, const NameTimeline &name_timeline,
           const strategy::StrategySpec &spec, int s_idx) {
  // 注: meta.delist_date 是 ex-post (build 时拉的最新表), 但 `delist_age` 已在
  //     feature 层做 PIT 截断 — 仅 D ≥ delist_date 写值 (≥ 0), 否则 NaN. 此处兜底
  //     用 is_finite 判 "今日已退市" 即可, 不需也不应去读"距退市天数".
  misc::Timer t("[backtest] run");
  auto t0 = std::chrono::high_resolution_clock::now();

  using strategy::SF;
  const int slot_pool = strategy::slot(s_idx, SF::pool);
  const int slot_score = strategy::slot(s_idx, SF::score);
  const int slot_rank = strategy::slot(s_idx, SF::rank);

  // ---- 解析回测窗口 (闭区间; 右端点固定为最新日) --------------------------
  int bt_d_lo = find_d(axes, spec.bt_start_date, /*floor=*/false);
  int bt_d_hi_inc = axes.n_d() - 1;
  assert(bt_d_lo >= 0 && bt_d_hi_inc >= bt_d_lo &&
         bt_d_hi_inc < axes.n_d() && "backtest window invalid");
  int bt_d_hi = bt_d_hi_inc + 1; // half-open
  int n_d_bt = bt_d_hi - bt_d_lo;
  int n_a = axes.n_a();

  // ---- 内核 ----------------------------------------------------------------
  //   决策 + 现金/仓位/成本记账全在 Engine 里 (backtest/engine.hpp), 与 mine
  //   共用同一份实现; 本函数只负责 "候选序怎么来" 与 "记录什么".
  MarketWindow mk = load_market(axes, T, bt_d_lo, n_d_bt);
  Engine eng(mk, spec.hold_n, spec.exit_ratio);

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
  hold_codes.reserve(static_cast<std::size_t>(n_d_bt) *
                     static_cast<std::size_t>(spec.hold_n));
  hold_weights.reserve(static_cast<std::size_t>(n_d_bt) *
                       static_cast<std::size_t>(spec.hold_n));
  hold_names.reserve(static_cast<std::size_t>(n_d_bt) *
                     static_cast<std::size_t>(spec.hold_n));

  // CSR 因子观察窗 (top hold_n*2, 段内因子降序 — 报告 tooltip 用).
  //   hold_w / hold_days / bought 三列把 tooltip 需要的持仓状态一次性算完, 前端
  //   hover 只做格式化, 不再从 holdings / fills 反推 (见 backtest.hpp 输出契约).
  int watch_n = spec.hold_n * 2;
  std::vector<std::int32_t> watch_off(static_cast<std::size_t>(n_d_bt) + 1, 0);
  std::vector<std::int32_t> watch_codes;
  std::vector<float> watch_scores;
  std::vector<float> watch_rank_chg;         // 5d rank MA − 当日 rank; + = 排名上升
  std::vector<float> watch_hold_w;           // 当日持仓权重; NaN = 未持仓
  std::vector<std::int32_t> watch_hold_days; // 连续持有天数; 0 = 未持仓
  std::vector<std::int32_t> watch_bought;    // 1 = 当日正式买入
  std::vector<std::string> watch_names;
  std::size_t watch_cap = static_cast<std::size_t>(n_d_bt) *
                          static_cast<std::size_t>(watch_n);
  watch_codes.reserve(watch_cap);
  watch_scores.reserve(watch_cap);
  watch_rank_chg.reserve(watch_cap);
  watch_hold_w.reserve(watch_cap);
  watch_hold_days.reserve(watch_cap);
  watch_bought.reserve(watch_cap);
  watch_names.reserve(watch_cap);
  std::deque<std::vector<int>> rank_win; // 近 5 日 1-based 因子排名, 0=未入选

  // 连续持有天数: streak[a] 在"昨日也持有"时 +1, 否则重置 1 (last_seen 判连续).
  std::vector<int> hold_streak(static_cast<std::size_t>(n_a), 0);
  std::vector<int> hold_last_seen(static_cast<std::size_t>(n_a), -2);
  // 前一日收盘权重 (卖出 fill 的仓位标注取卖出前一日口径); prev_held 记非零项,
  //   每日只清这几个下标, 不必整表 memset.
  std::vector<float> prev_weight(static_cast<std::size_t>(n_a), 0.0f);
  std::vector<int> prev_held;

  // 成交 (open-close 配对)
  struct OpenRec {
    int open_d;
    float open_px;
  };
  std::unordered_map<int, OpenRec> open_recs;

  std::vector<std::int32_t> tr_inst, tr_open_d, tr_close_d;
  std::vector<float> tr_open_px, tr_close_px;
  std::vector<std::string> tr_open_names, tr_close_names; // 开/平仓当日历史简称

  // 正式调仓成交 (因子 pop/补槽 / 退市强平; 不含再平衡加仓)
  std::vector<std::int32_t> fill_d, fill_a, fill_side; // side: +1 买, -1 卖
  std::vector<float> fill_px;
  std::vector<float> fill_weight; // 买: 当日收盘权重 (循环末回填); 卖: 前一日权重
  std::vector<float> fill_pnl;    // 卖: 本笔 trade 收益率 %; 买: NaN
  std::vector<std::string> fill_names;
  // 当日买入 fill 的下标 (权重要等 pv_end 算出来才能回填)
  std::vector<std::size_t> buy_fill_idx;
  std::unordered_set<int> bought_today; // 当日正式买入 (watch_bought 标记用)

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

  // 买: weight 先占位 NaN, 下标记入 buy_fill_idx, 当日 pv_end 算出后回填;
  //     pnl 恒 NaN (开仓无盈亏).
  // 卖: weight 直接取 prev_weight[a] (此刻仍是前一日值), pnl 由本笔 trade 算出.
  auto push_fill = [&](int a, int d, int side, float px, float pnl) {
    fill_d.push_back(static_cast<std::int32_t>(d));
    fill_a.push_back(static_cast<std::int32_t>(a));
    fill_side.push_back(static_cast<std::int32_t>(side));
    fill_px.push_back(px);
    fill_pnl.push_back(pnl);
    if (side > 0) {
      buy_fill_idx.push_back(fill_weight.size());
      fill_weight.push_back(std::nanf(""));
    } else {
      fill_weight.push_back(prev_weight[static_cast<std::size_t>(a)]);
    }
    fill_names.emplace_back(name_of(name_timeline, meta, a, d));
  };

  // 本笔卖出收益率 (比率, 非百分数) — trade 口径 (开仓价 → 平仓价), 与 trades_* 一致.
  //   报告里所有比率一律裸比率落盘, 百分号是前端格式化的事.
  auto trade_ret = [](float open_px, float close_px) {
    return (open_px > 0.0f) ? (close_px / open_px - 1.0f) : std::nanf("");
  };

  // pool benchmark NAV
  double pool_nav_d = ::config::BACKTEST_CAPITAL_BASE;

  // Recorder — 内核的三个成交钩子, 把明细落到上面那堆容器里 (挖掘路径用
  //   NullRecorder, 这三段整体消失). 钩子拿窗口内 i, 全局 D 在此处加回.
  auto rec = make_recorder(
      // 退市强平: 与正常卖出一样关 trade record, 额外打 [WARN] —
      //   触发说明上游 filter 未能在退市前卖出 (可能一直停牌 / 缺 ST 标记).
      [&](int a, int i, float c, double sh) {
        int d = bt_d_lo + i;
        auto rec_it = open_recs.find(a);
        assert(rec_it != open_recs.end() && "delisted holding without open record");
        float open_px = rec_it->second.open_px;
        float da = T.at(feature::def::delist_age_spec, a, d);
        std::printf("[WARN] backtest d=%s 持仓 %s (%s) 已退市 "
                    "(delist_age=%.0f), 持有 %d 交易日, "
                    "开仓 %s@%.4f → 强平 @%.4f, 盈亏 %+.2f%% (%.2f 股)\n",
                    axes.dates[static_cast<std::size_t>(d)].c_str(),
                    axes.codes[static_cast<std::size_t>(a)].c_str(),
                    meta.name[static_cast<std::size_t>(a)].c_str(),
                    da, d - rec_it->second.open_d,
                    axes.dates[static_cast<std::size_t>(rec_it->second.open_d)].c_str(),
                    open_px, c,
                    (open_px > 0.0f) ? (c / open_px - 1.0f) * 100.0f : 0.0f, sh);
        close_trade(a, d, rec_it->second, c);
        push_fill(a, d, -1, c, trade_ret(open_px, c));
        open_recs.erase(rec_it);
      },
      [&](int a, int i, float c) {
        int d = bt_d_lo + i;
        auto it = open_recs.find(a);
        assert(it != open_recs.end() && "sell without open record");
        close_trade(a, d, it->second, c);
        push_fill(a, d, -1, c, trade_ret(it->second.open_px, c));
        open_recs.erase(it);
      },
      [&](int a, int i, float c) {
        int d = bt_d_lo + i;
        open_recs[a] = OpenRec{d, c};
        push_fill(a, d, +1, c, std::nanf(""));
        bought_today.insert(a);
      });

  // ---- 主循环 --------------------------------------------------------------
  for (int i = 0; i < n_d_bt; ++i) {
    int d = bt_d_lo + i;
    dates_out[i] = d;
    buy_fill_idx.clear();
    bought_today.clear(); // 当日正式买入 (watch_bought 标记用)
    const float *close = mk.close_row(i);
    const std::uint8_t *flag = mk.flag_row(i);

    // (1) 候选 universe: 策略 rank 列直读 (rank ≥ 1 ⇔ pool ∧ finite(score);
    //   排名已在 columns.cpp 固化, 回测与实盘选股读同一列).
    //   cands[r-1] = (score, a): rank 是 1..K 连续整数, 两遍扫描按位回填.
    std::vector<std::pair<float, int>> cands;
    int n_ranked = 0;
    for (int a = 0; a < n_a; ++a) {
      float r = T.strat_at(slot_rank, a, d);
      assert(is_finite(r) && r >= 0.0f && "rank column: NaN/negative");
      if (r > 0.0f)
        ++n_ranked;
    }
    cands.assign(static_cast<std::size_t>(n_ranked),
                 {std::nanf(""), -1});
    for (int a = 0; a < n_a; ++a) {
      float r = T.strat_at(slot_rank, a, d);
      if (!(r > 0.0f))
        continue;
      int ri = static_cast<int>(r);
      assert(ri >= 1 && ri <= n_ranked && "rank column: not contiguous 1..K");
      cands[static_cast<std::size_t>(ri - 1)] = {T.strat_at(slot_score, a, d),
                                                 a};
    }

    // (2) 内核走一天: 退市强平 → mark-to-market → 卖出/买入意向 → 执行 →
    //   再平衡现金清扫 → 当日终值. 全部账目在 Engine 内, 与 mine 同一份代码.
    eng.step(i, std::span<const std::pair<float, int>>(cands), rec);
    strat_nav[i] = static_cast<float>(eng.pv_end);

    // pool 等权影子指数 (策略可买母集, filters 已前置到 pool).
    //   时点: D-1 close 按 mask[a, d-1] 等权 → 持有到 D close, 得 daily_return[a, d].
    //   PIT: mask[d-1]; daily_return[d] NaN ⇒ 该持仓退出当日均值.
    //   i=0: NAV = capital_base; i>=1: 累乘.
    if (i > 0) {
      auto ew = [&](int mask_slot) {
        double dr_sum = 0.0;
        int dr_n = 0;
        int d_prev = d - 1;
        for (int a = 0; a < n_a; ++a) {
          if (!strat_read_bool(T, mask_slot, a, d_prev))
            continue;
          float r = T.at(feature::def::daily_return_spec, a, d);
          if (!is_finite(r))
            continue;
          dr_sum += static_cast<double>(r);
          ++dr_n;
        }
        return dr_n > 0 ? dr_sum / static_cast<double>(dr_n) : 0.0;
      };
      pool_nav_d *= (1.0 + ew(slot_pool));
    }
    pool_nav[i] = static_cast<float>(pool_nav_d);

    // 持仓数 / 仓位 / 换手 / 停牌占比 / 可执行率
    const std::vector<Pos> &holdings = eng.holdings;
    pos_count[i] = static_cast<std::int32_t>(holdings.size());
    pos_pct[i] = static_cast<float>(eng.mv_end / eng.pv_end);
    // 双边换手: 买卖额 / 2 / 当日决策时点组合市值; 满额换 1 个成分股 = 1/HOLD_N
    turnover[i] = static_cast<float>(eng.turn_amt / (2.0 * eng.pv));
    int n_susp_h = 0;
    for (const Pos &p : holdings) {
      if ((flag[p.a] & MarketWindow::SUSP) != 0)
        ++n_susp_h;
    }
    susp_pct[i] = holdings.empty()
                      ? 0.0f
                      : static_cast<float>(n_susp_h) /
                            static_cast<float>(holdings.size());
    int intent = eng.n_sell_intent + eng.n_buy_intent;
    exec_pct[i] = (intent == 0)
                      ? 1.0f
                      : static_cast<float>(eng.n_sell_ok + eng.n_buy_ok) /
                            static_cast<float>(intent);

    // CSR 持仓 (按 d 写一段; 段内权重降序 — 便于 py 直接显示)
    std::vector<std::pair<int, double>> sorted_hold; // (a, weight)
    sorted_hold.reserve(holdings.size());
    for (const Pos &p : holdings) {
      double mv = p.shares * static_cast<double>(close[p.a]);
      sorted_hold.emplace_back(p.a, mv / eng.pv_end);
    }
    std::sort(sorted_hold.begin(), sorted_hold.end(),
              [](const auto &x, const auto &y) { return x.second > y.second; });
    for (auto &kv : sorted_hold) {
      hold_codes.push_back(static_cast<std::int32_t>(kv.first));
      hold_weights.push_back(static_cast<float>(kv.second));
      hold_names.emplace_back(name_of(name_timeline, meta, kv.first, d));
    }
    hold_off[i + 1] = static_cast<std::int32_t>(hold_codes.size());

    // 当日权重表 + 连续持有天数 (watch 三列 / 买入 fill 权重回填的共同来源).
    std::unordered_map<int, float> today_w;
    today_w.reserve(sorted_hold.size() * 2);
    for (auto &kv : sorted_hold) {
      int a = kv.first;
      today_w[a] = static_cast<float>(kv.second);
      std::size_t ua = static_cast<std::size_t>(a);
      hold_streak[ua] = (hold_last_seen[ua] == i - 1) ? hold_streak[ua] + 1 : 1;
      hold_last_seen[ua] = i;
    }

    // 买入 fill 的权重 = 成交当日收盘权重 (卖出侧在 push_fill 时已按前一日填好).
    for (std::size_t k : buy_fill_idx) {
      int a = static_cast<int>(fill_a[k]);
      auto it = today_w.find(a);
      fill_weight[k] = (it != today_w.end()) ? it->second : 0.0f;
    }

    // prev_weight 滚动到今日 (先清上一日的非零项, 再写今日).
    for (int a : prev_held)
      prev_weight[static_cast<std::size_t>(a)] = 0.0f;
    prev_held.clear();
    prev_held.reserve(sorted_hold.size());
    for (auto &kv : sorted_hold) {
      prev_weight[static_cast<std::size_t>(kv.first)] =
          static_cast<float>(kv.second);
      prev_held.push_back(kv.first);
    }

    int n_watch = std::min(watch_n, static_cast<int>(cands.size()));
    std::vector<int> rank_today(static_cast<std::size_t>(n_a), 0);
    for (int k = 0; k < static_cast<int>(cands.size()); ++k)
      rank_today[static_cast<std::size_t>(cands[static_cast<std::size_t>(k)].second)] =
          k + 1;
    rank_win.push_back(rank_today);
    if (rank_win.size() > 5)
      rank_win.pop_front();
    for (int k = 0; k < n_watch; ++k) {
      int a = cands[static_cast<std::size_t>(k)].second;
      watch_codes.push_back(static_cast<std::int32_t>(a));
      watch_scores.push_back(cands[static_cast<std::size_t>(k)].first);
      int r = rank_today[static_cast<std::size_t>(a)];
      double sum = 0.0;
      int cnt = 0;
      for (const auto &rd : rank_win) {
        int rv = rd[static_cast<std::size_t>(a)];
        if (rv > 0) {
          sum += rv;
          ++cnt;
        }
      }
      float chg = (cnt > 0 && r > 0)
                      ? static_cast<float>(sum / static_cast<double>(cnt) - r)
                      : 0.0f;
      watch_rank_chg.push_back(chg);
      auto wit = today_w.find(a);
      bool held = wit != today_w.end();
      watch_hold_w.push_back(held ? wit->second : std::nanf(""));
      watch_hold_days.push_back(
          held ? static_cast<std::int32_t>(hold_streak[static_cast<std::size_t>(a)])
               : 0);
      watch_bought.push_back(bought_today.count(a) ? 1 : 0);
      watch_names.emplace_back(name_of(name_timeline, meta, a, d));
    }
    watch_off[i + 1] = static_cast<std::int32_t>(watch_codes.size());
  }

  // ---- 派生序列 --------------------------------------------------------------
  std::vector<float> ret_s = report::daily_returns(strat_nav);
  std::vector<float> ret_p = report::daily_returns(pool_nav);
  std::vector<float> dd_s = report::drawdown_curve(strat_nav);
  std::vector<float> dd_p = report::drawdown_curve(pool_nav);

  // 回测窗口的日期字符串 (年月分组 / 周月赢率用)
  std::vector<std::string> bt_dates;
  bt_dates.reserve(static_cast<std::size_t>(n_d_bt));
  for (int i = 0; i < n_d_bt; ++i)
    bt_dates.push_back(axes.dates[static_cast<std::size_t>(bt_d_lo + i)]);

  // ---- 写盘 ----------------------------------------------------------------
  fs::path out =
      misc::git_root() / "output" / "strategy" / std::string(spec.name) / "backtest";
  fs::create_directories(out);

  wi(out / "dates.npy", dates_out);
  wf(out / "strategy_nav.npy", strat_nav);
  wf(out / "strategy_dd.npy", dd_s);
  // pool_nav/pool_dd 不再落盘 (纯展示用途已删,
  //   计算本身保留供下方 rel_stats / "超额" 行使用).
  wi(out / "position_count.npy", pos_count);
  wf(out / "position_pct.npy", pos_pct);
  wf(out / "turnover.npy", turnover);
  wf(out / "susp_pct.npy", susp_pct);
  wf(out / "executable_pct.npy", exec_pct);

  wi(out / "holdings_offsets.npy", hold_off);
  wi(out / "holdings_codes.npy", hold_codes);
  wf(out / "holdings_weights.npy", hold_weights);

  wi(out / "watch_offsets.npy", watch_off);
  wi(out / "watch_codes.npy", watch_codes);
  wf(out / "watch_scores.npy", watch_scores);
  wf(out / "watch_rank_chg.npy", watch_rank_chg);
  wf(out / "watch_hold_w.npy", watch_hold_w);
  wi(out / "watch_hold_days.npy", watch_hold_days);
  wi(out / "watch_bought.npy", watch_bought);

  wi(out / "trades_inst.npy", tr_inst);
  wi(out / "trades_open_d.npy", tr_open_d);
  wi(out / "trades_close_d.npy", tr_close_d);
  wf(out / "trades_open_px.npy", tr_open_px);
  wf(out / "trades_close_px.npy", tr_close_px);

  wi(out / "fills_d.npy", fill_d);
  wi(out / "fills_a.npy", fill_a);
  wi(out / "fills_side.npy", fill_side);
  wf(out / "fills_px.npy", fill_px);
  wf(out / "fills_weight.npy", fill_weight);
  wf(out / "fills_pnl.npy", fill_pnl);

  // labels.json: 标的名字符串数组, 与对应 npy 同长 (按 trades / hold_codes / watch_codes 顺序).
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
    add_str_arr("watch_names", watch_names);
    add_str_arr("fills_names", fill_names);

    misc::atomic_write_json(out / "labels.json", doc);
    yyjson_mut_doc_free(doc);
  }

  // report.json — 指标与表格全部在此算完, py 侧零计算 (只格式化 + 组 figure).
  //   indicators: 2 行 (策略 / 超额) × 指标
  //     超额 = 策略 − pool指数 的差值 (年化/波动率/夏普/最大回撤), 相对类指标
  //     (信息比率/Alpha/跟踪误差) 直接取策略 vs pool 的 rel_stats.
  //   trade_stats: 16 项交易统计 (CPU时长 / Tensor内存 是运行诊断, 在 meta.json)
  //   annual / monthly: 列式表 (期次 × 8 指标), 基准列 = pool指数
  //   holdings: 末日持仓表 (a / 当日简称 / 开仓日 / 开仓价 / 最近收盘 / 权重 /
  //     累计收益率); code 与行业由 py 用 a 去 meta.json 的 codes/industries 查
  {
    report::NavStats st_s = report::nav_stats(strat_nav);
    report::NavStats st_p = report::nav_stats(pool_nav);
    report::RelStats rel = report::rel_stats(ret_s, ret_p);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *ind = report::add_obj(doc, root, "indicators");
    {
      // 策略行额外带相对本策略 pool 指数的 4 个指标 (聚合层的跨策略指标表
      // 复用同一份 pool_nav, 见 report/aggregate.hpp).
      yyjson_mut_val *o = add_nav_stats(doc, ind, "策略", st_s);
      report::add_f4(doc, o, "信息比率", rel.info_ratio);
      report::add_f4(doc, o, "Beta", rel.beta);
      report::add_f4(doc, o, "Alpha", rel.alpha);
      report::add_f4(doc, o, "跟踪误差", rel.tracking_error);
    }
    // pool指数不再单独展示行 (仅供下方"超额"差值与 rel_stats 用).
    {
      yyjson_mut_val *o = report::add_obj(doc, ind, "超额");
      report::add_f4(doc, o, "年化", st_s.ann_return - st_p.ann_return);
      report::add_f4(doc, o, "波动率", st_s.ann_vol - st_p.ann_vol);
      report::add_f4(doc, o, "夏普", st_s.sharpe - st_p.sharpe);
      report::add_f4(doc, o, "最大回撤", st_s.max_drawdown - st_p.max_drawdown);
      report::add_f4(doc, o, "信息比率", rel.info_ratio);
      report::add_f4(doc, o, "Alpha", rel.alpha);
      report::add_f4(doc, o, "跟踪误差", rel.tracking_error);
    }

    // 交易收益率 (每笔 trade), 供均值 / 赢率 / 正负分组
    std::vector<float> tr_ret(tr_inst.size());
    for (std::size_t k = 0; k < tr_inst.size(); ++k)
      tr_ret[k] = trade_ret(tr_open_px[k], tr_close_px[k]);
    std::vector<float> tr_pos, tr_neg;
    int n_win = 0;
    for (float r : tr_ret) {
      if (!is_finite(r))
        continue;
      if (r > 0.0f) {
        tr_pos.push_back(r);
        ++n_win;
      } else if (r < 0.0f) {
        tr_neg.push_back(r);
      }
    }
    float mean_turn = report::nan_mean(turnover);

    yyjson_mut_val *ts = report::add_obj(doc, root, "trade_stats");
    report::add_f4(doc, ts, "年换手率", mean_turn * report::TRADING_DAYS);
    report::add_f4(doc, ts, "平均持有天数",
                   mean_turn > 0.0f ? 1.0f / mean_turn : std::nanf(""));
    {
      // position_count 是 int32 列, 均值走 float 视图
      std::vector<float> pc(pos_count.begin(), pos_count.end());
      report::add_f4(doc, ts, "平均持仓股票数", report::nan_mean(pc));
    }
    report::add_f4(doc, ts, "平均交易收益", report::nan_mean(tr_ret));
    report::add_f4(doc, ts, "正收益平均", report::nan_mean(tr_pos));
    report::add_f4(doc, ts, "负收益平均", report::nan_mean(tr_neg));
    report::add_f4(doc, ts, "交易赢率",
                   tr_ret.empty() ? std::nanf("")
                                  : static_cast<float>(n_win) /
                                        static_cast<float>(tr_ret.size()));
    report::add_f4(doc, ts, "换股次数",
                   report::nan_sum(turnover) *
                       static_cast<float>(spec.hold_n));
    report::add_f4(doc, ts, "持仓停牌股票比例", report::nan_mean(susp_pct));
    report::add_f4(doc, ts, "月赢率", report::win_rate(bt_dates, ret_s, 2));
    report::add_f4(doc, ts, "周赢率", report::win_rate(bt_dates, ret_s, 1));
    report::add_f4(doc, ts, "日赢率", report::win_rate(bt_dates, ret_s, 0));
    report::add_f4(doc, ts, "调仓指令可执行比例", report::nan_mean(exec_pct));
    report::add_f4(doc, ts, "指数跟踪误差", rel.tracking_error);
    report::add_f4(doc, ts, "平均持仓仓位", report::nan_mean(pos_pct));
    yyjson_mut_obj_add_int(doc, ts, "创新高最长天数", st_s.longest_no_new_high);

    // 年 / 月表 — 列式落盘 (前端零转置)
    auto add_period_table = [&](const char *key, bool by_year) {
      std::vector<report::PeriodStats> rows =
          report::period_stats(bt_dates, ret_s, ret_p, by_year);
      std::vector<std::string> period;
      std::vector<float> c_sr, c_br, c_sdd, c_bdd, c_te, c_ir, c_vol, c_sh;
      for (const report::PeriodStats &p : rows) {
        period.push_back(p.period);
        c_sr.push_back(p.strat_return);
        c_br.push_back(p.bench_return);
        c_sdd.push_back(p.strat_max_dd);
        c_bdd.push_back(p.bench_max_dd);
        c_te.push_back(p.tracking_error);
        c_ir.push_back(p.info_ratio);
        c_vol.push_back(p.ann_vol);
        c_sh.push_back(p.sharpe);
      }
      yyjson_mut_val *o = report::add_obj(doc, root, key);
      report::add_str_arr(doc, o, "期次", period);
      report::add_f4_arr(doc, o, "策略收益", c_sr);
      report::add_f4_arr(doc, o, "基准收益", c_br);
      report::add_f4_arr(doc, o, "策略最大回撤", c_sdd);
      report::add_f4_arr(doc, o, "基准最大回撤", c_bdd);
      report::add_f4_arr(doc, o, "跟踪误差", c_te);
      report::add_f4_arr(doc, o, "信息比率", c_ir);
      report::add_f4_arr(doc, o, "波动率", c_vol);
      report::add_f4_arr(doc, o, "夏普比率", c_sh);
    };
    add_period_table("annual", /*by_year=*/true);
    add_period_table("monthly", /*by_year=*/false);

    // 末日持仓表 — 段内已按权重降序 (主循环写 CSR 时排好)
    {
      std::size_t lo = static_cast<std::size_t>(hold_off[static_cast<std::size_t>(n_d_bt) - 1]);
      std::size_t hi = static_cast<std::size_t>(hold_off[static_cast<std::size_t>(n_d_bt)]);
      // 末日简称走 meta.json::names (全局 per-a, 同一口径), 此处只要价格.
      const float *close_last = mk.close_row(n_d_bt - 1);
      std::vector<std::int32_t> h_a, h_open_d;
      std::vector<float> h_open_px, h_last_px, h_w, h_ret;
      for (std::size_t k = lo; k < hi; ++k) {
        int a = static_cast<int>(hold_codes[k]);
        auto it = open_recs.find(a);
        assert(it != open_recs.end() && "末日持仓无开仓记录");
        float last_px = close_last[a];
        assert(is_finite(last_px) && "末日持仓无收盘价");
        h_a.push_back(static_cast<std::int32_t>(a));
        h_open_d.push_back(static_cast<std::int32_t>(it->second.open_d));
        h_open_px.push_back(it->second.open_px);
        h_last_px.push_back(last_px);
        h_w.push_back(hold_weights[k]);
        h_ret.push_back(trade_ret(it->second.open_px, last_px));
      }
      yyjson_mut_val *o = report::add_obj(doc, root, "holdings");
      report::add_i4_arr(doc, o, "a", h_a);
      report::add_i4_arr(doc, o, "open_d", h_open_d);
      report::add_f4_arr(doc, o, "open_px", h_open_px);
      report::add_f4_arr(doc, o, "last_px", h_last_px);
      report::add_f4_arr(doc, o, "weight", h_w);
      report::add_f4_arr(doc, o, "ret", h_ret);
    }

    misc::atomic_write_json(out / "report.json", doc);
    yyjson_mut_doc_free(doc);
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = t1 - t0;

  Result res;
  res.elapsed_seconds = dur.count();
  res.d_lo = bt_d_lo;
  res.strategy_nav = std::move(strat_nav);
  res.pool_nav = std::move(pool_nav);
  res.hold_off = std::move(hold_off);
  res.hold_codes = std::move(hold_codes);
  res.hold_weights = std::move(hold_weights);
  return res;
}

} // namespace backtest
