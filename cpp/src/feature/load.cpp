#include "feature/load.hpp"

#include "feature/axis.hpp"
#include "feature/pit.hpp"
#include "misc/fs.hpp"
#include "misc/mmap.hpp"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
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
//     u64 cache_key     (FNV-1a over POOL_VERSION + file_name + dayfile list + axes meta)
//     u64 _reserved     (头对齐到 32)
//   [Section Table  n × 16 bytes]
//     per section: u64 file_offset, u64 bytes
//   [Section Data]
//     每段 8 字节对齐, content = PoolArr<T> raw bytes.
//
// hit: mmap → 校验 magic/version/key → cache_layout(Map) 把 PoolArr.data 指过去.
// miss: itf.build → cache_layout(Size) 算总长 → cache_layout(Write) 落盘 →
//       重新 mmap 取代 owned (可选, 这里不做; 当次留 owned, 下次再 hit 即可).
// ============================================================================

constexpr std::uint64_t POOL_MAGIC = 0x315441444c4f4f50ULL; // 'POOLDAT1'
// POOL_VERSION: PitPool 字段 / Ev struct / cache_layout 顺序变更时手动 +1.
constexpr std::uint32_t POOL_VERSION = 1;

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

// axes _meta 文件: trading_days / cn_stock_basic_info 决定 D/A 轴.
//   用文件内容 hash, 不用 mtime: emit_meta 每轮会重建 _meta, 内容不变不应失效.
//   cn_stock_static_data 只参与 apply_meta_overlays, overlay 每次都跑, 不入 pool cache key.
constexpr const char *AXIS_META_FILES[] = {
    "trading_days.json",
    "cn_stock_basic_info.json",
};

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

// 沿用旧 enumerate_dayfiles 逻辑 (扫 data/YYYY/MM/DD/<itf>.json, path 升序).
std::vector<DayFile> enumerate_dayfiles(const char *file_name) {
  std::vector<DayFile> files;
  fs::path data_root = misc::git_root() / "data";
  assert(fs::exists(data_root));

  for (auto &y_ent : fs::directory_iterator(data_root)) {
    if (!y_ent.is_directory()) continue;
    std::string y = y_ent.path().filename().string();
    if (y.size() != 4 || !std::isdigit(static_cast<unsigned char>(y[0])))
      continue;

    for (auto &m_ent : fs::directory_iterator(y_ent.path())) {
      if (!m_ent.is_directory()) continue;
      std::string m = m_ent.path().filename().string();
      if (m.size() != 2) continue;

      for (auto &d_ent : fs::directory_iterator(m_ent.path())) {
        if (!d_ent.is_directory()) continue;
        std::string dd = d_ent.path().filename().string();
        if (dd.size() != 2) continue;

        std::string day = y + m + dd;
        fs::path p = d_ent.path() / (std::string(file_name) + ".json");
        if (fs::exists(p)) files.push_back(DayFile{std::move(day), std::move(p)});
      }
    }
  }
  std::sort(files.begin(), files.end(),
            [](const DayFile &a, const DayFile &b) { return a.path < b.path; });
  return files;
}

std::uint64_t compute_axes_meta_key() {
  fs::path root = misc::git_root();
  std::uint64_t h = FNV_OFFSET;
  for (const char *meta : AXIS_META_FILES) {
    fs::path p = root / "data" / "_meta" / meta;
    mix_string(h, std::string_view(meta));
    bool exists = fs::exists(p);
    mix_pod(h, exists);
    if (!exists) continue;
    std::string buf = misc::read_file_all(p);
    std::uint64_t sz = static_cast<std::uint64_t>(buf.size());
    mix_pod(h, sz);
    mix_string(h, buf);
  }
  return h;
}

