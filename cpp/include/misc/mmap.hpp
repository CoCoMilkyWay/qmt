#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace misc {

// ============================================================================
// MmapFile: POSIX mmap(MAP_PRIVATE) RAII.
//   MAP_PRIVATE 关键: 共享只读 page cache (零 IO, 多次 hit 不重复读盘); 写时 COW
//   (修改不脏文件, 不会持久化). 给 pool cache hit 路径用 — overlay / ffill 这类
//   末尾少量修改自动落到匿名页, 与文件无干涉.
//
//   - 文件不存在 / 空 ⇒ data()==nullptr & size()==0 (caller 直接当 miss).
//   - 析构自动 munmap; 中间 close(fd) 不影响已 mapped 区.
//   - 不读不写 .bin 内部, 也不解释字节; caller 自己 reinterpret_cast.
// ============================================================================
class MmapFile {
public:
  MmapFile() = default;
  explicit MmapFile(const std::filesystem::path &path) { open(path); }
  ~MmapFile() { close(); }

  MmapFile(const MmapFile &) = delete;
  MmapFile &operator=(const MmapFile &) = delete;
  MmapFile(MmapFile &&o) noexcept { steal(o); }
  MmapFile &operator=(MmapFile &&o) noexcept {
    if (this != &o) {
      close();
      steal(o);
    }
    return *this;
  }

  void open(const std::filesystem::path &path);
  void close();

  const std::uint8_t *data() const { return data_; }
  std::uint8_t *data() { return data_; }
  std::size_t size() const { return size_; }
  bool valid() const { return data_ != nullptr; }

private:
  void steal(MmapFile &o) {
    data_ = o.data_;
    size_ = o.size_;
    o.data_ = nullptr;
    o.size_ = 0;
  }

  std::uint8_t *data_ = nullptr;
  std::size_t size_ = 0;
};

} // namespace misc
