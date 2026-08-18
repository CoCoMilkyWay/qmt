#include "feature/load.hpp"

#include "feature/axis.hpp"
#include "feature/pit.hpp"
#include "misc/fs.hpp"
#include "misc/mmap.hpp"
#include "misc/parquet.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace feature {

namespace fs = std::filesystem;

namespace {

// ============================================================================
// cache 文件 layout (data/pool/<itf>.bin):
//
//   [Header 32 bytes]
//     u64 magic         = POOL_MAGIC
//     u32 version       = POOL_VERSION
//     u32 n_sections
//     u64 cache_key     (FNV-1a over POOL_VERSION + file_name + 月 parquet 列表
//                        + axes 语义 hash)
//     u64 _reserved     (头对齐到 32)
//   [Section Table  n × 16 bytes]
//     per section: u64 file_offset, u64 bytes
//   [Section Data]
//     每段 8 字节对齐, content = PoolArr<T> raw bytes.
//
// hit: mmap → 校验 magic/version/key → cache_layout(Map) 把 PoolArr.data 指过去.
// miss: itf.build → cache_layout(Write) 落盘.
// ============================================================================

constexpr std::uint64_t POOL_MAGIC = 0x315441444c4f4f50ULL; // 'POOLDAT1'
// POOL_VERSION: PitPool 字段 / Ev struct / cache_layout 顺序变更时手动 +1.
constexpr std::uint32_t POOL_VERSION = 2;

constexpr std::uint64_t FNV_OFFSET = 1469598103934665603ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;

struct PoolHeader {
  std::uint64_t magic;
  std::uint32_t version;
  std::uint32_t n_sections;
  std::uint64_t cache_key;
  std::uint64_t _reserved;
};
static_assert(sizeof(PoolHeader) == 32, "PoolHeader must be 32 bytes");

using SectionEntry = std::pair<std::uint64_t, std::uint64_t>; // (offset, bytes)

template <class T>
void mix_pod(std::uint64_t &h, const T &v) {
  const auto *p = reinterpret_cast<const unsigned char *>(&v);
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    h ^= static_cast<std::uint64_t>(p[i]);
    h *= FNV_PRIME;
  }
}

void mix_string(std::uint64_t &h, std::string_view s) {
  for (unsigned char c : s) {
    h ^= static_cast<std::uint64_t>(c);
    h *= FNV_PRIME;
  }
}

// axes 语义 hash — 直接对 D 轴 / A 轴内容 hash (零文件依赖):
//   _meta / all_trading_days parquet 每轮 update 重写 (mtime 变) 但轴内容通常不变,
//   语义 hash 保证 cache 不被无谓打穿; 轴真变 (新交易日 / 新股) 时全部失效 (正确).
std::uint64_t compute_axes_key(const Axes &axes) {
  std::uint64_t h = FNV_OFFSET;
  for (const std::string &d : axes.dates) mix_string(h, d);
  for (const std::string &c : axes.codes) mix_string(h, c);
  return h;
}

// cache key — 看该 itf 全部月 parquet 的 relpath/size/mtime + axes 语义 key
//   (+ 代码 POOL_VERSION + itf name 防错配).
//   开放月重拉 → mtime 变 ⇒ 仅该 itf cache 失效; 其他 itf 不受影响.
std::uint64_t compute_cache_key(const ItfDesc &itf,
                                const std::vector<MonthFile> &files,
                                std::uint64_t axes_key) {
  fs::path root = misc::git_root();
  std::uint64_t h = FNV_OFFSET;
  mix_pod(h, POOL_VERSION);
  mix_string(h, itf.file_name);
  mix_pod(h, axes_key);

  for (const MonthFile &f : files) {
    std::string rel = fs::relative(f.path, root).generic_string();
    mix_string(h, rel);
    std::uint64_t sz = static_cast<std::uint64_t>(fs::file_size(f.path));
    auto mt = fs::last_write_time(f.path).time_since_epoch().count();
    mix_pod(h, sz);
    mix_pod(h, mt);
  }

  return h;
}

std::vector<MonthFile> enumerate_month_files(const char *file_name) {
  std::vector<MonthFile> files;
  for (auto &[ym, path] : misc::pq::list_month_files(file_name)) {
    files.push_back(MonthFile{ym, path});
  }
  return files;
}

fs::path pool_cache_path(const ItfDesc &itf) {
  return misc::git_root() / "data" / "pool" /
         (std::string(itf.file_name) + ".bin");
}

