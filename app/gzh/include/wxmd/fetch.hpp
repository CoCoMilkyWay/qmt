#pragma once

#include <cstddef>
#include <string>

#include "wxmd/model.hpp"

namespace wxmd {

// 以微信内置浏览器的身份 GET 一个 mp.weixin.qq.com 链接，返回响应体。
// 直接由本机出口 IP 访问，不经任何代理；cookie 为空时不发送该头。
// 正文页是公开 URL，不需要 cookie；带 cookie 的那一路是历史消息列表在用。
std::string fetch_raw(const std::string &url, const std::string &cookie = "");

// 抓正文里的第 seq 张图，顺带把落盘用的文件名定下来（扩展名先看 Content-Type，
// 再退回 URL 上的 wx_fmt，都认不出就当场失败）。图片不在 mp.weixin.qq.com 而在
// 腾讯的图片 CDN 上，所以另开一个入口，而不是把 fetch_raw 的白名单撕开。
AssetFile fetch_asset(const std::string &url, size_t seq);

} // namespace wxmd
