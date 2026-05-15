#include "misc/journal.hpp"

#include "misc/date.hpp"
#include "misc/fs.hpp"
#include "misc/store.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace misc::journal {

namespace fs = std::filesystem;

// ============================================================================
// 路径布局
// ============================================================================

namespace {

constexpr const char *kSep = "__";

fs::path journal_root() { return git_root() / "data" / "_journal"; }

fs::path txn_dir(const std::string &name, const std::string &yyyymm) {
  assert(yyyymm.size() == 6);
  return journal_root() / (name + kSep + yyyymm);
}

fs::path manifest_path(const std::string &name, const std::string &yyyymm) {
  return txn_dir(name, yyyymm) / "manifest.json";
}

fs::path staged_day_path(const std::string &name, const std::string &yyyymm,
                         const std::string &dd) {
  assert(dd.size() == 2);
  return txn_dir(name, yyyymm) / (dd + ".json");
}

// 从 txn_dir 目录名反解 (name, yyyymm); 末 2 段以 kSep 切分. yyyymm 末 6 字符.
bool parse_txn_dirname(const std::string &dirname, std::string &name,
                       std::string &yyyymm) {
  const std::string sep = kSep;
  auto pos = dirname.rfind(sep);
  if (pos == std::string::npos)
    return false;
  if (pos + sep.size() + 6 != dirname.size())
    return false;
  name = dirname.substr(0, pos);
  yyyymm = dirname.substr(pos + sep.size());
  return !name.empty();
}

// ============================================================================
// manifest JSON 读写
// ============================================================================

struct Manifest {
  std::string name;
  std::string yyyymm;
  std::vector<std::string> days_with_data; // 升序
};

void write_manifest(const Manifest &m) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_strncpy(doc, root, "name", m.name.data(), m.name.size());
  yyjson_mut_obj_add_strncpy(doc, root, "yyyymm", m.yyyymm.data(),
                             m.yyyymm.size());
  yyjson_mut_val *arr = yyjson_mut_arr(doc);
  for (auto &dd : m.days_with_data)
    yyjson_mut_arr_add_strncpy(doc, arr, dd.data(), dd.size());
  yyjson_mut_obj_add_val(doc, root, "days_with_data", arr);

  size_t out_len = 0;
  char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, &out_len);
  assert(json);
  atomic_write(manifest_path(m.name, m.yyyymm), json, out_len);
  std::free(json);
  yyjson_mut_doc_free(doc);
}

Manifest read_manifest(const fs::path &path) {
  std::string buf = read_file_all(path);
  assert(!buf.empty());
  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_obj(root));
  Manifest m;
  m.name = yyjson_get_str(yyjson_obj_get(root, "name"));
  m.yyyymm = yyjson_get_str(yyjson_obj_get(root, "yyyymm"));
  assert(!m.name.empty() && m.yyyymm.size() == 6);
  yyjson_val *arr = yyjson_obj_get(root, "days_with_data");
  assert(yyjson_is_arr(arr));
  size_t idx, n;
  yyjson_val *v;
  yyjson_arr_foreach(arr, idx, n, v) {
    assert(yyjson_is_str(v));
    m.days_with_data.emplace_back(yyjson_get_str(v));
  }
  yyjson_doc_free(doc);
  return m;
}

// ============================================================================
// apply (idempotent replay) — manifest 已 commit 后的提交逻辑
// ============================================================================

// data/YYYY/MM/DD/<name>.json
fs::path target_day_path(const std::string &name, const std::string &yyyymm,
                         const std::string &dd) {
  return git_root() / "data" / yyyymm.substr(0, 4) / yyyymm.substr(4, 2) / dd /
         (name + ".json");
}

