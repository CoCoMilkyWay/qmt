#pragma once

#include "package/yyjson/yyjson.h"

#include <cstddef>
#include <filesystem>
#include <string>

// 通用文件系统工具: 定位 git 根、整文件读、原子写。业务无关。
namespace misc {

// 从 cwd 向上爬找含 .git 的目录 (仓库根)。结果缓存为 static, 第一次失败 assert。
std::filesystem::path git_root();

// 读整个文件到 string (二进制模式)。文件不存在不会 throw, 返回空串。
std::string read_file_all(const std::filesystem::path &path);

// tmp + rename 原子写。父目录不存在会自动 create_directories。
void atomic_write(const std::filesystem::path &path, const char *data,
                  std::size_t len);

// 序列化 yyjson_mut_doc 为 PRETTY_TWO_SPACES + atomic_write; doc 不释放 (caller 负责).
// 三处行式 JSON 落盘 (bigquant store / tushare store / import) 共用样板.
void atomic_write_json(const std::filesystem::path &path, yyjson_mut_doc *doc);

// 序列化 yyjson_mut_doc 为 PRETTY_TWO_SPACES 文本 (caller 负责释放 doc).
// 当不直接写盘 (需进一步打包, e.g. journal MonthTxn) 时使用.
std::string serialize_json(yyjson_mut_doc *doc);

} // namespace misc
