#include "wxmd/capture.hpp"

#include <algorithm>
#include <chrono>

#include "strutil.hpp"

namespace wxmd {
namespace {

std::string query_param(const std::string &url, const std::string &name) {
  const size_t query_begin = url.find('?');
  if (query_begin == std::string::npos) {
    return {};
  }

  const std::string needle = name + "=";
  size_t at = query_begin + 1;
  while (at < url.size()) {
    const size_t end = std::min(url.find('&', at), url.size());
    if (url.compare(at, needle.size(), needle) == 0) {
      return str::percent_decode(
          url.substr(at + needle.size(), end - at - needle.size()));
    }
    at = end + 1;
  }
  return {};
}

// Set-Cookie 原文形如 "name=value; Path=/; HttpOnly"，只取最前面的键值对。
std::string cookie_pair(const std::string &set_cookie) {
  const std::string head =
      str::trim(set_cookie.substr(0, set_cookie.find(';')));
  const size_t equal = head.find('=');
  if (equal == std::string::npos || equal + 1 >= head.size()) {
    return {};
  }
  return head;
}

} // namespace

int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool CapturedAccount::valid_at(int64_t at_ms) const {
  return remaining_seconds(at_ms) > 0;
}

int CapturedAccount::remaining_seconds(int64_t at_ms) const {
  const int64_t deadline = timestamp_ms + kCredentialLiveMinutes * 60 * 1000;
  const int64_t left = (deadline - at_ms) / 1000;
  return left > 0 ? static_cast<int>(left) : 0;
}

bool parse_exchange(const Exchange &exchange, CapturedAccount &out) {
  Credential cred;
  cred.biz = query_param(exchange.url, "__biz");
  cred.uin = query_param(exchange.url, "uin");
  cred.key = query_param(exchange.url, "key");
  cred.pass_ticket = query_param(exchange.url, "pass_ticket");

  // 微信自己的页内跳转经常不带 key，这类请求没有凭证价值。
  if (cred.biz.empty() || cred.uin.empty() || cred.key.empty() ||
      cred.pass_ticket.empty()) {
    return false;
  }

  // 请求里的 Cookie 比这一次响应的 Set-Cookie 更稳：会话 cookie 早就种下了，
  // 不一定每次打开文章都会重新下发 wap_sid2。
  std::string cookie = exchange.cookie;
  if (cookie.empty()) {
    for (const std::string &set_cookie : exchange.set_cookies) {
      const std::string pair = cookie_pair(set_cookie);
      if (pair.empty() || pair.find("EXPIRED") != std::string::npos) {
        continue;
      }
      if (!cookie.empty()) {
        cookie += "; ";
      }
      cookie += pair;
    }
  }

  cred.cookie = cookie;
  out.cred = cred;
  out.url = exchange.url;
  out.nickname.clear();
  out.timestamp_ms = now_ms();
  return true;
}

bool CredentialStore::offer(const CapturedAccount &account) {
  const std::lock_guard<std::mutex> guard(mutex_);

  for (CapturedAccount &existing : items_) {
    if (existing.cred.biz == account.cred.biz) {
      const std::string kept = existing.nickname;
      existing = account;
      existing.nickname = kept; // 名字解析过就不必再解析
      return false;
    }
  }
  items_.push_back(account);
  return true;
}

std::vector<CapturedAccount> CredentialStore::snapshot() const {
  const std::lock_guard<std::mutex> guard(mutex_);
  return items_;
}

void CredentialStore::set_nickname(const std::string &biz,
                                   const std::string &nickname) {
  const std::lock_guard<std::mutex> guard(mutex_);
  for (CapturedAccount &existing : items_) {
    if (existing.cred.biz == biz) {
      existing.nickname = nickname;
      return;
    }
  }
}

} // namespace wxmd
