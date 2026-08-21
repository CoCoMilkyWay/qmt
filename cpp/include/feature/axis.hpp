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
//   dates: data/YYYY-MM/all_trading_days.parquet 中 market_code='CN' 的 date
//          升序去重, 截到 today (全年提前排程含未来日; last_d ≜ 实盘当日)
//   codes: data/_meta/cn_stock_basic_info.parquet 中 instrument 升序去重 — 已过滤
//          delist_date < PIPELINE_START_DATE 的标的 (axis 内永远 NaN, 纯冗员).
//          含 window 内已退市标的 (它们在 delist 之前的 row 是 finite, delist 之后
//          被 grid_ffill 延续 last close — 但下游 pool 用 ¬is_finite(delist_age)
//          兜底排除, 无害).
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
//   来源: data/_meta/cn_stock_basic_info.parquet (BigQuant Static 全量 snapshot)
//
//   name        — instrument 当前简称 (诊断/日志用, 非 PIT — 历史改名走时变 feature)
//   list_date   — YYYYMMDD; 空串 = 未上市 (理论不应出现)
//   delist_date — YYYYMMDD; 空串 = 未退市
//   list_sector — 板块中文名 (与 exchange 同口径, 全程汉语, 源 int8 在此映射):
//                 "主板" / "创业板" / "科创板" / "北交所"; "未知" = 源 null/缺失/越界.
//   exchange    — 中文全称: "上海证券交易所" / "深圳证券交易所" / "北京证券交易所"
//
//   注: industry_l1 不再是 meta 静态 — 由 ts_industry_l1 inter feature 从
//       cn_stock_industry_component (月初快照) + cn_stock_industry_change (日内
//       增量) per-(D, A) 计算, 编码为 SW2021 一级行业 ID (见 feature/industry.hpp).
//   注: risk_warn 直接读 cn_stock_status.st_status, 不用 namechange 修正 ST
//       状态边界 (历史简称仅 backtest 报表展示用, 见 backtest NameTimeline).
// ============================================================================
struct StockMeta {
  std::vector<std::string> name;
  std::vector<std::string> list_date;
  std::vector<std::string> delist_date;
  std::vector<std::string> list_sector;
  std::vector<std::string> exchange;
};

// list_sector 源 int8 → 中文名 (axis.cpp 读取时调用; 内部计算判主板用 MAIN_BOARD).
//   源值 0 = 未知 (null/缺失), 1=主板, 2=创业板, 3=科创板, 4=北交所.
inline constexpr std::string_view LIST_SECTOR_NAMES[5] = {
    "未知", "主板", "创业板", "科创板", "北交所"};
inline constexpr std::string_view MAIN_BOARD = "主板";
inline std::string_view list_sector_name(std::int8_t v) {
  unsigned u = static_cast<unsigned>(static_cast<unsigned char>(v));
  return u < 5 ? LIST_SECTOR_NAMES[u] : LIST_SECTOR_NAMES[0];
}

Axes load_axes();
StockMeta load_stock_meta(const Axes &);

} // namespace feature
