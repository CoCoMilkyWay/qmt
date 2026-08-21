#pragma once

#include <string>

namespace wxmd {

enum class ArticleStatus {
  Success,   // 正常文章
  Deleted,   // 已被发布者删除
  Exception, // 状态异常（含风控），message 为原因
  Error,     // 页面结构无法识别
};

struct ValidateResult {
  ArticleStatus status = ArticleStatus::Error;
  std::string message; // Success 时为 comment_id，Exception 时为异常原因
};

// 校验文章 html 是否抓取成功，并提取 comment_id。
ValidateResult validate_html(const std::string &html);

// 提取包含 `window.cgiDataNew` 的 script 脚本正文（不含 script 标签）。
// 找不到时断言失败。
std::string extract_cgi_script(const std::string &html);

} // namespace wxmd
