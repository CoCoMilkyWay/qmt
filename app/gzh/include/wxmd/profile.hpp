#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace wxmd {

// 微信客户端侧凭证：在微信内打开公众号历史消息页时抓包得到。
// key / pass_ticket 时效很短（分钟级），过期后必须重新抓包，程序无法自行续签。
struct Credential {
  std::string biz; // __biz
  std::string uin;
  std::string key;
  std::string pass_ticket;
  std::string cookie; // 可为空
};

// 历史消息列表里的一篇文章。
struct ProfileEntry {
  std::string title;
  std::string author;
  std::string digest;
  std::string link; // content_url，已做 HTML 反转义，可直接喂给 fetch_article
  int64_t datetime = 0; // 发布时间，unix 秒
  int item_show_type = 0;
};

struct ProfilePage {
  std::vector<ProfileEntry> entries;
  bool can_continue = false;
  int next_offset = 0;
  int msg_count = 0; // 本页消息条数；一条消息可能含多篇文章
};

struct ProfileList {
  std::vector<ProfileEntry> entries;
  int next_offset = 0;       // 断点续传：下次从这里接着拉
  bool can_continue = false; // 为真说明是被 limit 截断的，还没拉完
};

// 拉取一页历史消息的原始响应体，不做任何解析。
// key 失效时微信返回的是风控页而非 JSON，用它可以直接看到原文。
std::string fetch_profile_raw(const Credential &cred, int offset, int count);

// 拉取并解析一页。
ProfilePage fetch_profile_page(const Credential &cred, int offset, int count);

// 从 offset 起反复翻页，直到没有更多或取满 limit（limit <= 0 表示不限）。
// interval_ms 是两次请求之间的间隔，用来避让风控。
// on_page 每翻完一页调用一次：key 可能在翻页途中过期而中止，
// 调用方据此把已拿到的部分随时落盘，不至于前功尽弃。
using PageCallback = std::function<void(const ProfilePage &page, int offset)>;
ProfileList fetch_profile_list(const Credential &cred, int offset, int count,
                               int limit, int interval_ms,
                               const PageCallback &on_page = {});

// 从 JSON 文件读取凭证。字段：biz / uin / key / pass_ticket / cookie(可选)。
Credential load_credential(const std::string &path);

// 打开一篇文章，从页内的 `var nickname = ...` 取出公众号名称。
// 名称只是展示用的可选信息：解析不到就返回空串，由调用方回退显示 __biz，
// 不因页面结构变动而中断捕获。
std::string fetch_account_name(const std::string &article_url);

} // namespace wxmd
