#pragma once

#include "backtest/backtest.hpp"
#include "feature/axis.hpp"

#include <span>

// ============================================================================
// 跨策略聚合层 — main 的 per-strategy 循环之后跑一次, 输入是各策略的
//   backtest::Result (不重读 npy).
//
// 窗口 = 各策略回测窗口的**交集** (左端 = max(d_lo), 右端固定 axes 最新日):
//   组合净值 / 相关矩阵 / 持仓重叠度都要求当日全部策略都有数据, 交集才自洽.
//   交集为空 (某策略起始日晚于其他策略的结束日) 直接 assert.
//
// 基准: strategy::BENCHMARK (可 nullptr, 见 strategy/registry.hpp 顶注).
//   非空时额外给出各策略 / 组合相对该策略的 信息比率/Beta/Alpha/跟踪误差.
//   注意这是**聚合视图专属**语义, per-strategy 报告里的同名指标仍相对各自
//   pool 指数, 两者不混.
//
// 输出 <git_root>/output/aggregate/:
//   - dates.npy          [n_d_ag] int32   (axes 全局索引)
//   - strategy_nav.npy   [n_s, n_d_ag] f4 (各策略净值, 归一到窗口首日 = 1.0)
//   - combo_nav.npy      [n_d_ag] f4      (等权组合: 每日 rebalance 到 1/n_s)
//   - overlap_count.npy  [n_d_ag] int32   (被 ≥2 策略同时持有的股票数)
//   - corr.npy           [n_s, n_s] f4    (策略间日收益 Pearson)
//   - report.json        strategies[] 顺序 / benchmark / metrics{} / desk{}
//                          desk = 今日多策略下单台 (末日各策略持仓合并, 按组合
//                          权重降序; 含每策略权重列与命中策略数 ⇒ 重叠股一眼可见)
//
// 返回 elapsed_seconds (进 meta.json).
namespace report {

double aggregate(const feature::Axes &axes,
                 std::span<const backtest::Result> results);

} // namespace report
