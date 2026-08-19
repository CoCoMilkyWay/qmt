#pragma once

#include <chrono>
#include <iostream>
#include <string>

namespace misc {

class Timer {
public:
  Timer(const std::string &label = "")
      : label_(label), start_(std::chrono::high_resolution_clock::now()) {}

  // 自成一行 (label 前不留空行, 行末带换行) — 调用方若要与前段视觉分隔,
  //   自己在 label 外显式插入空行, 不依赖这里的隐式格式.
  ~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = end - start_;
    std::cout << label_ << " " << elapsed.count() * 1000 << "ms" << std::endl;
  }

private:
  std::string label_;
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

} // namespace misc
