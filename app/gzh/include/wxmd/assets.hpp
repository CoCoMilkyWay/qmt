#pragma once

#include <functional>
#include <string>

namespace wxmd {

// 把一张远端图片换成本地相对路径的钩子：入参是图片地址，返回写在 src 上的
// 相对路径；返回空串表示这张图保持原样（仍指向 CDN）。
// 同一个地址在一篇文章里只回调一次，重复引用共用同一个本地文件。
using AssetHook = std::function<std::string(const std::string &url)>;

// 把规范化 HTML 里所有 <img> 的 src 交给 on_asset 改写。
// 放在「渲染完 HTML、还没转 Markdown」之间：Markdown 里的图片链接直接由
// src 生成，改在这一层就不必去动 markdown 的规则集。
std::string localize_images(const std::string &html, const AssetHook &on_asset);

} // namespace wxmd
