#include "wxmd/fetch.hpp"

#include <httplib.h>

#include "wxmd/assert.hpp"

namespace wxmd {
namespace {

constexpr const char *kUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) "
    "Chrome/107.0.0.0 Safari/537.36 MicroMessenger/6.8.0(0x16080000) "
    "NetType/WIFI "
    "MiniProgramEnv/Mac MacWechat/WECHAT/WeChatBrowser XWEB/1191";

constexpr int kTimeoutSeconds = 30;

struct SplitUrl {
  std::string origin; // scheme://host[:port]
  std::string target; // path + query + fragment
};

SplitUrl split_url(const std::string &url) {
  const size_t scheme_end = url.find("://");
  WXMD_ASSERT(scheme_end != std::string::npos, "URL 缺少协议前缀: " + url);

  const size_t host_begin = scheme_end + 3;
  const size_t path_begin = url.find('/', host_begin);

  if (path_begin == std::string::npos) {
    return {url, "/"};
  }
  return {url.substr(0, path_begin), url.substr(path_begin)};
}

} // namespace

std::string fetch_article(const std::string &url) {
  WXMD_ASSERT(url.rfind("https://mp.weixin.qq.com", 0) == 0 ||
                  url.rfind("http://mp.weixin.qq.com", 0) == 0,
              "只支持 mp.weixin.qq.com 的文章链接: " + url);

  const SplitUrl parts = split_url(url);

  httplib::Client client(parts.origin);
  client.set_follow_location(true);
  client.set_connection_timeout(kTimeoutSeconds, 0);
  client.set_read_timeout(kTimeoutSeconds, 0);
  client.set_default_headers({
      {"User-Agent", kUserAgent},
      {"Referer", "https://mp.weixin.qq.com/"},
      {"Accept-Encoding", "identity"},
  });

  httplib::Result response = client.Get(parts.target);
  WXMD_ASSERT(static_cast<bool>(response),
              "请求失败: " + httplib::to_string(response.error()) + " (" + url +
                  ")");
  WXMD_ASSERT(response->status == 200,
              "HTTP 状态异常: " + std::to_string(response->status) + " (" +
                  url + ")");
  WXMD_ASSERT(!response->body.empty(), "响应内容为空: " + url);

  return response->body;
}

} // namespace wxmd