void apply(const Manifest &m) {
  const std::string yyyy = m.yyyymm.substr(0, 4);
  const std::string mm = m.yyyymm.substr(4, 2);
  const int last_day = std::stoi(month_last_dd(m.yyyymm));

  std::unordered_set<std::string> data_set(m.days_with_data.begin(),
                                           m.days_with_data.end());

  // 1) 有数据日: staged → target (atomic rename, 覆盖目标)
  for (auto &dd : m.days_with_data) {
    fs::path src = staged_day_path(m.name, m.yyyymm, dd);
    fs::path dst = target_day_path(m.name, m.yyyymm, dd);
    if (fs::exists(src)) {
      fs::create_directories(dst.parent_path());
      fs::rename(src, dst); // POSIX atomic, 覆盖已存在 dst
    }
    // 不存在 → 假定上次 replay 已 rename, 跳过 (dst 应已存在或后续 update 会发现)
  }

  // 2) 整月无数据日 ([01, 月末] 但不在 data_set): 删除残留 target file
  //    用 ASCII DD 字符串比较: "01"~"31", 字典序与数值序一致
  for (int d = 1; d <= last_day; ++d) {
    char dd_buf[3];
    std::snprintf(dd_buf, sizeof(dd_buf), "%02d", d);
    std::string dd(dd_buf);
    if (data_set.count(dd))
      continue;
    fs::path dst = target_day_path(m.name, m.yyyymm, dd);
    if (fs::exists(dst))
      fs::remove(dst);
  }

  // 3) 更新 _empty.json (atomic): 本月 [01, 月末] 内非数据日全部入 set
  store::EmptyMonth em = store::read_empty_month(yyyy, mm);
  store::EmptySet &set = em[m.name];
  for (auto &dd : m.days_with_data)
    set.erase(dd);
  for (int d = 1; d <= last_day; ++d) {
    char dd_buf[3];
    std::snprintf(dd_buf, sizeof(dd_buf), "%02d", d);
    std::string dd(dd_buf);
    if (!data_set.count(dd))
      set.insert(dd);
  }
  store::write_empty_month(yyyy, mm, em);

  // 4) cleanup: 删 staged dir + manifest
  fs::path tdir = txn_dir(m.name, m.yyyymm);
  std::error_code ec;
  fs::remove_all(tdir, ec); // 错误吃掉 (目录可能已被部分清理)
}

} // namespace

// ============================================================================
// 对外接口
// ============================================================================

void commit(const MonthTxn &txn) {
  assert(!txn.name.empty());
  assert(txn.yyyymm.size() == 6);
  const std::string last_dd = month_last_dd(txn.yyyymm);

  fs::path tdir = txn_dir(txn.name, txn.yyyymm);

  // 防御性: 清掉残留 staged dir
  std::error_code ec;
  fs::remove_all(tdir, ec);
  fs::create_directories(tdir);

  // stage day files
  std::vector<std::string> days_with_data;
  days_with_data.reserve(txn.day_files.size());
  for (auto &[dd, bytes] : txn.day_files) {
    assert(dd.size() == 2);
    assert(dd >= "01" && dd <= last_dd);
    atomic_write(staged_day_path(txn.name, txn.yyyymm, dd), bytes.data(),
                 bytes.size());
    days_with_data.push_back(dd);
  }
  std::sort(days_with_data.begin(), days_with_data.end());

  // 写 manifest (commit point)
  Manifest m{txn.name, txn.yyyymm, std::move(days_with_data)};
  write_manifest(m);

  // 立即 apply (idempotent)
  apply(m);
}

void replay_all() {
  fs::path root = journal_root();
  if (!fs::exists(root))
    return;
  for (auto &entry : fs::directory_iterator(root)) {
    if (!entry.is_directory())
      continue;
    std::string dirname = entry.path().filename().string();
    std::string name, yyyymm;
    if (!parse_txn_dirname(dirname, name, yyyymm)) {
      // 命名不符: 视作孤儿删除
      std::error_code ec;
      fs::remove_all(entry.path(), ec);
      continue;
    }
    fs::path mpath = entry.path() / "manifest.json";
    if (fs::exists(mpath)) {
      Manifest m = read_manifest(mpath);
      apply(m);
    } else {
      // 无 manifest → 未到 commit point → 孤儿 staged, 删除
      std::error_code ec;
      fs::remove_all(entry.path(), ec);
    }
  }
}

} // namespace misc::journal
