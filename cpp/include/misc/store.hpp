#pragma once

#include "package/yyjson/yyjson.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// ============================================================================
// 通用数据落地存储 — 不绑定具体数据源 (bigquant / tushare 共用)
//
// 仅持久化原语:
//   - 路径生成: day_data_path / meta_data_path / lastupdate_path / empty_month_path
//   - 反向稀疏标记: _empty.json {name: [DD,...]} = 拉过且为空
//   - 单 itf lastupdate 去重: data/_meta/<name>.lastupdate (unix epoch s 文本)
//   - 整段 write: write_day_docs (写所有 day file + _empty.json 维护, 两侧 store 共用尾段)
//
// 调度 (缺失扫描 / 段切分) 见 misc/schedule.hpp.
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
// write_day_docs — bigquant / tushare store 共用尾段
//   - 对 docs 中每个 (vd, doc) 原子写到 data/YYYY/MM/DD/<name>.json
//   - 末端 update_empty_for_range(name, [start, end]) 维护稀疏标记
//   - docs 内每个 yyjson_mut_doc* 写后释放 (本函数接管所有权)
// ============================================================================
void write_day_docs(std::string_view name, std::string_view start,
                    std::string_view end,
                    std::map<std::string, yyjson_mut_doc *> docs);

// ============================================================================
// 单 itf lastupdate 去重 (内容 = unix epoch seconds 文本)
//   调用方语义: should_skip_api 命中 → 跳过整段; 否则跑完后 mark_api_updated.
//   粒度 = 数据文件名 (data/.../<name>.json), 不同 itf 共用 api 但走独立 key.
//
//   verify_exists 可选: 非空时, 该路径必须存在才能 skip; 不存在 → 强制重抓.
//   用途: Static 表的 _meta 单文件输出在 lastupdate 已 mark 但文件丢失时,
//        若仅看 lastupdate 会永久跳过 (Static 无 day file 兜底).
//        emit_meta 表的 _meta 是从 day file 聚合的产物, pipeline 每轮无脑重建,
//        不依赖 verify, 此处不传.
// ============================================================================
bool should_skip_api(std::string_view name, int window_seconds,
                     const std::filesystem::path &verify_exists = {});
void mark_api_updated(std::string_view name);

} // namespace misc::store
