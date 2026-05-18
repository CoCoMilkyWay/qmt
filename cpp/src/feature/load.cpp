#include "feature/load.hpp"

#include "feature/axis.hpp"
#include "feature/pit.hpp"
#include "misc/affinity.hpp"
#include "misc/fs.hpp"
#include "package/yyjson/yyjson.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace feature {

namespace fs = std::filesystem;

namespace {

constexpr uint64_t AGG_MAGIC = 0x314747414d54514dULL; // "MQTMAGG1" little-endian tag
constexpr uint32_t AGG_VERSION = 1;
constexpr uint64_t FNV_OFFSET = 1469598103934665603ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

struct DayFile {
  std::string day;
  fs::path path;
};

template <class T>
void mix_pod(uint64_t &h, const T &v) {
  const auto *p = reinterpret_cast<const unsigned char *>(&v);
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    h ^= static_cast<uint64_t>(p[i]);
    h *= FNV_PRIME;
  }
}

void mix_string(uint64_t &h, std::string_view s) {
  for (unsigned char c : s) {
    h ^= static_cast<uint64_t>(c);
    h *= FNV_PRIME;
  }
}

std::vector<DayFile> enumerate_dayfiles(const char *file_name) {
  std::vector<DayFile> files;
  fs::path data_root = misc::git_root() / "data";
  assert(fs::exists(data_root));

  for (auto &y_ent : fs::directory_iterator(data_root)) {
    if (!y_ent.is_directory())
      continue;
    std::string y = y_ent.path().filename().string();
    if (y.size() != 4 || !std::isdigit(static_cast<unsigned char>(y[0])))
      continue;

    for (auto &m_ent : fs::directory_iterator(y_ent.path())) {
      if (!m_ent.is_directory())
        continue;
      std::string m = m_ent.path().filename().string();
      if (m.size() != 2)
        continue;

      for (auto &d_ent : fs::directory_iterator(m_ent.path())) {
        if (!d_ent.is_directory())
          continue;
        std::string dd = d_ent.path().filename().string();
        if (dd.size() != 2)
          continue;

        std::string day = y + m + dd;
        fs::path p = d_ent.path() / (std::string(file_name) + ".json");
        if (fs::exists(p))
          files.push_back(DayFile{std::move(day), std::move(p)});
      }
    }
  }
  std::sort(files.begin(), files.end(),
            [](const DayFile &a, const DayFile &b) { return a.path < b.path; });
  return files;
}

uint64_t hash_dayfiles(const std::vector<DayFile> &files) {
  fs::path root = misc::git_root();
  uint64_t h = FNV_OFFSET;
  mix_pod(h, AGG_VERSION);
  for (const DayFile &f : files) {
    std::string rel = fs::relative(f.path, root).generic_string();
    uint64_t size = static_cast<uint64_t>(fs::file_size(f.path));
    auto mtime = fs::last_write_time(f.path).time_since_epoch().count();
    mix_string(h, rel);
    mix_pod(h, size);
    mix_pod(h, mtime);
  }
  return h;
}

void write_bytes(std::string &out, const void *p, std::size_t n) {
  const char *c = static_cast<const char *>(p);
  out.append(c, n);
}

template <class T>
void write_pod(std::string &out, const T &v) {
  write_bytes(out, &v, sizeof(T));
}

void write_string(std::string &out, const std::string &s) {
  uint64_t n = static_cast<uint64_t>(s.size());
  write_pod(out, n);
  write_bytes(out, s.data(), s.size());
}

struct Reader {
  const std::string &buf;
  std::size_t pos = 0;

  template <class T>
  T read_pod() {
    assert(pos + sizeof(T) <= buf.size());
    T v;
    std::memcpy(&v, buf.data() + pos, sizeof(T));
    pos += sizeof(T);
    return v;
  }

  std::string read_string() {
    uint64_t n = read_pod<uint64_t>();
    assert(pos + n <= buf.size());
    std::string s(buf.data() + pos, static_cast<std::size_t>(n));
    pos += static_cast<std::size_t>(n);
    return s;
  }
};

fs::path aggregate_path(const ItfDesc &itf) {
  return misc::git_root() / "data" / "aggre" /
         (std::string(itf.file_name) + ".bin");
}

bool read_aggregate_cache(const ItfDesc &itf, uint64_t input_hash,
                          std::vector<AggregateRow> &rows) {
  std::string buf = misc::read_file_all(aggregate_path(itf));
  if (buf.empty())
    return false;

  Reader r{buf};
  uint64_t magic = r.read_pod<uint64_t>();
  if (magic != AGG_MAGIC)
    return false;
  uint32_t version = r.read_pod<uint32_t>();
  if (version != AGG_VERSION)
    return false;
  std::string name = r.read_string();
  if (name != itf.file_name)
    return false;
  uint64_t stored_hash = r.read_pod<uint64_t>();
  if (stored_hash != input_hash)
    return false;

  uint64_t n = r.read_pod<uint64_t>();
  rows.clear();
  rows.reserve(static_cast<std::size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    AggregateRow row;
    row.day = r.read_string();
    row.code = r.read_string();
    row.f0 = r.read_pod<float>();
    row.f1 = r.read_pod<float>();
    row.f2 = r.read_pod<float>();
    row.i0 = r.read_pod<int>();
    row.i1 = r.read_pod<int>();
    row.i2 = r.read_pod<int>();
    row.s0 = r.read_string();
    row.s1 = r.read_string();
    row.s2 = r.read_string();
    rows.push_back(std::move(row));
  }
  assert(r.pos == buf.size());
  return true;
}

