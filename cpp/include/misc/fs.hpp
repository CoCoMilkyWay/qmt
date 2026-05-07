#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// 通用文件系统工具：定位 git 根、整文件读、原子写。业务无关。
namespace misc {

// 从 cwd 向上爬找含 .git 的目录 (仓库根)。结果缓存为 static，第一次失败 assert。
std::filesystem::path git_root();

// 读整个文件到 string (二进制模式)。文件不存在不会 throw，返回空串。
std::string read_file_all(const std::filesystem::path &path);

// tmp + rename 原子写。父目录不存在会自动 create_directories。
void atomic_write(const std::filesystem::path &path, const char *data,
                  std::size_t len);

} // namespace misc
