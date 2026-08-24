#pragma once

// 内部文件系统工具。本地缓存要求「中间态不入库」，落盘的原子性全靠
// 「写临时文件 → fsync → rename → fsync 父目录」这一套动作，集中在这里一份，
// 免得各处各写一遍还漏掉 fsync。

#include <fcntl.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "wxmd/assert.hpp"

namespace wxmd::fsu {

inline bool exists(const std::string &path) {
  return std::filesystem::exists(path);
}

inline void mkdirs(const std::string &path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  WXMD_ASSERT(std::filesystem::is_directory(path),
              "创建目录失败: " + path + " (" + ec.message() + ")");
}

// 文件不存在时返回空串：缺索引、缺队列都是「还没开始」的正常状态。
inline std::string read_file(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return {};
  }

  std::string out;
  char buffer[65536];
  for (;;) {
    const ssize_t got = ::read(fd, buffer, sizeof(buffer));
    WXMD_ASSERT(got >= 0, "读取失败: " + path);
    if (got == 0) {
      break;
    }
    out.append(buffer, static_cast<size_t>(got));
  }
  ::close(fd);
  return out;
}

// 把整个文件刷到盘上；目录也可以传，用来让 rename / create 生效。
inline void fsync_path(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  WXMD_ASSERT(fd >= 0, "无法打开以 fsync: " + path);
  const int rc = ::fsync(fd);
  ::close(fd);
  WXMD_ASSERT(rc == 0, "fsync 失败: " + path);
}

inline void write_sync(const std::string &path, const std::string &content,
                       mode_t mode) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  WXMD_ASSERT(fd >= 0, "无法写入: " + path);

  size_t written = 0;
  while (written < content.size()) {
    const ssize_t n =
        ::write(fd, content.data() + written, content.size() - written);
    WXMD_ASSERT(n > 0, "写入失败: " + path);
    written += static_cast<size_t>(n);
  }
  // 权限要显式设一次：open 的 mode 会被 umask 削掉，凭证文件必须是 0600。
  WXMD_ASSERT(::fchmod(fd, mode) == 0, "设置权限失败: " + path);
  WXMD_ASSERT(::fsync(fd) == 0, "fsync 失败: " + path);
  ::close(fd);
}

// 先写同目录下的临时文件再 rename：读者看到的永远是完整的旧版或完整的新版。
inline void write_atomic(const std::string &path, const std::string &content,
                         mode_t mode = 0644) {
  const std::string tmp = path + ".tmp";
  write_sync(tmp, content, mode);

  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  WXMD_ASSERT(!ec,
              "rename 失败: " + tmp + " → " + path + " (" + ec.message() + ")");
  fsync_path(std::filesystem::path(path).parent_path().string());
}

// 追加一行并落盘。索引就靠它推进：一行短文本的 O_APPEND 写是原子的，
// 加上 fsync 之后「这一行在索引里」就等价于「这篇已经完整入库」。
inline void append_line(const std::string &path, const std::string &line,
                        mode_t mode = 0644) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, mode);
  WXMD_ASSERT(fd >= 0, "无法追加: " + path);

  const std::string payload = line + "\n";
  const ssize_t n = ::write(fd, payload.data(), payload.size());
  WXMD_ASSERT(n == static_cast<ssize_t>(payload.size()),
              "追加写入不完整: " + path);
  WXMD_ASSERT(::fsync(fd) == 0, "fsync 失败: " + path);
  ::close(fd);
}

inline void remove_all(const std::string &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
  WXMD_ASSERT(!std::filesystem::exists(path),
              "删除失败: " + path + " (" + ec.message() + ")");
}

} // namespace wxmd::fsu
