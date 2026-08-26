#pragma once

#include <string>

namespace wxmd {

enum class ArticleStatus {
  Success,   // 正常文章
  Deleted,   // 已被发布者删除
  Exception, // 状态异常（含风控），message 为原因
  Error,     // 页面结构无法识别
};

// 抓下来的一页文章。判状态与抠脚本是同一次解析的两个产物：正文 HTML 实测
// 3.05MB，分成两个入口就要用 lexbor 解析两遍。
struct ArticlePage {
  ArticleStatus status = ArticleStatus::Error;
  std::string message;    // Exception 时的原因，其余状态为空
  std::string cgi_script; // 定义 window.cgiDataNew 的脚本正文，仅 Success 时有
};

// 状态的名字，进断言消息与缓存索引。
std::string status_text(ArticleStatus status);

// 解析文章页：判定状态，Success 时顺带取出 cgiDataNew 脚本（取不到即页面结构
// 变了，当场断言）。
ArticlePage parse_article(const std::string &html);

} // namespace wxmd
