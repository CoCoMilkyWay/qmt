#pragma once

namespace tushare {

class Http;

// 全量刷新 stock_basic 全局 meta，覆盖 data/_meta/stock_basic.json。
// 跨 list_status (L/D/P/G) 四次调用合并去重 (PK=ts_code)，按 ts_code 升序输出。
// 与 SPECS / strategy / store 的 per-day 体系完全独立，仅在 update() 入口调一次。
void refresh_stock_basic(Http &http);

} // namespace tushare
