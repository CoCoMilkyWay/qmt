#include "wxmd/capture.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

#include <nlohmann/json.hpp>

#include "wxmd/assert.hpp"

#include "fsutil.hpp"
#include "strutil.hpp"

namespace wxmd {
namespace {

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

nlohmann::json account_json(const Account &account) {
  return {
      {"biz", account.biz},
      {"nickname", account.nickname},
      {"uin", account.uin},
      {"key", account.key},
      {"pass_ticket", account.pass_ticket},
      {"cookie", account.cookie},
      {"source_url", account.source_url},
      {"captured_ms", account.captured_ms},
  };
}

Account parse_account(const nlohmann::json &item) {
  Account account;
  account.biz = item.value("biz", std::string());
  account.nickname = item.value("nickname", std::string());
  account.uin = item.value("uin", std::string());
  account.key = item.value("key", std::string());
  account.pass_ticket = item.value("pass_ticket", std::string());
  account.cookie = item.value("cookie", std::string());
  account.source_url = item.value("source_url", std::string());
  account.captured_ms = item.value("captured_ms", int64_t{0});
  return account;
}

} // namespace

bool parse_exchange(const Exchange &exchange, Account &out) {
  Account account;
  account.biz = str::query_param(exchange.url, "__biz");
  account.uin = str::query_param(exchange.url, "uin");
  account.key = str::query_param(exchange.url, "key");
  account.pass_ticket = str::query_param(exchange.url, "pass_ticket");

  // 微信自己的页内跳转经常不带 key，这类请求没有凭证价值。
  if (account.biz.empty() || !account.has_credential()) {
    return false;
  }

  // 请求里的 Cookie 比这一次响应的 Set-Cookie 更稳：会话 cookie 早就种下了，
  // 不一定每次打开文章都会重新下发 wap_sid2。
  account.cookie = exchange.cookie;
  if (account.cookie.empty()) {
    for (const std::string &set_cookie : exchange.set_cookies) {
      const std::string pair = cookie_pair(set_cookie);
      if (pair.empty() || pair.find("EXPIRED") != std::string::npos) {
        continue;
      }
      if (!account.cookie.empty()) {
        account.cookie += "; ";
      }
      account.cookie += pair;
    }
  }

  account.source_url = exchange.url;
  account.captured_ms = now_ms();
  out = std::move(account);
  return true;
}

Credentials::Credentials(std::string path) : path_(std::move(path)) {
  const std::string text = fsu::read_file(path_);
  if (text.empty()) {
    return;
  }

  const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
  WXMD_ASSERT(root.is_array(), "凭证文件不是 JSON 数组: " + path_);

  for (const nlohmann::json &item : root) {
    Account account = parse_account(item);
    // 过期的读回来也没用，直接丢掉，省得后面处处再判一次。
    if (account.credential_seconds() > 0) {
      items_.push_back(std::move(account));
    }
  }
}

std::vector<Account> Credentials::snapshot() const {
  const std::lock_guard<std::mutex> guard(mutex_);
  return items_;
}

bool Credentials::offer(const Account &account) {
  const std::lock_guard<std::mutex> guard(mutex_);

  for (Account &existing : items_) {
    if (existing.biz == account.biz) {
      const std::string kept = existing.nickname;
      existing = account;
      if (existing.nickname.empty()) {
        existing.nickname = kept; // 名字解析过就不必再解析
      }
      return false;
    }
  }
  items_.push_back(account);
  return true;
}

void Credentials::set_nickname(const std::string &biz,
                               const std::string &nickname) {
  const std::lock_guard<std::mutex> guard(mutex_);
  for (Account &existing : items_) {
    if (existing.biz == biz) {
      existing.nickname = nickname;
      return;
    }
  }
}

bool Credentials::fill(Account &account) const {
  const std::lock_guard<std::mutex> guard(mutex_);

  const auto found = std::find_if(
      items_.begin(), items_.end(),
      [&account](const Account &item) { return item.biz == account.biz; });
  if (found == items_.end()) {
    return false;
  }

  const std::string nickname =
      account.nickname.empty() ? found->nickname : account.nickname;
  account = *found;
  account.nickname = nickname;
  return true;
}

void Credentials::save() const {
  const std::lock_guard<std::mutex> guard(mutex_);

  nlohmann::json root = nlohmann::json::array();
  for (const Account &account : items_) {
    root.push_back(account_json(account));
  }
  fsu::mkdirs(std::filesystem::path(path_).parent_path().string());
  fsu::write_atomic(path_, root.dump(2) + "\n", 0600);
}

} // namespace wxmd
