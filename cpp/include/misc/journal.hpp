#pragma once

#include <string>
#include <utility>
#include <vector>

// ============================================================================
// 通用 (name, yyyymm) 粒度的原子整月事务 — 不绑定具体数据源.
//
// 设计动机:
//   - misc::store 提供单 day file / 单 _empty.json 的 atomic 原语 (tmp+rename).
//   - 当一次写入跨多个 day file + _empty.json 时, 中断会导致部分写, 视图不一致.
//   - 本模块在 store 之上提供 "整月事务" 粒度: 对单 (name, yyyymm) 的整月 day
//     file 写入 + _empty.json 更新视为一个事务. 语义为 "整月替换覆盖":
//     manifest commit 后, 本月 [01, 月末] 内的所有 target day file 与 _empty
//     条目都以 manifest 内容为权威.
//
// 布局 (临时区, 不入正式数据集):
//   data/_journal/<name>__<yyyymm>/
//     <DD>.json        — staged day file 内容 (有数据日)
//     manifest.json    — atomic 写成功即为 "commit point"
//
// 语义:
//   - 调用方在 MonthTxn 中声明本月哪些 DD 有数据 (附完整 JSON bytes), 整月内
//     其余 DD 视为空 (会落入 _empty.json 且 target 上的残留 day file 会被删除).
//   - commit():
//       1. 清空 data/_journal/<name>__<yyyymm>/ (防御性)
//       2. 写所有 staged day file
//       3. atomic_write manifest.json (commit point)
//       4. 立即 apply (rename staged → target + 更新 _empty.json + cleanup)
//   - 中断恢复:
//       a. manifest.json 已写 → 下次 replay_all() 续完
//       b. manifest.json 未写 → 残留 staged dir 视为孤儿, replay_all() 删除即可,
//          调用方下次 commit 会重新 stage (原子语义: 未到 commit point = 未发生)
//
// 进程启动时调用方应先 replay_all() 再做任何 commit, 确保历史中断状态被收尾.
// ============================================================================
namespace misc::journal {

// 一次整月事务的输入 — name+yyyymm 唯一定位. day_files 列出本月所有有数据的
// DD (附完整 JSON bytes); 未列出且 ∈ [01, 该月最后一天] 的 DD 视为空 (会清掉
// 目标位的残留 day file + 入 _empty.json). 跨月数据非法 — 调用方需先按 yyyymm
// 拆分再逐月 commit.
struct MonthTxn {
  std::string name;
  std::string yyyymm; // "YYYYMM"
  // 有数据日的 (DD, full JSON bytes); DD 必须落在 [01, 该月最后一天] 内.
  std::vector<std::pair<std::string, std::string>> day_files;
};

// stage + atomic commit + apply + cleanup. 同步完成; 异常路径仅在网络/磁盘错误
// 触发 (内部 assert). 中断后由 replay_all() 续完已 commit (manifest 已写) 的事务.
void commit(const MonthTxn &txn);

// 启动时调一次: 扫 data/_journal/*/:
//   - 有 manifest.json → apply + cleanup (续完中断的事务)
//   - 无 manifest.json → 删除 (孤儿 staged dir, 视为未提交)
void replay_all();

} // namespace misc::journal
