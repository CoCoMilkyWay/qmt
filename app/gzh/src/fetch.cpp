#include "wxmd/fetch.hpp"

#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <httplib.h>

#include "wxmd/assert.hpp"

#include "strutil.hpp"

namespace wxmd {
namespace {

constexpr const char *kUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) "
    "Chrome/107.0.0.0 Safari/537.36 MicroMessenger/6.8.0(0x16080000) "
    "NetType/WIFI "
    "MiniProgramEnv/Mac MacWechat/WECHAT/WeChatBrowser XWEB/1191";

constexpr int kTimeoutSeconds = 30;

// 连接级失败（没拿到 HTTP 响应）的重试。微信图片 CDN 偶发抖动/限流，
// 直接断言会把整轮 sync 拖死在一张图上，不值得。拿到响应之后的状态码
// 仍是真实信号，不重试——404 就是图没了，重试也没用。
constexpr int kMaxAttempts = 4;
constexpr int kRetryBaseMs = 1000;

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

std::string host_of(const std::string &url) {
  const size_t scheme_end = url.find("://");
  WXMD_ASSERT(scheme_end != std::string::npos, "URL 缺少协议前缀: " + url);

  const size_t host_begin = scheme_end + 3;
  const size_t host_end = url.find_first_of("/?#:", host_begin);
  return url.substr(host_begin, host_end == std::string::npos
                                    ? std::string::npos
                                    : host_end - host_begin);
}

struct Response {
  std::string body;
  std::string content_type; // 定扩展名要用：微信 CDN 的 URL 未必带后缀
};

Response get(const std::string &url, const std::string &cookie) {
  const SplitUrl parts = split_url(url);

  httplib::Headers headers = {
      {"User-Agent", kUserAgent},
      {"Referer", "https://mp.weixin.qq.com/"},
      {"Accept-Encoding", "identity"},
  };
  if (!cookie.empty()) {
    headers.emplace("Cookie", cookie);
  }

  httplib::Client client(parts.origin);
  client.set_follow_location(true);
  client.set_connection_timeout(kTimeoutSeconds, 0);
  client.set_read_timeout(kTimeoutSeconds, 0);
  client.set_default_headers(headers);

  httplib::Result response;
  for (int attempt = 1;; ++attempt) {
    response = client.Get(parts.target);
    if (static_cast<bool>(response) || attempt >= kMaxAttempts) {
      break;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kRetryBaseMs << (attempt - 1)));
  }
  WXMD_ASSERT(static_cast<bool>(response),
              "请求失败 (重试 " + std::to_string(kMaxAttempts) +
                  " 次后仍连不上): " + httplib::to_string(response.error()) +
                  " (" + url + ")");
  WXMD_ASSERT(response->status == 200,
              "HTTP 状态异常: " + std::to_string(response->status) + " (" +
                  url + ")");
  WXMD_ASSERT(!response->body.empty(), "响应内容为空: " + url);

  return {response->body, response->get_header_value("Content-Type")};
}

// 图片扩展名：先认 Content-Type，再退回 URL 上的 wx_fmt，两处都认不出就当场
// 失败——宁可停下来看一眼，也不要在缓存里堆一批认不出的文件。
std::string extension_of(const std::string &content_type,
                         const std::string &url) {
  static const std::unordered_map<std::string, std::string> kByMime = {
      {"image/jpeg", "jpg"},    {"image/jpg", "jpg"},   {"image/png", "png"},
      {"image/gif", "gif"},     {"image/webp", "webp"}, {"image/bmp", "bmp"},
      {"image/svg+xml", "svg"},
  };
  static const std::unordered_map<std::string, std::string> kByFormat = {
      {"jpeg", "jpg"},  {"jpg", "jpg"}, {"png", "png"},   {"gif", "gif"},
      {"webp", "webp"}, {"bmp", "bmp"}, {"other", "jpg"},
  };

  // Content-Type 可能带参数，如 "image/jpeg; charset=UTF-8"。
  std::string mime = str::lowered(content_type);
  const size_t semicolon = mime.find(';');
  if (semicolon != std::string::npos) {
    mime = mime.substr(0, semicolon);
  }

  const auto by_mime = kByMime.find(str::trim(mime));
  if (by_mime != kByMime.end()) {
    return by_mime->second;
  }

  const std::string format = str::query_param(url, "wx_fmt");
  const auto by_format = kByFormat.find(str::lowered(format));
  if (by_format != kByFormat.end()) {
    return by_format->second;
  }

  WXMD_ASSERT(false, "认不出图片格式（Content-Type: " + content_type +
                         "，wx_fmt: " + format + "）: " + url);
}

} // namespace

std::string fetch_raw(const std::string &url, const std::string &cookie) {
  WXMD_ASSERT(host_of(url) == "mp.weixin.qq.com",
              "只支持 mp.weixin.qq.com 的链接: " + url);
  return get(url, cookie).body;
}

AssetFile fetch_asset(const std::string &url, size_t seq) {
  // 正文里偶有协议相对地址（//mmbiz.qpic.cn/…），补齐后再走后面的检查。
  const std::string full = url.rfind("//", 0) == 0 ? "https:" + url : url;

  // 正文图片散落在 mmbiz.qpic.cn / mmecoa.qpic.cn / res.wx.qq.com 等域名下，
  // 逐个枚举跟不上微信改域名的节奏，按后缀限定在腾讯自家域即可。
  static constexpr std::string_view kSuffixes[] = {".qpic.cn", ".qlogo.cn",
                                                   ".qq.com"};
  const std::string host = host_of(full);
  bool allowed = false;
  for (std::string_view suffix : kSuffixes) {
    if (host.size() > suffix.size() &&
        host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0) {
      allowed = true;
      break;
    }
  }
  WXMD_ASSERT(allowed, "图片域名不在腾讯 CDN 范围内: " + full);

  const Response response = get(full, "");

  char name[32];
  std::snprintf(name, sizeof(name), "%03zu.%s", seq,
                extension_of(response.content_type, full).c_str());
  return {name, response.body};
}

} // namespace wxmd
