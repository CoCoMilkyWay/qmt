#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "wxmd/profile.hpp"
#include "wxmd/proxy.hpp"

namespace wxmd {

// 凭证有效期，与上游 CREDENTIAL_LIVE_MINUTES 取值一致。
constexpr int kCredentialLiveMinutes = 25;

struct CapturedAccount {
  Credential cred;
  std::string url;      // 捕获到的那篇文章链接，用来反查公众号名字
  std::string nickname; // 惰性解析，未解析时为空
  int64_t timestamp_ms = 0;

  bool valid_at(int64_t now_ms) const;
  int remaining_seconds(int64_t now_ms) const;
};

int64_t now_ms();

// 从一次 HTTP 往返里认出微信凭证，认不出返回 false。
// 纯函数：不关心字节是代理抓的还是从别处导入的，换抓包方式照样复用。
bool parse_exchange(const Exchange &exchange, CapturedAccount &out);

// 按 __biz 去重的凭证仓库，同一个号只保留最新一次捕获。
class CredentialStore {
public:
  // 返回 true 表示这是一个此前没见过的公众号。
  bool offer(const CapturedAccount &account);
  std::vector<CapturedAccount> snapshot() const;
  void set_nickname(const std::string &biz, const std::string &nickname);

private:
  mutable std::mutex mutex_;
  std::vector<CapturedAccount> items_;
};

} // namespace wxmd
