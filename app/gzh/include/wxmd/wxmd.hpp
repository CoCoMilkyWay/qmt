#pragma once

#include <string>

#include "wxmd/assets.hpp" // IWYU pragma: export

namespace wxmd {

// 五级流水线的门面：原始 HTML 进，Markdown 出。标题、作者、公众号、发布时间与
// 原文链接由第 4 级（renderer）直接渲进正文，不再另开一套结构对外重复一遍。

// 规范化 HTML → Markdown。on_asset 非空时，正文图片的 src 先被换成它返回的本地
// 相对路径（图片字节由钩子自己取走），Markdown 里的图片链接随之指向本地文件。
std::string render_article_markdown(const std::string &raw_html,
                                    const AssetHook &on_asset = {});

// 只做到「规范化 HTML」这一步，不转 Markdown：调试渲染层用。
std::string render_article_html(const std::string &raw_html);

} // namespace wxmd
