#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "wxmd/model.hpp"

namespace wxmd {

// 一个公众号的本地缓存目录：
//
//   <root>/<账号目录>/
//     account.json                  __biz 与公众号名
//     index.jsonl                   已入库文章，按提交顺序追加（不保证升序）
//     pending.jsonl                 已发现未入库的队列，按发布时间升序
//     <日期>_<标题>_<id>/article.md  正文
//     <日期>_<标题>_<id>/assets/     正文图片
//
// 不变式：index.jsonl 里每一篇都是原子落盘的（内容写进临时目录、整目录 rename
// 之后才往索引追加一行），但篇与篇之间允许乱序、允许留空洞——某篇抓不到就跳过，
// 它仍留在 pending 里，下一轮再抓。水位线 = 已入库里最新的发布时间，空洞落在
// 水位线之下、由 pending 保留。被打断最多丢掉正在下的那一篇，已入库的永不重下、
// 也永不改写。
class AccountStore {
public:
  // 打开（或建立）这个号的缓存目录。account.nickname 非空时写回 account.json，
  // 为空则从盘上读回来——凭证抓到的名字和缓存里记的名字在这里合流。
  AccountStore(const std::string &root, const Account &account);

  const Account &account() const { return account_; }
  size_t count() const { return index_.size(); }

  // 已入库文章里最新的发布时间，也就是增量的水位线；空库返回 0（要拉全量）。
  // 入库允许乱序，所以这里取 max 而非末尾——末尾只是「最后提交的」，未必最新。
  int64_t watermark() const {
    int64_t max = 0;
    for (const Entry &e : index_) {
      if (e.datetime > max) {
        max = e.datetime;
      }
    }
    return max;
  }
  bool has(const std::string &id) const { return ids_.count(id) != 0; }

  // 已发现未入库的队列，按发布时间升序。返回副本：commit 会改动它。
  std::vector<Entry> pending() const { return pending_; }

  // 把新发现的文章并入既有队列（剔除已入库的、按 id 去重、按发布时间
  // 升序排好）。合并而非覆盖：上一轮 drain 可能因单篇抓取失败留下空洞，
  // 那些篇仍在队列里，这里只追加新发现的，不能把它们冲掉。
  void set_pending(std::vector<Entry> entries);

  // 原子提交一篇。markdown 为空即墓碑：正文不可用（已删除 / 违规）的文章永远
  // 抓不回来，不建目录但索引照样推进，否则这一篇会把整条流水线永久卡在同一个
  // 位置——历史久的号踩到删除件是常态。
  void commit(Entry entry, const std::string &markdown,
              const std::vector<AssetFile> &assets);

private:
  void flush_pending() const;

  std::string path_; // 账号目录的完整路径
  Account account_;
  std::vector<Entry> index_;
  std::unordered_set<std::string> ids_;
  std::vector<Entry> pending_;
};

// 缓存根目录下已有的公众号（只填 biz 与 nickname）。根目录不存在时返回空。
std::vector<Account> list_accounts(const std::string &root);

} // namespace wxmd
