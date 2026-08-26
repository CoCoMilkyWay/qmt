#pragma once

#include <string>

#include "wxmd/assets.hpp" // IWYU pragma: export

namespace wxmd {

// 五级流水线的后三级：cgiDataNew 脚本进，Markdown 出（求值 → 渲染 → 转换）。
// 前两级是 html.hpp 的 parse_article（解析 + 判状态 + 抠脚本），它必须先跑：
// 不可用的文章要的是墓碑而不是正文。
//
// 标题、作者、公众号、发布时间与原文链接由渲染那一级直接渲进正文，不另开一套
// 结构对外重复一遍。
//
// 正文图片的 src 先被 on_asset 换成它返回的本地相对路径（图片字节由钩子自己
// 取走），Markdown 里的图片链接随之指向本地文件。
std::string render_article_markdown(const std::string &cgi_script,
                                    const AssetHook &on_asset);

} // namespace wxmd
