#pragma once

#include <string>
#include <string_view>
#include <utility>

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

// HTTP 头部字段名大小写不敏感，比对前统一降级。
inline std::string lowered(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  return out;
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

// URL query 里的 key / pass_ticket 是百分号编码的，取出来必须先还原，
// 否则重新发请求时会被二次编码。
inline std::string percent_decode(const std::string &text) {
  const auto hex_value = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return -1;
  };

  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '+') {
      out += ' ';
      continue;
    }
    if (text[i] == '%' && i + 2 < text.size()) {
      const int high = hex_value(text[i + 1]);
      const int low = hex_value(text[i + 2]);
      if (high >= 0 && low >= 0) {
        out += static_cast<char>(high * 16 + low);
        i += 2;
        continue;
      }
    }
    out += text[i];
  }
  return out;
}

// 历史消息接口返回的 title / content_url 等字段仍带 HTML 实体，
// 链接必须还原成 & 才能直接请求。
inline std::string unescape_html(const std::string &text) {
  static constexpr std::pair<std::string_view, char> kEntities[] = {
      {"&amp;", '&'},  {"&lt;", '<'},   {"&gt;", '>'},
      {"&quot;", '"'}, {"&#39;", '\''},
  };

  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    bool matched = false;
    if (text[i] == '&') {
      for (const auto &[entity, decoded] : kEntities) {
        if (text.compare(i, entity.size(), entity) == 0) {
          out += decoded;
          i += entity.size();
          matched = true;
          break;
        }
      }
    }
    if (!matched) {
      out += text[i];
      ++i;
    }
  }
  return out;
}

} // namespace wxmd::str
