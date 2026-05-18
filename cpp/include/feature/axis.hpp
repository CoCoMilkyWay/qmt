#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace feature {

// ============================================================================
// D 轴 + A 轴 + 反向索引. 由 load_axes() 一次性构造, 此后只读.
//   dates: data/_meta/trading_days.json 中 market_code='CN' 的 date 升序去重
//   codes: data/_meta/cn_stock_basic_info.json 全量 instrument (含已退市) 升序
//   date_days: dates[i] 的 sys_days 缓存 (list_age / rolling 等场合避免重复 parse)
// ============================================================================
struct Axes {
  std::vector<std::string> dates;
  std::vector<std::string> codes;
  std::vector<std::chrono::sys_days> date_days;
  std::unordered_map<std::string, int> date_idx;
  std::unordered_map<std::string, int> code_idx;

  int n_d() const { return static_cast<int>(dates.size()); }
  int n_a() const { return static_cast<int>(codes.size()); }

  // max{i : dates[i] <= d}; 找不到 (d < dates[0]) 返回 -1
  // 用于事件 visible_date → 行 D 的映射 (周末/节假日 visible_date 自动落到上一交易日)
  int floor_date(std::string_view d) const;
};

// ============================================================================
// per-A 静态信息 (asset 维), 与 Axes.codes 同序同长.
//   来源: data/_meta/cn_stock_basic_info.json (BigQuant Static 全量 snapshot)
//
//   name        — instrument 当前简称 (诊断/日志用, 非 PIT — 历史改名走时变 feature)
//   list_date   — YYYYMMDD; 空串 = 未上市 (理论不应出现)
//   delist_date — YYYYMMDD; 空串 = 未退市
//   list_sector — int8 板块编码 (源数据值): 1=主板 / 2=创业板 / 3=科创板 / 4=北交所
//                 0 = 未知 (源数据为 null 或缺失). 内部 ID 即源数据值, 不再做中文映射.
//   exchange    — 中文全称: "上海证券交易所" / "深圳证券交易所" / "北京证券交易所"
//
//   注: industry_l1 不再是 meta 静态 — 由 ts_industry_l1 inter feature 从
//       cn_stock_industry_component (月初快照) + cn_stock_industry_change (日内
//       增量) per-(D, A) 计算, 编码为 SW2021 一级行业 ID (见 feature/industry.hpp).
//   注: name_history 已删除 — risk_warn 简化版直接读 cn_stock_status.st_status,
//       无需再用 namechange 修正 ST 状态边界.
// ============================================================================
struct StockMeta {
  std::vector<std::string> name;
  std::vector<std::string> list_date;
  std::vector<std::string> delist_date;
  std::vector<int8_t> list_sector;
  std::vector<std::string> exchange;
};

Axes load_axes();
StockMeta load_stock_meta(const Axes &);

} // namespace feature
