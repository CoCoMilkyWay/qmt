#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "wxmd/model.hpp"
#include "wxmd/proxy.hpp"

namespace wxmd {

// 从一次 HTTP 往返里认出微信凭证，认不出返回 false。
// 纯函数：不关心字节是代理抓的还是从别处导入的，换抓包方式照样复用。
bool parse_exchange(const Exchange &exchange, Account &out);

// 按 __biz 去重的凭证表，同一个号只留最新一次捕获。抓包线程写、主线程读，
// 所以内部上锁。
//
// 落盘在 ~/.wxmd/credentials.json（0600）：存的是分钟级失效的短命数据，但足够
// 让 25 分钟内的重跑完全不必再碰微信——尤其是补齐上一轮没下完的文章。
// 构造即读回，已过期的直接丢掉；save() 由调用方在收下新凭证后显式调用。
class Credentials {
public:
  explicit Credentials(std::string path);

  std::vector<Account> snapshot() const;

  // 收下一个号的凭证，覆盖同号旧值（已解析出的名字留着，不必再解析一次）。
  // 返回 true 表示这是一个此前没见过的公众号。
  bool offer(const Account &account);
  void set_nickname(const std::string &biz, const std::string &nickname);

  // 把这个号的凭证填进 account；表里没有它就原样返回 false。
  bool fill(Account &account) const;

  void save() const;

private:
  mutable std::mutex mutex_;
  std::string path_;
  std::vector<Account> items_;
};

} // namespace wxmd
