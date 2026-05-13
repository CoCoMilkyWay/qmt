#pragma once

#include <filesystem>
#include <string>

namespace verify::fs_util {

// 读整个文件到 string. 文件不存在返回空串.
std::string read_file_all(const std::filesystem::path &path);

// tmp + rename 原子写, 自动创建父目录.
void atomic_write(const std::filesystem::path &path, const char *data, std::size_t len);

} // namespace verify::fs_util
