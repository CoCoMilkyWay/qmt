#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

// 极简 .npy v1.0 writer (https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html).
// 仅支持 fortran_order=False, little-endian; dtype 由调用端给 descr 字串.
//
// 用法:
//   misc::write_npy_f4(path, data_f32, {n_d, n_a});
//   misc::write_npy_i4(path, data_i32, {n_trades});
//
// 父目录不存在自动创建. 写失败 assert.
namespace misc {

void write_npy_f4(const std::filesystem::path &path,
                  std::span<const float> data,
                  std::span<const std::size_t> shape);

void write_npy_i4(const std::filesystem::path &path,
                  std::span<const std::int32_t> data,
                  std::span<const std::size_t> shape);

} // namespace misc
