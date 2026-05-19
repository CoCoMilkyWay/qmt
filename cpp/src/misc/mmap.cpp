#include "misc/mmap.hpp"

#include <cassert>
#include <cerrno>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace misc {

namespace fs = std::filesystem;

void MmapFile::open(const fs::path &path) {
  assert(data_ == nullptr);
  int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    // 文件不存在: caller 当 miss 处理, 不算错误.
    return;
  }
  struct stat st;
  int rc = ::fstat(fd, &st);
  assert(rc == 0);
  if (st.st_size == 0) {
    ::close(fd);
    return;
  }
  void *p = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE, fd, 0);
  ::close(fd); // 已 mapped, fd 可立即释放
  assert(p != MAP_FAILED);
  data_ = static_cast<std::uint8_t *>(p);
  size_ = static_cast<std::size_t>(st.st_size);
}

void MmapFile::close() {
  if (data_) {
    ::munmap(data_, size_);
    data_ = nullptr;
    size_ = 0;
  }
}

} // namespace misc
