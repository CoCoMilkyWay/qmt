#include "api/bigquant/store.hpp"

#include "api/bigquant/parse.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"
#include "package/yyjson/yyjson.h"

#include <arrow/table.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace bigquant::store {

namespace {

// MonthFirst spec 保证单月仅 1 DD (visible_date = MIN(date)).
//   lookback 强拉时, 服务端 MIN 可能因数据补齐而前移 (5/11 → 5/06), 旧 DD 若不清掉
//   会出现单月双 DD, 违反 spec → ts_industry_l1 回放被旧快照覆盖.
//   策略: docs 写过的月, 同月其余 DD 文件全部 fs::remove (在 write_day_docs 之前).
void cleanup_stale_month_dds(
    const std::string &name,
    const std::map<std::string, yyjson_mut_doc *> &docs) {
  namespace fs = std::filesystem;
  std::set<std::string> wrote_ym_dd; // "YYYYMM_DD"
  std::set<std::string> wrote_yms;   // "YYYYMM"
  for (auto &[vd, _] : docs) {
    wrote_ym_dd.insert(vd.substr(0, 6) + "_" + vd.substr(6, 2));
    wrote_yms.insert(vd.substr(0, 6));
  }
  std::string fname = name + ".json";
  for (auto &ym : wrote_yms) {
    fs::path month_dir =
        misc::git_root() / "data" / ym.substr(0, 4) / ym.substr(4, 2);
    if (!fs::exists(month_dir))
      continue;
    for (auto &entry : fs::directory_iterator(month_dir)) {
      if (!entry.is_directory())
        continue;
      std::string dd = entry.path().filename().string();
      if (wrote_ym_dd.count(ym + "_" + dd))
        continue;
      fs::path target = entry.path() / fname;
      if (fs::exists(target))
        fs::remove(target);
    }
  }
}

} // namespace

// ============================================================================
// write_meta — Static 表 DAI 单段响应直写 _meta/<name>.json
//   (emit_meta 表不走此路径, 见 aggregate_meta)
// ============================================================================

void write_meta(const std::shared_ptr<arrow::Table> &t, const TableSpec &spec) {
  assert(spec.kind == FetchKind::Static);
  assert(!spec.emit_meta && "Static 表自有 _meta 路径, emit_meta 必须为 false");
  assert(t);
  yyjson_mut_doc *doc = table_to_json(t);
  misc::atomic_write_json(misc::store::meta_data_path(spec.name), doc);
  yyjson_mut_doc_free(doc);
}

// ============================================================================
// aggregate_meta — emit_meta 表收尾: day file 全量聚合到 _meta/<name>.json
//   data/<Y>/<M>/<D>/<name>.json 是行式数组 (records); 按日期升序 concat 后整段写 _meta.
//   yyjson 直接拼数组 (yyjson_mut_arr_append 接受 immutable val, 用 mut_val_mut_copy 桥接).
// ============================================================================

void aggregate_meta(const TableSpec &spec) {
  assert(spec.emit_meta);
  assert(spec.kind == FetchKind::Partition && spec.freq == FetchFreq::Day &&
         "emit_meta 当前仅支持 Partition+Day");
  namespace fs = std::filesystem;

  // 扫 data/, 收集 (yyyymmdd, day_file_path); 按日期升序.
  fs::path data_root = misc::git_root() / "data";
  std::string fname = spec.name + ".json";
  std::vector<std::pair<std::string, fs::path>> day_files;
  for (auto &year_entry : fs::directory_iterator(data_root)) {
    if (!year_entry.is_directory()) continue;
    std::string yyyy = year_entry.path().filename().string();
    if (yyyy.size() != 4) continue;
    for (auto &mon_entry : fs::directory_iterator(year_entry.path())) {
      if (!mon_entry.is_directory()) continue;
      std::string mm = mon_entry.path().filename().string();
      if (mm.size() != 2) continue;
      for (auto &day_entry : fs::directory_iterator(mon_entry.path())) {
        if (!day_entry.is_directory()) continue;
        std::string dd = day_entry.path().filename().string();
        if (dd.size() != 2) continue;
        fs::path target = day_entry.path() / fname;
        if (!fs::exists(target)) continue;
        day_files.emplace_back(yyyy + mm + dd, target);
      }
    }
  }
  std::sort(day_files.begin(), day_files.end());

  yyjson_mut_doc *out_doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *out_root = yyjson_mut_arr(out_doc);
  yyjson_mut_doc_set_root(out_doc, out_root);

  for (auto &[ymd, path] : day_files) {
    std::string buf = misc::read_file_all(path);
    assert(!buf.empty() && "aggregate_meta: day file 为空");
    yyjson_doc *in_doc = yyjson_read(buf.data(), buf.size(), 0);
    assert(in_doc);
    yyjson_val *in_root = yyjson_doc_get_root(in_doc);
    assert(yyjson_is_arr(in_root) && "aggregate_meta: day file 顶层必须是数组");
    size_t i, n;
    yyjson_val *item;
    yyjson_arr_foreach(in_root, i, n, item) {
      yyjson_mut_val *copy = yyjson_val_mut_copy(out_doc, item);
      assert(copy);
      yyjson_mut_arr_append(out_root, copy);
    }
    yyjson_doc_free(in_doc);
  }

  misc::atomic_write_json(misc::store::meta_data_path(spec.name), out_doc);
  yyjson_mut_doc_free(out_doc);
}

// ============================================================================
// write_by_visible_date — Partition / Where × Day / MonthFirst 共用
//   桶分逻辑 (含 PK upsert / 范围裁剪) 抽到 parse::bucket_by_visible_date 共享给
//   bigquant::import_parquet, 尾段 (写 day file + _empty) 调 misc::store::write_day_docs.
// ============================================================================

void write_by_visible_date(const std::shared_ptr<arrow::Table> &t,
                           const TableSpec &spec, std::string_view start,
                           std::string_view end) {
  std::map<std::string, yyjson_mut_doc *> docs;
  for (auto &[vd, rows] : bucket_by_visible_date(t, spec, start, end)) {
    docs.emplace(vd, table_subset_to_json(t, rows));
  }

  if (spec.freq == FetchFreq::MonthFirst) {
    cleanup_stale_month_dds(spec.name, docs);
  }

  misc::store::write_day_docs(spec.name, start, end, std::move(docs));
}

} // namespace bigquant::store
