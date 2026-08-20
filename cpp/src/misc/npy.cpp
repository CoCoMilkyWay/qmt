#include "misc/npy.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace misc {

namespace fs = std::filesystem;

namespace {

// header 字符串: "{'descr': '<f4', 'fortran_order': False, 'shape': (D, A,), }"
// 整体 (magic + ver + len + header) 长度对齐到 64; header 末尾用空格补齐 + '\n'.
std::string build_header(const char *descr,
                         std::span<const std::size_t> shape) {
  std::string body = "{'descr': '";
  body += descr;
  body += "', 'fortran_order': False, 'shape': (";
  for (std::size_t i = 0; i < shape.size(); ++i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%zu", shape[i]);
    body += buf;
    if (shape.size() == 1 || (i + 1) < shape.size())
      body += ", ";
  }
  body += "), }";

  // 总头部 = 6 (magic) + 2 (ver) + 2 (header_len) + header_str; 对齐 64
  std::size_t prefix = 6 + 2 + 2;
  std::size_t total = prefix + body.size() + 1; // +1 = 末尾 '\n'
  std::size_t pad = (64 - (total % 64)) % 64;
  body.append(pad, ' ');
  body.push_back('\n');
  return body;
}

void write_npy_raw(const fs::path &path, const char *descr,
                   const void *data, std::size_t elem_size,
                   std::span<const std::size_t> shape) {
  fs::create_directories(path.parent_path());

  std::string header = build_header(descr, shape);
  std::uint16_t header_len = static_cast<std::uint16_t>(header.size());
  assert(header.size() <= 0xFFFFu);

  std::size_t n = 1;
  for (std::size_t s : shape) n *= s;

  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  assert(f.is_open());

  // magic + version
  static constexpr unsigned char MAGIC[6] = {0x93, 'N', 'U', 'M', 'P', 'Y'};
  f.write(reinterpret_cast<const char *>(MAGIC), 6);
  unsigned char ver[2] = {1, 0};
  f.write(reinterpret_cast<const char *>(ver), 2);
  // header_len (little-endian u16)
  unsigned char hl[2] = {
      static_cast<unsigned char>(header_len & 0xFFu),
      static_cast<unsigned char>((header_len >> 8) & 0xFFu),
  };
  f.write(reinterpret_cast<const char *>(hl), 2);
  // header
  f.write(header.data(), static_cast<std::streamsize>(header.size()));
  // data
  if (n > 0) {
    f.write(reinterpret_cast<const char *>(data),
            static_cast<std::streamsize>(n * elem_size));
  }
  assert(f.good());
}

} // namespace

void write_npy_f4(const fs::path &path, std::span<const float> data,
                  std::span<const std::size_t> shape) {
  std::size_t n = 1;
  for (std::size_t s : shape) n *= s;
  assert(n == data.size());
  write_npy_raw(path, "<f4", data.data(), sizeof(float), shape);
}

void write_npy_i4(const fs::path &path, std::span<const std::int32_t> data,
                  std::span<const std::size_t> shape) {
  std::size_t n = 1;
  for (std::size_t s : shape) n *= s;
  assert(n == data.size());
  write_npy_raw(path, "<i4", data.data(), sizeof(std::int32_t), shape);
}

void write_npy_i1(const fs::path &path, std::span<const std::int8_t> data,
                  std::span<const std::size_t> shape) {
  std::size_t n = 1;
  for (std::size_t s : shape) n *= s;
  assert(n == data.size());
  write_npy_raw(path, "|i1", data.data(), sizeof(std::int8_t), shape);
}

} // namespace misc
