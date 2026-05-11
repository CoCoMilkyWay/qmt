#pragma once

#include <string_view>

namespace tushare {

class Http;

// 全量刷新 stock_basic 全局 meta，覆盖 data/_meta/stock_basic.json。
// 跨 list_status (L/D/P/G) 四次调用合并去重 (PK=ts_code)，按 ts_code 升序输出。
// 与 SPECS / strategy / store 的 per-day 体系完全独立，仅在 update() 入口调一次。
void refresh_stock_basic(Http &http);

// 全量刷新申万行业成分 (is_new=Y)，覆盖 data/_meta/index_member_all.json。
// 单次 index_member_all 上限 2000 行，全市场约 5000 行 → 先调 index_classify
// (level=L1, src=SW2021) 拿 ~31 个 L1 代码，按 L1 分批拉再合并 (PK=ts_code)。
// 与 stock_basic 同套，仅在 update() 入口调一次。
void refresh_index_member_all(Http &http);

// 聚合所有 data/YYYY/MM/DD/namechange.json 为全局 meta，覆盖
// data/_meta/namechange.json。不调 tushare API，纯本地扫描。
// 结构 = nested object {ts_code: [{name, start_date, ann_date, change_reason}, ...]}
//   - 外层 ts_code 升序; 内层数组按 start_date 升序 (历史正序, 旧 → 新)
//   - PK=(ts_code, start_date) 去重: 按 day file YYYY/MM/DD 升序遍历, 后写覆盖
//     (lookback 内 tushare 修正会让较新 ann_date 的记录胜出)
// 在 update() 末尾无条件调用 (本地操作, 不受 namechange spec dedup 影响)。
void refresh_namechange_meta();

// ============================================================================
// 单 itf 去重 (data/_meta/<name>.lastupdate, 内容 = unix epoch seconds)
// 调用方语义: should_skip_api 命中 → 跳过整段; 否则跑完后 mark_api_updated。
// 粒度 = 数据文件名 spec.name (≡ data/.../<name>.json);
// 不能用 spec.api: disclosure 与 report 共用 api=disclosure_date,
// 但是两个独立 itf, 共享 key 会让先跑的把后跑的也吃掉。
// meta 刷新走同一通道, 用 "stock_basic" / "index_member_all"。
// ============================================================================

// 上次成功更新距今 < window_seconds 返回 true；文件不存在返回 false。
bool should_skip_api(std::string_view name, int window_seconds);

// 写当前时间到 data/_meta/<name>.lastupdate (atomic_write)。
void mark_api_updated(std::string_view name);

} // namespace tushare
