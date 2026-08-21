#pragma once

#include "feature/def/factor/bp_ttm3.hpp"
#include "feature/def/factor/cffoa_ttm12.hpp"
#include "feature/def/factor/close.hpp"
#include "feature/def/factor/cp_ttm12.hpp"
#include "feature/def/factor/dy_ttm12.hpp"
#include "feature/def/factor/ep_ttm12.hpp"
#include "feature/def/factor/fmcap.hpp"
#include "feature/def/factor/mcap.hpp"
#include "feature/def/factor/mr_bal.hpp"
#include "feature/def/factor/ms_bal.hpp"
#include "feature/def/factor/roa_ttm12.hpp"
#include "feature/def/factor/roe_ttm12.hpp"
#include "feature/def/factor/sp_ttm12.hpp"
#include "feature/graph.hpp"

#include <string_view>

// ============================================================================
// 因子权重挖掘配置 — 唯一挂载点.
//
// 挖掘只搜"权重"这一个自由度: 目标策略的 pool 白名单 / filters / rank_key /
//   universe_size / bt_start_date / hold_n / exit_ratio 全部原样继承, 回测走
//   backtest/engine.hpp 的同一个内核 ⇒ 挖出来的权重填回
//   strategy/def/<name>.hpp 的 weights, cpp 侧跑出的 NAV 与挖掘期一致.
//
// 唯独 weights 不继承 — 权重就是被搜索的东西. 候选因子由 MINE_FACTORS 定义,
//   与目标策略当前 weights 里有哪些因子完全无关 (可以更多, 也可以毫不相干).
// ============================================================================
namespace mine {

// lattice 扫描总开关 (main.cpp Phase 4.5). 注意与 MINE_FACTORS 无关 —
//   MINE_FACTORS 无条件进计算图 (见 feature/registry.hpp), 关掉挖掘这些因子
//   照样计算/落张量.
inline constexpr bool MINE_ENABLE = false;

// 目标策略名, 必须命中 strategy::STRATEGIES[] 里的某个 name (mine.cpp 启动期
//   assert). 改这里即切策略, 不需要动 cpp 其他任何地方.
inline constexpr std::string_view MINE_STRATEGY = "低价小市值";

// 搜索因子 bucket. 全部必须 Kind::Factor (mine.cpp consteval 校验);
//   顺序即权重向量的维度顺序 (落盘的 k_grid 列序与此一致).
inline constexpr const feature::FeatureSpec *MINE_FACTORS[] = {
    &feature::def::mcap_spec,
    &feature::def::fmcap_spec,
    &feature::def::close_spec,
    &feature::def::cp_ttm12_spec,
    &feature::def::sp_ttm12_spec,
    &feature::def::cffoa_ttm12_spec,
    &feature::def::bp_ttm3_spec,
    &feature::def::ep_ttm12_spec,
    &feature::def::roe_ttm12_spec,
    &feature::def::roa_ttm12_spec,
    &feature::def::dy_ttm12_spec,
    &feature::def::mr_bal_spec,
    &feature::def::ms_bal_spec,
};

inline constexpr int MINE_N_FACTORS =
    static_cast<int>(sizeof(MINE_FACTORS) / sizeof(MINE_FACTORS[0]));

// 带符号 simplex lattice 阶数: w_i = k_i / M, k_i ∈ Z, Σ|k_i| = M.
//   精度 = 1/M. 负权重 = 因子反转 (方向由搜索决定, feature 层不预设方向);
//   w 与 c·w (c>0) 选出的持仓完全相同 ⇒ 有意义的搜索空间就是这个 L1 球面.
//   点数 = Σ_j C(n,j)·C(M-1,j-1)·2^j, n = MINE_N_FACTORS:
//     n=13: M=6 → 4.5e5 | M=8 → 6.1e6 | M=10 → 5.4e7
//   单点开销 ≈ n_d_bt × (池内点积 + top-K 选择 + 内核走一天).
inline constexpr int MINE_LATTICE_M = 6;

// 进"持仓去重"的候选数 (按总分降序). 这是**算力预算**而非统计阈值: 去重是
//   流式贪心, 单候选要现算逐日持仓 (≈5ms), 4096 个约 20s; 最终留下几个风格由
//   重合度自然决定, 不设上限.
inline constexpr int MINE_DEDUP_CAND = 16384;

// 去重线: 平均逐日持仓重合度 ≥ 此值 ⇒ 同一风格. 0.2 = "平均两成持仓相同即视为
//   同款", 比原来的 0.5 更激进 (留下更多风格). 零假设线不可用: 400 池独立选 10
//   只的期望重合 ≈ 0.25 只 (0.025), 所有真实候选都远高于它, 按零假设筛会一个
//   不留. 注意: 改此值会让结果不可跨次比较.
inline constexpr double MINE_DEDUP_OVERLAP = 0.1;

} // namespace mine
