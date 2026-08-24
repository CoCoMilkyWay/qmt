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
//     account.json                       __biz 与公众号名
//     index.jsonl                        已入库文章，严格按发布时间升序追加
//     pending.jsonl                      已发现未入库的队列，同样按时间升序
//     <日期>_<标题>_<id>/article.md      正文
//     <日期>_<标题>_<id>/assets/         正文图片
//
// 不变式：index.jsonl 覆盖 [最早一篇, 水位线] 之间的全部文章，中间没有空洞。
// 每篇的提交是原子的——内容先写进临时目录，整目录 rename 到位之后才往索引追加
// 一行。所以「索引里有这一行」等价于「这篇连图片都已经完整落盘」，被打断最多
// 丢掉正在下的那一篇，已入库的永不重下、也永不改写。
class AccountStore {
public:
  // 打开（或建立）这个号的缓存目录。account.nickname 非空时写回 account.json，
  // 为空则从盘上读回来——凭证抓到的名字和缓存里记的名字在这里合流。
  AccountStore(const std::string &root, const Account &account);

  const Account &account() const { return account_; }
  size_t count() const { return index_.size(); }

  // 已入库最新一篇的发布时间，也就是增量的水位线；空库返回 0（要拉全量）。
  int64_t watermark() const {
    return index_.empty() ? 0 : index_.back().datetime;
  }
  bool has(const std::string &id) const { return ids_.count(id) != 0; }

  // 已发现未入库的队列，按发布时间升序。返回副本：commit 会改动它。
  std::vector<Entry> pending() const { return pending_; }
  // 用新发现的文章覆盖队列（内部剔除已入库的，并按发布时间升序排好）。
  void set_pending(std::vector<Entry> entries);

  // 原子提交一篇。markdown 为空即墓碑：正文不可用（已删除 / 违规）的文章永远
  // 抓不回来，不建目录但索引照样推进，否则这一篇会把整条流水线永久卡在同一个
  // 位置——历史久的号踩到删除件是常态。
  void commit(Entry entry, const std::string &markdown = {},
              const std::vector<AssetFile> &assets = {});

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
