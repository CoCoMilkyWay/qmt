#pragma once

#include "api/bigquant/spec.hpp"

#include <string_view>
#include <vector>

namespace bigquant {

// ============================================================================
// BigQuant parquet 月数据库导入 — 独立阶段, 与 DAI 完全解耦.
//
// 数据源布局 (与 doc/bigquant/fetch.py 输出一致):
//   <root>/<yyyy>-<mm>/<table>.parquet
//
// 行为:
//   1. misc::journal::replay_all() 收尾上次中断
//   2. 扫描 <root>/ 下所有命名为 "YYYY-MM" 的子目录 (其他名字静默跳过, 含 _meta);
//      按 yyyymm 升序逐月处理.
//   3. 对每个月每张表 (SPECS 中 kind != Static):
//        - parquet 不存在 → 静默 continue
//        - 读 parquet → arrow::Table
//        - 按 spec.visible_date 分桶到整月 [01, 月末] (同次响应 PK upsert)
//        - 通过 misc::journal::commit 提交 "整月替换覆盖" 事务:
//            stage → atomic manifest (commit point) → apply → cleanup
//
// 完整性保证:
//   - 进程中断: manifest 未写 → staged 视为孤儿; manifest 已写 → 下次 replay 续完.
//   - 已有月数据: apply 阶段会清掉 [01, 月末] 内不在 manifest 中的残留 target file,
//     等价于 "先删整月再写整月".
//
// 静态表 (kind == Static): 不走 parquet, 留给 DAI 整刷.
// ============================================================================
void import_parquet(std::string_view database_root,
                    const std::vector<TableSpec> &specs);

} // namespace bigquant
