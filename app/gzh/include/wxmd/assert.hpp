#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace wxmd {

[[noreturn]] inline void assert_fail(const char *file, int line,
                                     const char *expr, const std::string &msg) {
  std::fprintf(stderr, "[wxmd] assertion failed: %s\n  at %s:%d\n  %s\n", expr,
               file, line, msg.c_str());
  std::abort();
}

} // namespace wxmd

// 断言在 Release 下同样生效：本项目以「尽早失败」代替错误处理。
#define WXMD_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ::wxmd::assert_fail(__FILE__, __LINE__, #cond, (msg));                   \
    }                                                                          \
  } while (0)
