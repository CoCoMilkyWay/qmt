#pragma once

#include <string>
#include <string_view>

namespace wxmd::str {

inline bool is_ascii_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
         c == '\v';
}

inline void replace_all(std::string &text, std::string_view from,
                        std::string_view to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

inline std::string replaced(std::string text, std::string_view from,
                            std::string_view to) {
  replace_all(text, from, to);
  return text;
}

inline std::string trim(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && is_ascii_space(text[begin])) {
    ++begin;
  }
  while (end > begin && is_ascii_space(text[end - 1])) {
    --end;
  }
  return text.substr(begin, end - begin);
}

// 去掉所有空白（含 U+00A0 不换行空格），用于判断内容是否实质为空。
inline std::string strip_whitespace(const std::string &text) {
  static constexpr std::string_view kNbsp = "\xC2\xA0";

  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (is_ascii_space(text[i])) {
      continue;
    }
    if (text.compare(i, kNbsp.size(), kNbsp) == 0) {
      i += kNbsp.size() - 1;
      continue;
    }
    out += text[i];
  }
  return out;
}

inline std::string escape_html(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

} // namespace wxmd::str
