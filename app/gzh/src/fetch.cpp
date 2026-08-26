#include "wxmd/fetch.hpp"

#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <httplib.h>

#include "wxmd/assert.hpp"
#include "wxmd/egress.hpp"

#include "config.hpp"
#include "strutil.hpp"

namespace wxmd {
namespace {

constexpr int kTimeoutSeconds = 30;

// 重试按出网方式分两套，因为「重试」对两者含义不同：
//   直连 —— 只在没拿到响应时重试（连接被拒/超时）；拿到响应后状态码是真实信号，
//           404 再来也没用。指数退避。
//   隧道 —— 每请求换出口 IP，重试就是换 IP 再来，连状态码一起重试：出口里混着
//           被拉黑的（403）和隧道抖动的（517/超时），换几次就过。固定短间隔，
//           403 是那个 IP 的问题，等久也不会变好。
// 连接超时对隧道压到 5s：挂住的出口要么秒回 517/200、要么彻底不通，5s 足以区分
// 「慢出口」与「死出口」；读超时仍 30s，大图慢慢传正常。
struct Retry {
  int attempts;
  int backoff_ms; // 直连按指数增长，隧道固定
};
constexpr Retry kDirectRetry{4, 1000};
constexpr Retry kTunnelRetry{5, 300};
constexpr int kTunnelConnectSeconds = 5;

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

// 重试耗尽仍拿不到 200 → 静默返回空。喊不喊由调用方决定：凭证/翻页那一路 fatal
// （见 profile.cpp 的 assert，消息自带 URL），正文与图片失败都由 fetch_article
// 用标题喊一行（整篇作空洞，下轮重来）。
std::optional<Response> get(const std::string &url, const std::string &cookie,
                            Route route) {
  const SplitUrl parts = split_url(url);

  // 不设 Accept-Encoding：httplib 只在请求没带这个头时才自己填 gzip 并自动
  // 解压，手写 identity 等于把压缩关掉。正文 HTML 占每篇下载字节的 86%，
  // gzip 省 78%。
  httplib::Headers headers = {
      {"User-Agent", config::kUserAgent},
      {"Referer", std::string("https://") + config::kTargetHost + "/"},
  };
  if (!cookie.empty()) {
    headers.emplace("Cookie", cookie);
  }

  httplib::Client client(parts.origin);
  client.set_follow_location(true);
  client.set_connection_timeout(kTimeoutSeconds, 0);
  client.set_read_timeout(kTimeoutSeconds, 0);
  client.set_default_headers(headers);

  const EgressPolicy &policy = egress();
  const bool tunneled = route == Route::Tunneled && policy.tunneled();
  const Retry retry = tunneled ? kTunnelRetry : kDirectRetry;
  if (tunneled) {
    client.set_proxy(policy.host, policy.port);
    client.set_proxy_basic_auth(policy.user, policy.pass);
    // 每请求新连接才能换出口 IP；复用连接会让重定向留在同一个 IP 上。
    client.set_keep_alive(false);
    client.set_connection_timeout(kTunnelConnectSeconds, 0);
  }

  httplib::Result response;
  for (int attempt = 1;; ++attempt) {
    throttle();
    response = client.Get(parts.target);

    // 隧道重试连状态码一起算（下一次是另一个 IP）；直连只在没拿到响应时重试。
    const bool ok =
        static_cast<bool>(response) && (!tunneled || response->status == 200);
    if (ok || attempt >= retry.attempts) {
      break;
    }
    const int backoff =
        tunneled ? retry.backoff_ms : retry.backoff_ms << (attempt - 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
  }
  if (!response || response->status != 200 || response->body.empty()) {
    return std::nullopt;
  }
  return Response{response->body, response->get_header_value("Content-Type")};
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

std::optional<std::string> fetch_raw(const std::string &url,
                                     const std::string &cookie, Route route) {
  WXMD_ASSERT(host_of(url) == config::kTargetHost,
              std::string("只支持 ") + config::kTargetHost + " 的链接: " + url);
  // 统一升 https：微信下发的 content_url 常是 http://，已落盘的
  // pending.jsonl 里也存着旧的 http 链接。走隧道时 http→https 的 301
  // 响应体不稳定，会卡到超时。
  std::string safe = url;
  if (safe.rfind("http://", 0) == 0) {
    safe.replace(0, 4, "https");
  }
  auto response = get(safe, cookie, route);
  if (!response) {
    return std::nullopt;
  }
  return std::move(response->body);
}

std::optional<AssetFile> fetch_asset(const std::string &url, size_t seq) {
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

  auto response = get(full, "", Route::Tunneled);
  if (!response) {
    return std::nullopt;
  }

  const std::string ext = extension_of(response->content_type, full);
  char name[32];
  std::snprintf(name, sizeof(name), "%03zu.%s", seq, ext.c_str());
  return AssetFile{name, std::move(response->body)};
}

} // namespace wxmd
