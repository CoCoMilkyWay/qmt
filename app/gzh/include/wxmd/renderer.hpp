#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace wxmd {

// 依据 cgiDataNew 渲染出一份规范化的文章 HTML（标题、元信息、原文链接、正文）。
// 不含评论与底部互动栏：本项目只做正文导出。
std::string render_html(const nlohmann::json &cgi);

} // namespace wxmd