// cache key — 看 dayfile mtime/size + axes meta 内容 (+ 代码 POOL_VERSION + itf name 防错配).
//   dayfile 或 axes 内容变化 ⇒ cache 失效; 仅 _meta 重写但内容不变 ⇒ 复用.
std::uint64_t compute_cache_key(const ItfDesc &itf,
                                const std::vector<DayFile> &files,
                                std::uint64_t axes_meta_key) {
  fs::path root = misc::git_root();
  std::uint64_t h = FNV_OFFSET;
  mix_pod(h, POOL_VERSION);
  mix_string(h, itf.file_name);
  mix_pod(h, axes_meta_key);

  for (const DayFile &f : files) {
    std::string rel = fs::relative(f.path, root).generic_string();
    mix_string(h, rel);
    std::uint64_t sz = static_cast<std::uint64_t>(fs::file_size(f.path));
    auto mt = fs::last_write_time(f.path).time_since_epoch().count();
    mix_pod(h, sz);
    mix_pod(h, mt);
  }

  return h;
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
  // section table 大小未知, 但 cache_layout 顺序一致 ⇒ 先跑 Write 模式拿
  // table + 主体 buffer, 再回填 header.
  std::vector<SectionEntry> sections;
  std::string body;
  // section 区在文件内的起点 = sizeof(header) + n_sections * 16, 但 n_sections
  // 必须先知道. 跑一遍 cache_layout(Size) 数 sections 数:
  std::size_t n_sections_pred = 0;
  {
    CacheVisitor v;
    v.kind = CacheVisitor::Size;
    // total_bytes 不关心, 借这个 mode 数 visit 次数.
    // 用 lambda hack: 我们自己 wrap visitor; 这里偷个懒, 直接走 Write 模式但
    // align_base 先用 0, 之后再 fix offset (offset 都 += 真正 align_base).
    (void)v;
  }
  std::size_t section_base = 0; // 占位; Write 后回填
  {
    CacheVisitor v;
    v.kind = CacheVisitor::Write;
    v.write_out = &body;
    v.sections = &sections;
    v.write_align_base = 0; // 临时
    itf.cache_layout(pool, v);
  }
  // 真实 section_base = header + table size:
  std::uint32_t n_sections = static_cast<std::uint32_t>(sections.size());
  section_base = sizeof(PoolHeader) +
                 static_cast<std::size_t>(n_sections) * sizeof(SectionEntry);
  // 把 align_base 加回去 (Write 时 align_base=0 → body 内 offset 已自 8 对齐;
  // section_base 自身需要也 8 对齐 — header(32) + table(16N) 必然 8 对齐 ✓).
  for (auto &s : sections) s.first += section_base;

  // 拼最终输出: header + table + body
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
//   per-itf: 算 cache_key → 试 hit (mmap + 视图) → fallback miss (build + dump).
//   全部 itf 都跑完后, 末段 overlay + ffill 统一过一遍 (不入 cache, 永远跑最新代码).
// ============================================================================
void load_pit(const Axes &axes, PitPool &pool) {
  pool._cache_mmaps.resize(static_cast<std::size_t>(ITFS_COUNT));

  std::uint64_t axes_meta_key = compute_axes_meta_key();
  std::size_t hit_count = 0;
  std::size_t total_files = 0;
  for (int i = 0; i < ITFS_COUNT; ++i) {
    const ItfDesc &itf = ITFS[i];
    std::vector<DayFile> files = enumerate_dayfiles(itf.file_name);
    total_files += files.size();
    std::uint64_t key = compute_cache_key(itf, files, axes_meta_key);

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
            << " hits, " << total_files << " dayfiles" << std::endl;

  // overlay (写 row=last_d 一行; mmap COW 触发) — 永远跑, 不入 cache.
  apply_meta_overlays(axes, pool);

  // post_ffill (网格 per-A linear; mmap COW 写到 dirty 页) — 永远跑.
  for (int i = 0; i < ITFS_COUNT; ++i) {
    if (ITFS[i].post_ffill) ITFS[i].post_ffill(axes, pool);
  }
}

} // namespace feature
