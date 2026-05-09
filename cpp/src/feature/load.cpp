#include "feature/load.hpp"

#include "feature/axis.hpp"
#include "feature/pit.hpp"
#include "misc/affinity.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <atomic>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace feature {

namespace fs = std::filesystem;

namespace {

// 单 (day, itf) 任务: ItfDesc* 指向 pit.cpp 内 ITFS[] 的某一项
struct Task {
  std::string day;   // YYYYMMDD (file's day; 即 visible_date)
  int         v_idx; // axes.floor_date(day); 网格场合 == axes.date_idx[day]
  const ItfDesc *itf;
  fs::path    path;
};

// 枚举 data/YYYY/MM/DD/<ITFS[i].file_name>.json 全部存在的文件 → tasks
//   不出现具体 itf 名 — 仅迭代 ITFS[].
std::vector<Task> enumerate_tasks(const Axes &axes) {
  std::vector<Task> tasks;
  fs::path data_root = misc::git_root() / "data";
  assert(fs::exists(data_root));

  for (auto &y_ent : fs::directory_iterator(data_root)) {
    if (!y_ent.is_directory()) continue;
    std::string y = y_ent.path().filename().string();
    if (y.size() != 4 || !std::isdigit(static_cast<unsigned char>(y[0]))) continue;

    for (auto &m_ent : fs::directory_iterator(y_ent.path())) {
      if (!m_ent.is_directory()) continue;
      std::string m = m_ent.path().filename().string();
      if (m.size() != 2) continue;

      for (auto &d_ent : fs::directory_iterator(m_ent.path())) {
        if (!d_ent.is_directory()) continue;
        std::string dd = d_ent.path().filename().string();
        if (dd.size() != 2) continue;

        std::string day = y + m + dd;
        int v_idx = axes.floor_date(day);
        // axes.dates 第 0 项之前的事件 (visible_date < dates[0]) 没有可写入的行 D, 整体跳过
        if (v_idx < 0) continue;

        for (int i = 0; i < ITFS_COUNT; ++i) {
          const ItfDesc &itf = ITFS[i];
          fs::path p = d_ent.path() / (std::string(itf.file_name) + ".json");
          if (!fs::exists(p)) continue;
          // 网格 itf 的 file's day 必须是交易日 (data 数据本身保证)
          if (!itf.is_event) {
            auto it = axes.date_idx.find(day);
            if (it == axes.date_idx.end()) continue; // 极端: 网格落到非交易日, 跳过
          }
          tasks.push_back(Task{day, v_idx, &itf, std::move(p)});
        }
      }
    }
  }
  return tasks;
}

void process_task(const Task &t, const Axes &axes, PitPool &pool,
                  std::vector<std::mutex> &mu) {
  std::string buf = misc::read_file_all(t.path);
  if (buf.empty()) return;
  yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
  assert(doc);
  yyjson_val *root = yyjson_doc_get_root(doc);
  assert(yyjson_is_arr(root));

  // 网格 itf 不需要 mu (无锁); 事件 itf 走 per-A mutex emplace
  std::vector<std::mutex> *mu_ptr = t.itf->is_event ? &mu : nullptr;
  t.itf->parse(root, t.v_idx, axes, pool, mu_ptr);

  yyjson_doc_free(doc);
}

} // namespace

void load_pit(const Axes &axes, PitPool &pool) {
  // ---- 1. 通用 prealloc: 迭代 ITFS[] ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    ITFS[i].prealloc(axes, pool);
  }

  // ---- 2. 枚举任务 ----
  std::vector<Task> tasks = enumerate_tasks(axes);
  std::cout << "[feature][load] " << tasks.size() << " (day, itf) tasks"
            << std::endl;

  // ---- 3. 并行 parse ----
  std::vector<std::mutex> mu(static_cast<std::size_t>(axes.n_a()));

  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  std::atomic<std::size_t> next{0};
  std::atomic<std::size_t> done{0};
  std::size_t total = tasks.size();

  auto worker = [&]() {
    for (;;) {
      std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= total) break;
      process_task(tasks[i], axes, pool, mu);
      done.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();
  assert(done.load() == total);

  // ---- 4. 通用 post_sort: 迭代 ITFS[] ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_sort) ITFS[i].post_sort(pool);
  }

  // ---- 5. 通用 post_ffill: 迭代 ITFS[] (网格 itf per-A forward fill) ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_ffill) ITFS[i].post_ffill(axes, pool);
  }
}

} // namespace feature
