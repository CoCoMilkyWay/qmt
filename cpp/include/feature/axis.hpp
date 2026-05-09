#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace feature {

// D 轴 + A 轴 + 反向索引. 由 load_axes() 一次性构造, 此后只读.
//   dates: SSE ∪ SZSE 且 is_open=1 的交易日 YYYYMMDD, 升序去重
//   codes: _meta/stock_basic.json 全量 ts_code, 升序 (含已退市)
//   date_days: dates[i] 的 sys_days 缓存 (list_age / rolling 等场合避免重复 parse)
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

// per-A 静态信息 (asset 维), 与 Axes.codes 同序同长.
//   list_date / delist_date: YYYYMMDD; delist_date 空串 = 未退市
//   market: 主板 / 创业板 / 科创板 / ...
//   exchange: SSE / SZSE / BSE
struct StockMeta {
  std::vector<std::string> list_date;
  std::vector<std::string> delist_date;
  std::vector<std::string> market;
  std::vector<std::string> exchange;
};

Axes load_axes();
StockMeta load_stock_meta(const Axes &);

} // namespace feature
