#include "api/bigquant/import.hpp"

#include "api/bigquant/parse.hpp"
#include "api/bigquant/spec.hpp"
#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/journal.hpp"
#include "package/yyjson/yyjson.h"

#include <arrow/io/file.h>
#include <arrow/memory_pool.h>
#include <arrow/result.h>
#include <arrow/table.h>
#include <parquet/arrow/reader.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bigquant {

namespace {

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// parquet 读取 → arrow::Table (整张一次性读完, 与 DAI fetch 对齐)
// ----------------------------------------------------------------------------
std::shared_ptr<arrow::Table> read_parquet_table(const fs::path &path) {
  auto file_res = arrow::io::ReadableFile::Open(path.string());
  assert(file_res.ok() && "bigquant::import: parquet 文件无法打开");
  std::shared_ptr<arrow::io::ReadableFile> file = file_res.ValueOrDie();

  auto reader_res =
      parquet::arrow::OpenFile(file, arrow::default_memory_pool());
  assert(reader_res.ok() && "bigquant::import: parquet 文件构造 reader 失败");
  std::unique_ptr<parquet::arrow::FileReader> reader =
      std::move(reader_res).ValueOrDie();

  auto table_res = reader->ReadTable();
  assert(table_res.ok() && "bigquant::import: parquet ReadTable 失败");
  return std::move(table_res).ValueOrDie();
}

// ----------------------------------------------------------------------------
// (table_subset_to_json + serialize) → MonthTxn.day_files 一行
// ----------------------------------------------------------------------------
std::string render_day_json(const std::shared_ptr<arrow::Table> &t,
                            const std::vector<int64_t> &row_idxs) {
  yyjson_mut_doc *doc = table_subset_to_json(t, row_idxs);
  std::string s = misc::serialize_json(doc);
  yyjson_mut_doc_free(doc);
  return s;
}

// ----------------------------------------------------------------------------
// 路径解析
// ----------------------------------------------------------------------------
fs::path resolve_database_root(std::string_view database_root) {
  fs::path p(database_root);
  if (p.is_absolute())
    return p;
  return misc::git_root() / p;
}

// "YYYY-MM" → "YYYYMM"; 不匹配返回空串.
std::string parse_yyyymm_dir(const std::string &dirname) {
  if (dirname.size() != 7 || dirname[4] != '-')
    return {};
  for (size_t i = 0; i < dirname.size(); ++i) {
    if (i == 4)
      continue;
    if (!std::isdigit(static_cast<unsigned char>(dirname[i])))
      return {};
  }
  return dirname.substr(0, 4) + dirname.substr(5, 2);
}

// 扫 <root>/, 收集所有 "YYYY-MM" 子目录, 返回 yyyymm 升序列表.
std::vector<std::string> scan_yyyymm_dirs(const fs::path &root) {
  std::vector<std::string> out;
  for (auto &entry : fs::directory_iterator(root)) {
    if (!entry.is_directory())
      continue;
    std::string ym = parse_yyyymm_dir(entry.path().filename().string());
    if (!ym.empty())
      out.push_back(std::move(ym));
  }
  std::sort(out.begin(), out.end());
  return out;
}

fs::path parquet_path_of(const fs::path &root, const std::string &yyyymm,
                         const std::string &name) {
  std::string ym_dash = yyyymm.substr(0, 4) + "-" + yyyymm.substr(4, 2);
  return root / ym_dash / (name + ".parquet");
}

} // namespace

// ============================================================================
// import_parquet — 主入口
// ============================================================================
void import_parquet(std::string_view database_root,
                    const std::vector<TableSpec> &specs) {
  fs::path root = resolve_database_root(database_root);

  std::cout << "[bigquant.import] database=" << root.string() << "  ("
            << specs.size() << " tables)" << std::endl;

  // 1) 先收尾任何上次中断的事务 (manifest 已 commit 但 apply 未完)
  misc::journal::replay_all();

  if (!fs::exists(root)) {
    std::cout << "[bigquant.import] database root 不存在, 跳过" << std::endl;
    return;
  }

  auto yyyymms = scan_yyyymm_dirs(root);
  std::cout << "[bigquant.import] " << yyyymms.size() << " month(s)"
            << std::endl;

  // 2) 月 × 表 (整月替换覆盖)
  for (const auto &yyyymm : yyyymms) {
    const std::string month_start = yyyymm + "01";
    const std::string month_end = yyyymm + misc::month_last_dd(yyyymm);

    std::cout << "\n== " << yyyymm.substr(0, 4) << "-" << yyyymm.substr(4, 2)
              << " ==" << std::endl;
    for (const auto &spec : specs) {
      if (spec.kind == FetchKind::Static || spec.kind == FetchKind::Snapshot)
        continue; // Static / Snapshot 只走 DAI _meta (不切月; Snapshot 在线取 MAX 一日)
      // emit_meta 表 (trading_days/holidays) 跟普通 Partition+Day 一样切月, 下轮
      // bigquant::update 末尾自动从 day file 聚合 _meta, 无需 import 端特判.

      fs::path pq = parquet_path_of(root, yyyymm, spec.name);
      if (!fs::exists(pq))
        continue;

      std::cout << "  [" << spec.name << "] " << pq.filename().string()
                << " ... " << std::flush;

      auto t = read_parquet_table(pq);
      // 桶分 + PK upsert (与 store 共用 parse::bucket_by_visible_date)
      auto by_day = bucket_by_visible_date(t, spec, month_start, month_end);

      misc::journal::MonthTxn txn;
      txn.name = spec.name;
      txn.yyyymm = yyyymm;
      txn.day_files.reserve(by_day.size());

      int64_t n_rows = 0;
      for (auto &[vd, rows] : by_day) {
        assert(vd.size() == 8 && vd.substr(0, 6) == yyyymm);
        n_rows += static_cast<int64_t>(rows.size());
        txn.day_files.emplace_back(vd.substr(6, 2), render_day_json(t, rows));
      }

      misc::journal::commit(txn);

      std::cout << t->num_rows() << " row(s) -> " << by_day.size() << " day(s)"
                << " (kept=" << n_rows << ")" << std::endl;
    }
  }

  std::cout << "\n[bigquant.import] done" << std::endl;
}

} // namespace bigquant
