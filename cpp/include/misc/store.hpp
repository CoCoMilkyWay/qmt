#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// 通用数据落地存储 — 不绑定具体数据源 (bigquant / tushare 共用)
//
// 设计原则:
//   - 按 visible_date 切日: data/YYYY/MM/DD/<name>.json
//   - 单文件全量 (axis / static): data/_meta/<name>.json
//   - 反向稀疏标记: data/YYYY/MM/_empty.json {name: [DD,...]} = 拉过且为空
//   - 单 itf 去重: data/_meta/<name>.lastupdate (unix epoch s 文本)
//
// 三态判定 (单源、互斥):
//   day file 存在            → 拉过有数据
//   day file 不存在 + 在 set → 拉过无数据
//   day file 不存在 + 不在 set → 未拉
// ============================================================================
namespace misc::store {

// data/YYYY/MM/DD/<name>.json
std::filesystem::path day_data_path(std::string_view yyyymmdd,
                                    std::string_view name);

// data/_meta/<name>.json (axis / static 表用, 不走 per-day)
std::filesystem::path meta_data_path(std::string_view name);

// data/_meta/<name>.lastupdate
std::filesystem::path lastupdate_path(std::string_view name);

// data/YYYY/MM/_empty.json
std::filesystem::path empty_month_path(std::string_view yyyy,
                                       std::string_view mm);

// ============================================================================
// _empty.json 读写 (每月一份, {name: [DD,...]} JSON)
// ============================================================================
using EmptySet = std::unordered_set<std::string>;             // {DD}
using EmptyMonth = std::unordered_map<std::string, EmptySet>; // {itf: {DD}}

// 月内空标记加载; 文件不存在返回空 EmptyMonth.
EmptyMonth read_empty_month(std::string_view yyyy, std::string_view mm);

// 月内空标记写回 (atomic_write); 自动按 itf / DD 排序输出, 跳过空 set.
void write_empty_month(std::string_view yyyy, std::string_view mm,
                       const EmptyMonth &data);

// 在 [start, end] 范围内更新 name 的 _empty.json (按月聚合后单次刷盘).
//   - has_data(d) → set 中移除 dd
//   - 否则 → 检查 day_data_path 是否存在; 不存在则加入 set, 存在则移除
//   - 跨月自动按月分桶, 末端一次性 write_empty_month
// 调用方在写完所有 day file 后调一次即可.
void update_empty_for_range(std::string_view name, std::string_view start,
                            std::string_view end,
                            const std::function<bool(const std::string &)> &has_data);

// ============================================================================
// 单 itf lastupdate 去重 (内容 = unix epoch seconds 文本)
//   调用方语义: should_skip_api 命中 → 跳过整段; 否则跑完后 mark_api_updated.
//   粒度 = 数据文件名 (data/.../<name>.json), 不同 itf 共用 api 但走独立 key.
// ============================================================================
bool should_skip_api(std::string_view name, int window_seconds);
void mark_api_updated(std::string_view name);

// ============================================================================
// 缺失日扫描 — Day 频率
//   - 文件不存在 ∧ 不在 _empty → 必拉
//   - 进入 lookback 窗口 (最近 N 个日历日) → 必拉 (PK upsert / 整段覆盖吃增量/订正)
// 日历 7 天约等于 5 个交易日, 避免当日数据未结算导致累积缺失.
// ============================================================================
std::vector<std::string> scan_missing_days(std::string_view name,
                                           std::string_view start,
                                           std::string_view end,
                                           int lookback_days);

// ============================================================================
// 缺失月扫描 — MonthFirst 频率
//   - 该月任一 day file 存在 → 整月跳过
//   - 进入 lookback 窗口的月 → 必拉
//   - 否则 → 加入 segments, 每段 = [该月首日, 该月末日] (闭区间)
// ============================================================================
struct MonthSeg {
  std::string start; // YYYYMMDD, 该月首日 (clamp 到 outer [start, end])
  std::string end;   // YYYYMMDD, 该月末日 (clamp 到 outer [start, end])
};
std::vector<MonthSeg> scan_missing_months(std::string_view name,
                                          std::string_view start,
                                          std::string_view end,
                                          int lookback_days);

} // namespace misc::store
