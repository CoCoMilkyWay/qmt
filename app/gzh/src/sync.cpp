#include "wxmd/sync.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "wxmd/assert.hpp"
#include "wxmd/egress.hpp"
#include "wxmd/fetch.hpp"
#include "wxmd/html.hpp"
#include "wxmd/profile.hpp"
#include "wxmd/store.hpp"
#include "wxmd/wxmd.hpp"

namespace wxmd {
namespace {

// 抓完待提交的一篇。字节全攒在内存里：入库必须串行（.staging 只有一个目录），
// 所以抓取的产物得先在这里等一等，不能边下边往最终目录写。
struct Fetched {
  Entry entry;
  std::string markdown; // 空即墓碑
  std::vector<AssetFile> assets;
};

// 抓一篇的正文与图片，不落盘。纯函数式的一段，可以并发跑：网络层无共享状态，
// quickjs 每次调用自建 runtime，lexbor 也只在自己的 Document 上工作。
//
// 返回空 optional = 空洞，这篇留在 pending 下轮重抓，两种成因：
//   1. 正文抓不到（fetch_raw 重试耗尽）；
//   2. 正文抓到了，但任意一张图抓不到——入库必须是「连图完整」的原子整体，
//      缺图的文章不脏入库，整篇作空洞下轮重来（宁可重抓整篇，不在索引里堆半成品）。
// 墓碑不受此影响：已删除 / 违规文章 parse_article
// 就判出来了，本就没图，照常入库推进水位线。
std::optional<Fetched> fetch_article(Entry entry) {
  auto raw_opt = fetch_raw(entry.link);
  if (!raw_opt) {
    warn("空洞: " + entry.title);
    return std::nullopt;
  }
  const ArticlePage page = parse_article(*raw_opt);
  entry.status = status_text(page.status);

  Fetched out;
  bool asset_failed = false;
  // 已删除 / 违规的文章永远抓不回来，留一块墓碑（markdown 为空）让水位线照样
  // 前进，否则这一篇会把整条流水线永久卡在同一个位置。
  if (page.status == ArticleStatus::Success) {
    out.markdown = render_article_markdown(
        page.cgi_script, [&out, &asset_failed](const std::string &url) {
          auto asset = fetch_asset(url, out.assets.size() + 1);
          if (!asset) {
            // 任一张图失败就标记整篇为空洞：不把缺图的文章脏入库。返回空只是让
            // localize_images
            // 继续跑完不中断，这篇最后会被丢弃，已下的图也一并作废。
            asset_failed = true;
            return std::string{};
          }
          out.assets.push_back(*std::move(asset));
          return "assets/" + out.assets.back().name;
        });
  }
  if (asset_failed) {
    warn("空洞（缺图）: " + entry.title);
    return std::nullopt;
  }
  out.entry = std::move(entry);
  return out;
}

// 把队列里的文章落盘。不需要凭证，这一步永远能做。
//
// 抓取并发、提交串行，但提交不死等队头：哪篇抓完哪篇先入库，允许留空洞。
// 严格按序 + 队头一篇慢 = 后面抓完的全干等、进度停死；放开后慢的那篇自己留作
// 空洞（仍在 pending，下轮补），其余照常前进。水位线因此取「已入库里最新的」而
// 非「最后提交的」，空洞落在水位线之下、由 pending 保留。
//
// 派发窗口取 workers，把内存框住：「在抓的 + 等提交的」合计不超过 workers 篇
// （实测一篇连图约 1.2MB，敞开跑会让几十篇图片字节同时挂在内存里）。
void drain(AccountStore &store) {
  const std::vector<Entry> queue = store.pending();
  if (queue.empty()) {
    return;
  }

  const EgressPolicy &policy = egress();
  const size_t workers = static_cast<size_t>(policy.workers);

  std::cout << "  待落盘 " << queue.size() << " 篇（" << workers << " 并发"
            << (policy.tunneled() ? "，走隧道" : "，直连") << "）\n";

  std::mutex mutex;
  std::condition_variable dispatch_cv; // 有活了 / 窗口挪了 → 唤醒 worker
  std::condition_variable ready_cv;    // 有篇抓完了 → 唤醒主线程
  // index → 抓取产物。空洞（fetch_article 返回 nullopt）不入这里，只把 holes
  // 加一，好让主线程的完成计数往前走、这篇留在 pending 等下轮。
  std::map<size_t, Fetched> ready;
  size_t next_dispatch = 0;
  size_t committed = 0; // 已提交的篇数（不含空洞），用于窗口与进度
  size_t holes = 0;     // 抓取失败、留在 pending 等下轮的篇数
  // 已派发但还没回到主线程的篇数，给心跳用：>0 说明活还在 worker 手里。
  size_t in_flight = 0;

  const auto work = [&] {
    for (;;) {
      size_t index = 0;
      {
        std::unique_lock<std::mutex> lock(mutex);
        // 窗口按「已回到主线程的」算（committed + holes）而不单按
        // committed：空洞不推进 committed，单按它算窗口会在前几个全空洞时
        // 卡死——派不出新活，主线程也等不到 ready。按已返回的算，
        // 在途 = 派发 - 已返回 ≤ workers。
        dispatch_cv.wait(lock, [&] {
          return next_dispatch >= queue.size() ||
                 next_dispatch < committed + holes + workers;
        });
        if (next_dispatch >= queue.size()) {
          return;
        }
        index = next_dispatch++;
        ++in_flight;
      }

      // 这里不再有篇间等待：频率由 egress 的 throttle() 逐请求发牌，两种出网
      // 方式共用同一个闸门。
      auto done = fetch_article(queue[index]);
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (done) {
          ready.emplace(index, std::move(*done));
        } else {
          ++holes;
        }
        --in_flight;
      }
      ready_cv.notify_one();
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(workers);
  for (size_t i = 0; i < workers; ++i) {
    pool.emplace_back(work);
  }

  // 完成计数 = 已提交 + 空洞。队头不再阻塞：ready 里有谁就提交谁，
  // 挑最小 index 只是为了让入库顺序尽量贴近发布顺序（水位线前进得好看些）。
  const auto processed = [&] { return committed + holes; };
  while (processed() < queue.size()) {
    Fetched done;
    {
      std::unique_lock<std::mutex> lock(mutex);
      // 心跳：一篇都没就绪时，每 2s 喊一声，不让屏幕静默。
      while (ready.empty() && processed() < queue.size()) {
        if (ready_cv.wait_for(lock, std::chrono::seconds(2),
                              [&] { return !ready.empty(); })) {
          break;
        }
        std::cerr << "\r  抓取中… 已提交 " << committed << "/" << queue.size()
                  << "，在抓 " << in_flight << " 篇        " << std::flush;
      }
      if (ready.empty()) {
        continue;
      }
      const auto slot = ready.begin();
      done = std::move(slot->second);
      ready.erase(slot);
    }

    std::cerr << "\r  [" << (committed + 1) << "/" << queue.size() << "] "
              << format_time(done.entry.datetime, "%Y-%m-%d %H:%M:%S") << " "
              << done.entry.title << "        " << std::flush;
    store.commit(done.entry, done.markdown, done.assets);
    if (done.markdown.empty()) {
      std::cerr << "\r  [" << done.entry.status << "] " << done.entry.title
                << "        \n";
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      ++committed;
    }
    dispatch_cv.notify_all();
  }

  for (std::thread &worker : pool) {
    worker.join();
  }
  std::cerr << "\r  已落盘 " << committed << " 篇，共 " << store.count()
            << " 篇                    \n";
  if (holes > 0) {
    std::cerr << "  留下 " << holes
              << " 篇空洞（抓取失败，仍在队列，下轮补）\n";
  }
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
  // 水位线之间会留一段没翻到的，而它们既不在 index 也不在 pending，下一轮的
  // 早停又恰好停在水位线上，永远不会回头补。set_pending 会把 found 并入既有
  // pending（上一轮 drain 留下的空洞），而不是覆盖掉它们。
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
