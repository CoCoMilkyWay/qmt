#include "wxmd/sync.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "wxmd/fetch.hpp"
#include "wxmd/html.hpp"
#include "wxmd/profile.hpp"
#include "wxmd/store.hpp"
#include "wxmd/wxmd.hpp"

namespace wxmd {
namespace {

// 抓两篇正文之间的间隔。没做成命令行开关：日常用不到，改了要重编译。
constexpr int kArticleIntervalMs = 800;

std::string format_time(int64_t unix_seconds) {
  const std::time_t raw = static_cast<std::time_t>(unix_seconds);
  std::tm parts{};
  localtime_r(&raw, &parts);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &parts);
  return buffer;
}

// 抓一篇的正文与图片，原子入库。
void download(AccountStore &store, Entry entry) {
  const std::string raw = fetch_raw(entry.link);

  const ValidateResult validated = validate_html(raw);
  entry.status = status_text(validated.status);

  // 已删除 / 违规的文章永远抓不回来。留一块墓碑让水位线照样前进，
  // 否则这一篇会把整条流水线永久卡在同一个位置。
  if (validated.status != ArticleStatus::Success) {
    store.commit(entry);
    std::cerr << "  [" << entry.status << "] " << entry.title << "\n";
    return;
  }

  std::vector<AssetFile> assets;
  const std::string markdown =
      render_article_markdown(raw, [&assets](const std::string &url) {
        assets.push_back(fetch_asset(url, assets.size() + 1));
        return "assets/" + assets.back().name;
      });
  store.commit(entry, markdown, assets);
}

// 把队列里的文章从早到晚逐篇落盘。不需要凭证，这一步永远能做。
void drain(AccountStore &store) {
  const std::vector<Entry> queue = store.pending();
  if (queue.empty()) {
    return;
  }

  std::cout << "  待落盘 " << queue.size() << " 篇\n";
  for (size_t i = 0; i < queue.size(); ++i) {
    if (i > 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(kArticleIntervalMs));
    }
    std::cerr << "\r  [" << (i + 1) << "/" << queue.size() << "] "
              << format_time(queue[i].datetime) << " " << queue[i].title
              << "        " << std::flush;
    download(store, queue[i]);
  }
  std::cerr << "\r  已落盘 " << queue.size() << " 篇，共 " << store.count()
            << " 篇                    \n";
}

// 增量发现：从最新一页往回翻，撞上水位线就停。空库则拉全量。
void discover(AccountStore &store) {
  std::vector<Entry> found = fetch_new_entries(
      store.account(), store.watermark(),
      [&store](const std::string &id) { return store.has(id); });

  if (found.empty()) {
    std::cout << "  没有新文章\n";
  } else {
    std::cout << "  发现 " << found.size() << " 篇新文章\n";
  }

  // 只有整趟翻完才写队列：翻页是从新往旧走的，中途被打断的话「已发现」和
  // 水位线之间会留一段空洞，那样入库会让水位线跳过没抓到的文章——空洞一旦被
  // 跳过，以后的早停永远不会再回头补。
  store.set_pending(std::move(found));
}

} // namespace

void sync_account(const std::string &root, const Account &account) {
  AccountStore store(root, account);
  std::cout << "\n[" << store.account().label() << "] 已缓存 " << store.count()
            << " 篇\n";

  drain(store);

  if (store.account().credential_seconds() == 0) {
    std::cout << "  没有可用凭证，跳过发现新文章\n";
    return;
  }
  if (!probe_credential(store.account())) {
    std::cout << "  凭证已失效，跳过发现新文章\n";
    return;
  }

  discover(store);
  drain(store);
}

} // namespace wxmd
