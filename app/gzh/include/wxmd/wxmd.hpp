#pragma once

#include <string>

#include "wxmd/cgi.hpp"      // IWYU pragma: export
#include "wxmd/fetch.hpp"    // IWYU pragma: export
#include "wxmd/html.hpp"     // IWYU pragma: export
#include "wxmd/markdown.hpp" // IWYU pragma: export
#include "wxmd/renderer.hpp" // IWYU pragma: export

namespace wxmd {

struct Article {
  std::string title;
  std::string author;
  std::string account; // 公众号名称
  std::string publish_time;
  std::string link;
  std::string markdown;
};

// 从文章原始 HTML 得到结果。
Article parse_article(const std::string &raw_html);

// 只做到「规范化 HTML」这一步，不转 Markdown。
std::string render_article_html(const std::string &raw_html);

// 从文章链接得到结果（抓取 + 解析）。
Article fetch_and_parse(const std::string &url);

} // namespace wxmd
