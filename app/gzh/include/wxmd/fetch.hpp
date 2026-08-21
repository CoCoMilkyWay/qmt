#pragma once

#include <string>

namespace wxmd {

// 以微信内置浏览器的身份 GET 一个 mp.weixin.qq.com 链接，返回响应体。
// 直接由本机出口 IP 访问，不经任何代理；cookie 为空时不发送该头。
std::string fetch_raw(const std::string &url, const std::string &cookie = "");

// 抓取公众号文章页面的原始 HTML。
std::string fetch_article(const std::string &url);

} // namespace wxmd