void write_aggregate_cache(const ItfDesc &itf, uint64_t input_hash,
                           const std::vector<AggregateRow> &rows) {
  std::string out;
  write_pod(out, AGG_MAGIC);
  write_pod(out, AGG_VERSION);
  write_string(out, std::string(itf.file_name));
  write_pod(out, input_hash);
  write_pod(out, static_cast<uint64_t>(rows.size()));
  for (const AggregateRow &row : rows) {
    write_string(out, row.day);
    write_string(out, row.code);
    write_pod(out, row.f0);
    write_pod(out, row.f1);
    write_pod(out, row.f2);
    write_pod(out, row.i0);
    write_pod(out, row.i1);
    write_pod(out, row.i2);
    write_string(out, row.s0);
    write_string(out, row.s1);
    write_string(out, row.s2);
  }
  fs::path path = aggregate_path(itf);
  misc::atomic_write(path, out.data(), out.size());
}

std::vector<AggregateRow> build_aggregate(const ItfDesc &itf,
                                          const std::vector<DayFile> &files) {
  // per-file 并行解析: cache miss 时 dayfile JSON 是绝对热点 (read + yyjson_read +
  //   itf.aggregate). 每个 worker 取一个 file, 写到自己的 per_file[i] 子 vector,
  //   最后按 file 顺序 move-concat ⇒ 结果与串行完全等价 (cache 二进制确定性保留).
  //   itf.aggregate 只读 yyjson_val + day, 写到独立 out vector, 无共享状态.
  std::size_t n = files.size();
  std::vector<std::vector<AggregateRow>> per_file(n);

  unsigned n_threads = misc::Affinity::core_count();
  if (n_threads == 0) n_threads = 1;
  if (static_cast<std::size_t>(n_threads) > n)
    n_threads = static_cast<unsigned>(n);

  std::atomic<std::size_t> next{0};
  auto worker = [&]() {
    for (;;) {
      std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
      if (i >= n) break;
      const DayFile &f = files[i];
      std::string buf = misc::read_file_all(f.path);
      if (buf.empty()) continue;
      yyjson_doc *doc = yyjson_read(buf.data(), buf.size(), 0);
      assert(doc);
      yyjson_val *root = yyjson_doc_get_root(doc);
      assert(yyjson_is_arr(root));
      itf.aggregate(root, f.day, per_file[i]);
      yyjson_doc_free(doc);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(n_threads);
  for (unsigned t = 0; t < n_threads; ++t) threads.emplace_back(worker);
  for (auto &th : threads) th.join();

  std::size_t total = 0;
  for (const auto &v : per_file) total += v.size();
  std::vector<AggregateRow> rows;
  rows.reserve(total);
  for (auto &v : per_file) {
    for (auto &r : v) rows.push_back(std::move(r));
  }
  return rows;
}

std::vector<AggregateRow> load_or_build_aggregate(const ItfDesc &itf,
                                                  std::size_t &file_count,
                                                  bool &cache_hit) {
  std::vector<DayFile> files = enumerate_dayfiles(itf.file_name);
  file_count = files.size();
  uint64_t input_hash = hash_dayfiles(files);

  std::vector<AggregateRow> rows;
  cache_hit = read_aggregate_cache(itf, input_hash, rows);
  if (cache_hit)
    return rows;

  rows = build_aggregate(itf, files);
  write_aggregate_cache(itf, input_hash, rows);
  return rows;
}

} // namespace

void load_pit(const Axes &axes, PitPool &pool) {
  // ---- 1. 通用 prealloc: 迭代 ITFS[] ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    ITFS[i].prealloc(axes, pool);
  }

  // ---- 2. per-itf aggregate cache → replay 到 PIT ----
  std::vector<std::mutex> mu(static_cast<std::size_t>(axes.n_a()));
  std::size_t total_files = 0;
  std::size_t hit_count = 0;
  for (int i = 0; i < ITFS_COUNT; ++i) {
    const ItfDesc &itf = ITFS[i];
    std::size_t file_count = 0;
    bool cache_hit = false;
    std::vector<AggregateRow> rows =
        load_or_build_aggregate(itf, file_count, cache_hit);
    total_files += file_count;
    if (cache_hit)
      ++hit_count;
    std::vector<std::mutex> *mu_ptr = itf.is_event ? &mu : nullptr;
    itf.replay(rows, axes, pool, mu_ptr);
  }
  std::cout << "[feature][load] aggregate " << hit_count << "/" << ITFS_COUNT
            << " cache hits, " << total_files << " dayfiles" << std::endl;

  // ---- 3. 通用 post_sort: 迭代 ITFS[] ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_sort)
      ITFS[i].post_sort(pool);
  }

  // ---- 4. _meta overlay: 真盘前快照填充 row=last_d (hybrid 伪装收尾) ----
  //   仅 hybrid itf (当前: status) 在实盘当日 day file 未入库时, 用 static_data
  //   真盘前 09:00 值填充 row=last_d. 仅触及 row=last_d 一行, 历史天不动.
  //   详见 pit.hpp::apply_meta_overlays 注释.
  apply_meta_overlays(axes, pool);

  // ---- 5. 通用 post_ffill: 迭代 ITFS[] (网格 itf per-A forward fill) ----
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_ffill)
      ITFS[i].post_ffill(axes, pool);
  }
}

} // namespace feature
