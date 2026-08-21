#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace wxmd {

// 依据 cgiDataNew 渲染出一份规范化的文章 HTML（标题、元信息、原文链接、正文）。
// 不含评论与底部互动栏：本项目只做正文导出。
std::string render_html(const nlohmann::json &cgi);

// 从 cgiDataNew 中提取标题，供文件命名等场景使用。
std::string extract_title(const nlohmann::json &cgi);

// 从 cgiDataNew 中提取原文链接。cgi 里的 link 带 &amp; 实体，这里还原为 &。
std::string extract_link(const nlohmann::json &cgi);

} // namespace wxmd
