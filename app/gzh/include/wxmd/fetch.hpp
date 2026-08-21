#pragma once

#include <string>

namespace wxmd {

// 抓取公众号文章页面的原始 HTML。
// 直接由本机出口 IP 访问 mp.weixin.qq.com，不经任何代理。
std::string fetch_article(const std::string &url);

} // namespace wxmd
