#pragma once

#include "feature/axis.hpp"

#include <string>
#include <vector>

// ============================================================================
// 报告用 per-a 展示标签 — 只为表格可读性存在, **不参与任何计算**.
//
// 与 PIT 的关系: 这里取的是最新一份快照, 历史变更不回溯. 真正参与选股计算的行业
//   归属是 feature/def/basic/industry_l1 那个时变节点 (月初成分 + 日频变动事件流,
//   PIT clean); 本文件只负责"给持仓 / 交易记录那一列填个人看得懂的名字".
//
// 落地方式: 结果写进 output/meta.json 的 industries[] (与 codes[] 同序同长),
//   py 侧按 a 索引直查 ⇒ 前端不再自己读 parquet (原 report.py 的
//   _sw_industry_map 已删).
// ============================================================================
namespace report {

// per-a 行业标签 "SW2021一级 -- 二级"; 未覆盖的 a → "未知".
//   源: data/YYYY-MM/cn_stock_industry_component.parquet 最新一个有行的月份,
//   WHERE industry = 'sw2021'.
std::vector<std::string> load_industry_labels(const feature::Axes &axes);

} // namespace report