// hit 路径: mmap 文件, 校验 header, 走 cache_layout(Map) 把 PoolArr 视图指过去.
//   成功 → mmap 句柄交给 pool._cache_mmaps 持有, 返回 true.
//   任何不匹配 → 返回 false (caller 走 miss).
bool try_map_pool_cache(const ItfDesc &itf, std::uint64_t key, PitPool &pool,
                        misc::MmapFile &mmap_holder) {
  fs::path p = pool_cache_path(itf);
  if (!fs::exists(p)) return false;

  mmap_holder.open(p);
  if (!mmap_holder.valid()) return false;
  if (mmap_holder.size() < sizeof(PoolHeader)) return false;

  const auto *hdr =
      reinterpret_cast<const PoolHeader *>(mmap_holder.data());
  if (hdr->magic != POOL_MAGIC) return false;
  if (hdr->version != POOL_VERSION) return false;
  if (hdr->cache_key != key) return false;

  std::size_t table_off = sizeof(PoolHeader);
  std::size_t table_bytes =
      static_cast<std::size_t>(hdr->n_sections) * sizeof(SectionEntry);
  if (mmap_holder.size() < table_off + table_bytes) return false;
  const auto *sections = reinterpret_cast<const SectionEntry *>(
      mmap_holder.data() + table_off);

  CacheVisitor v;
  v.kind = CacheVisitor::Map;
  v.map_base = mmap_holder.data();
  v.map_sections = sections;
  v.cursor = 0;
  itf.cache_layout(pool, v);
  assert(v.cursor == hdr->n_sections &&
         "cache_layout section count 与文件头不一致 — 可能 POOL_VERSION 漏 bump");
  return true;
}

// miss 路径收尾: itf.build 已写好 pool, 这里 dump 到 cache file.
void dump_pool_cache(const ItfDesc &itf, std::uint64_t key, PitPool &pool) {
  std::vector<SectionEntry> sections;
  std::string body;
  {
    CacheVisitor v;
    v.kind = CacheVisitor::Write;
    v.write_out = &body;
    v.sections = &sections;
    v.write_align_base = 0; // 临时; 真实 section_base 下面回填
    itf.cache_layout(pool, v);
  }
  std::uint32_t n_sections = static_cast<std::uint32_t>(sections.size());
  std::size_t section_base =
      sizeof(PoolHeader) +
      static_cast<std::size_t>(n_sections) * sizeof(SectionEntry);
  // header(32) + table(16N) 必然 8 对齐 → body 内 offset 加 base 后仍 8 对齐.
  for (auto &s : sections) s.first += section_base;

  std::string out;
  out.reserve(section_base + body.size());
  PoolHeader hdr{};
  hdr.magic = POOL_MAGIC;
  hdr.version = POOL_VERSION;
  hdr.n_sections = n_sections;
  hdr.cache_key = key;
  out.append(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
  out.append(reinterpret_cast<const char *>(sections.data()),
             sections.size() * sizeof(SectionEntry));
  out.append(body);

  misc::atomic_write(pool_cache_path(itf), out.data(), out.size());
}

} // namespace

// ============================================================================
// load_pit — Phase 1 入口.
//   per-itf: 枚举月 parquet → 算 cache_key → 试 hit (mmap + 视图) → fallback
//   miss (build + dump). 全部 itf 都跑完后, 末段 overlay + ffill 统一过一遍
//   (不入 cache, 永远跑最新代码).
// ============================================================================
void load_pit(const Axes &axes, PitPool &pool) {
  pool._cache_mmaps.resize(static_cast<std::size_t>(ITFS_COUNT));

  std::uint64_t axes_key = compute_axes_key(axes);
  std::size_t hit_count = 0;
  std::size_t total_files = 0;
  for (int i = 0; i < ITFS_COUNT; ++i) {
    const ItfDesc &itf = ITFS[i];
    std::vector<MonthFile> files = enumerate_month_files(itf.file_name);
    total_files += files.size();
    std::uint64_t key = compute_cache_key(itf, files, axes_key);

    bool hit = try_map_pool_cache(itf, key, pool, pool._cache_mmaps[i]);
    if (!hit) {
      // mmap 句柄可能开了但 magic/key 不对 ⇒ close, 防错 view.
      pool._cache_mmaps[i].close();
      itf.build(axes, files, pool);
      dump_pool_cache(itf, key, pool);
    } else {
      ++hit_count;
    }
  }
  std::cout << "[feature][load] pool cache " << hit_count << "/" << ITFS_COUNT
            << " hits, " << total_files << " month parquet(s)" << std::endl;

  // overlay (写 row=last_d 一行; mmap COW 触发) — 永远跑, 不入 cache.
  apply_meta_overlays(axes, pool);

  // post_ffill (网格 per-A linear; mmap COW 写到 dirty 页) — 永远跑.
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_ffill) ITFS[i].post_ffill(axes, pool);
  }
}

} // namespace feature
