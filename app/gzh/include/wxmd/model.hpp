#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>

namespace wxmd {

// 主 flow 只在三样东西之间搬运：一个号、一篇文章、一张图。这里是它们唯一的
// 定义，网络层、缓存层、抓包层共用同一套字段，落盘的 JSON 就是它们的形态。

// 凭证有效期，与上游 CREDENTIAL_LIVE_MINUTES 取值一致。
constexpr int kCredentialMinutes = 25;

inline int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// 本地时区的 strftime。Entry.datetime 是 unix 秒，进目录名（"%Y-%m-%d"）和进度
// 行（带时分秒）各要一种格式，格式串由调用方给。
inline std::string format_time(int64_t unix_seconds, const char *format) {
  const std::time_t raw = static_cast<std::time_t>(unix_seconds);
  std::tm parts{};
  localtime_r(&raw, &parts);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), format, &parts);
  return buffer;
}

// 一个公众号：身份是 __biz，nickname 只用于展示，后半截是抓包得来的凭证。
// 凭证按 __biz 绑定、分钟级过期且无法续签，所以「这个号现在没凭证」是常态而
// 不是错误：那时仍能把已发现的文章补齐（正文页是公开 URL），只是发现不了新的。
struct Account {
  std::string biz;
  std::string nickname;

  std::string uin;
  std::string key;
  std::string pass_ticket;
  std::string cookie;     // 可为空
  std::string source_url; // 凭证抓自哪篇文章，用来反查 nickname
  int64_t captured_ms = 0;

  // __biz 是一串 base64，但没解析出名字时只能拿它顶着显示。
  const std::string &label() const { return nickname.empty() ? biz : nickname; }

  bool has_credential() const {
    return !uin.empty() && !key.empty() && !pass_ticket.empty();
  }

  // 凭证还剩几秒；没有凭证、或已经过期，都返回 0。
  int credential_seconds() const {
    if (!has_credential()) {
      return 0;
    }
    const int64_t left =
        (captured_ms + int64_t{kCredentialMinutes} * 60 * 1000 - now_ms()) /
        1000;
    return left > 0 ? static_cast<int>(left) : 0;
  }
};

// index.jsonl 与 pending.jsonl 里的一行：一篇文章。两个文件同一套字段，差别
// 只在 status——空的是「已发现未入库」，其余是入库结果（见 ArticleStatus）。
struct Entry {
  std::string id;       // mid_idx：跨轮次稳定的身份，增量靠它对账
  int64_t datetime = 0; // 发布时间，unix 秒
  std::string title;
  std::string link;
  std::string status;
  std::string dir; // 入库后的文章目录名；墓碑没有目录
  int assets = 0;  // 一起缓存下来的图片数
};

// 正文里的一张图。字节先攒在内存里，等 commit 一次性写进临时目录再整体
// rename——边下边往最终目录写就成了「中间态入库」。
struct AssetFile {
  std::string name; // NNN.ext，序号即正文里的出现顺序
  std::string bytes;
};

} // namespace wxmd
