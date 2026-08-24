#include "wxmd/store.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>

#include "wxmd/assert.hpp"

#include "fsutil.hpp"

namespace wxmd {
namespace {

constexpr const char *kAccountFile = "account.json";
constexpr const char *kIndexFile = "index.jsonl";
constexpr const char *kPendingFile = "pending.jsonl";
constexpr const char *kStagingDir = ".staging"; // 正在下载的那一篇，随时可丢

// 文章目录名里标题最多占这么多字节，超了按 UTF-8 边界截断。
constexpr size_t kTitleBudget = 60;

// 文件名里不能出现的字符一律换成下划线；连续下划线折叠成一个。
std::string sanitize(const std::string &text) {
  static constexpr std::string_view kForbidden = "/\\:*?\"<>|";

  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    const unsigned char raw = static_cast<unsigned char>(c);
    const bool bad = raw < 0x20 || raw == 0x7F ||
                     kForbidden.find(c) != std::string_view::npos || c == ' ';
    if (bad) {
      if (!out.empty() && out.back() != '_') {
        out += '_';
      }
      continue;
    }
    out += c;
  }
  while (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  return out;
}

// 按 UTF-8 码点边界截断，避免把多字节字符切成半个。
std::string truncate_utf8(const std::string &text, size_t budget) {
  if (text.size() <= budget) {
    return text;
  }
  size_t end = budget;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
    --end;
  }
  return text.substr(0, end);
}

std::string date_prefix(int64_t datetime) {
  const std::time_t raw = static_cast<std::time_t>(datetime);
  std::tm parts{};
  localtime_r(&raw, &parts);

  char buffer[16];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &parts);
  return buffer;
}

// 目录名 = 日期 + 标题 + 文章 id。日期让目录天然按时间排序，标题让人能直接
// 翻，id 保证唯一（标题会重复，微信也允许改标题）。
std::string article_dir_name(const Entry &entry) {
  const std::string title = truncate_utf8(sanitize(entry.title), kTitleBudget);
  std::string name = date_prefix(entry.datetime);
  if (!title.empty()) {
    name += "_" + title;
  }
  name += "_" + sanitize(entry.id);
  return name;
}

// index.jsonl 与 pending.jsonl 共用这一对：同一个 Entry，队列里的那些只是还没
// 有 status / dir / assets，空字段就不写出来，免得队列文件里全是占位符。
// 刻意不叫 to_json / from_json：那两个名字是 nlohmann 的 ADL 钩子，
// 给自己的类型起同名函数会跟它的序列化机制撞上。
nlohmann::json entry_json(const Entry &entry) {
  nlohmann::json out = {
      {"id", entry.id},
      {"datetime", entry.datetime},
      {"title", entry.title},
      {"link", entry.link},
  };
  if (!entry.status.empty()) {
    out["status"] = entry.status;
  }
  if (!entry.dir.empty()) {
    out["dir"] = entry.dir;
  }
  if (entry.assets > 0) {
    out["assets"] = entry.assets;
  }
  return out;
}

Entry parse_entry(const nlohmann::json &item) {
  Entry entry;
  entry.id = item.value("id", std::string());
  entry.datetime = item.value("datetime", int64_t{0});
  entry.title = item.value("title", std::string());
  entry.link = item.value("link", std::string());
  entry.status = item.value("status", std::string());
  entry.dir = item.value("dir", std::string());
  entry.assets = item.value("assets", 0);
  return entry;
}

// jsonl 逐行解析。最后一行若因崩溃而残缺（没有换行结尾），丢掉它——
// 这正是「中间态不入库」在读取侧的对应动作。
std::vector<nlohmann::json> read_jsonl(const std::string &path) {
  const std::string text = fsu::read_file(path);

  std::vector<nlohmann::json> out;
  std::istringstream stream(text);
  std::string line;
  size_t consumed = 0;
  while (std::getline(stream, line)) {
    consumed += line.size() + 1;
    if (consumed > text.size()) {
      warn(path + " 末行不完整（上次被打断），已丢弃");
      break;
    }
    if (line.empty()) {
      continue;
    }
    nlohmann::json parsed = nlohmann::json::parse(line, nullptr, false);
    WXMD_ASSERT(!parsed.is_discarded(), path + " 里有不是 JSON 的行: " + line);
    out.push_back(std::move(parsed));
  }
  return out;
}

std::string jsonl_text(const std::vector<Entry> &entries) {
  std::string out;
  for (const Entry &entry : entries) {
    out += entry_json(entry).dump() + "\n";
  }
  return out;
}

} // namespace

AccountStore::AccountStore(const std::string &root, const Account &account)
    : account_(account) {
  WXMD_ASSERT(!account_.biz.empty(), "AccountStore 需要 __biz");

  path_ = root + "/" + sanitize(account_.biz);
  fsu::mkdirs(path_);

  // __biz 里可能有 base64 的 '/'，被 sanitize 换成了下划线，所以目录名不能
  // 反推回 __biz，必须把原值记在 account.json 里。
  const std::string account_path = path_ + "/" + kAccountFile;
  const std::string account_text = fsu::read_file(account_path);
  std::string stored_nickname;
  if (!account_text.empty()) {
    const nlohmann::json info =
        nlohmann::json::parse(account_text, nullptr, false);
    WXMD_ASSERT(info.is_object(), "account.json 不是对象: " + account_path);
    WXMD_ASSERT(info.value("biz", std::string()) == account_.biz,
                "account.json 里的 biz 与目录不符: " + account_path);
    stored_nickname = info.value("nickname", std::string());
  }
  if (account_.nickname.empty()) {
    account_.nickname = stored_nickname;
  }
  if (account_text.empty() || account_.nickname != stored_nickname) {
    const nlohmann::json info = {{"biz", account_.biz},
                                 {"nickname", account_.nickname}};
    fsu::write_atomic(account_path, info.dump(2) + "\n");
  }

  for (const nlohmann::json &item : read_jsonl(path_ + "/" + kIndexFile)) {
    Entry entry = parse_entry(item);
    ids_.insert(entry.id);
    index_.push_back(std::move(entry));
  }

  for (const nlohmann::json &item : read_jsonl(path_ + "/" + kPendingFile)) {
    Entry entry = parse_entry(item);
    if (!has(entry.id)) {
      pending_.push_back(std::move(entry));
    }
  }

  // 上次崩在下载途中留下的半成品，开局就清掉：它天然不在索引里，没有价值。
  fsu::remove_all(path_ + "/" + kStagingDir);
}

void AccountStore::set_pending(std::vector<Entry> entries) {
  entries.erase(
      std::remove_if(entries.begin(), entries.end(),
                     [this](const Entry &entry) { return has(entry.id); }),
      entries.end());

  // 从早到晚入库，水位线才能单调前进；一旦中断，本地始终是「到某个时间点为止
  // 连续完整」的状态，下次接着往后走就行。
  std::stable_sort(
      entries.begin(), entries.end(),
      [](const Entry &a, const Entry &b) { return a.datetime < b.datetime; });

  pending_ = std::move(entries);
  flush_pending();
}

void AccountStore::commit(Entry entry, const std::string &markdown,
                          const std::vector<AssetFile> &assets) {
  WXMD_ASSERT(!has(entry.id), "这篇已经入库了: " + entry.id);
  WXMD_ASSERT(!entry.status.empty(), "入库的文章必须带 status: " + entry.id);
  WXMD_ASSERT(entry.datetime >= watermark(),
              "入库顺序必须从早到晚（这篇 " + std::to_string(entry.datetime) +
                  " 早于水位线 " + std::to_string(watermark()) +
                  "）: " + entry.id);

  if (!markdown.empty()) {
    const std::string staging = path_ + "/" + kStagingDir;
    fsu::remove_all(staging);
    fsu::mkdirs(staging);

    if (!assets.empty()) {
      fsu::mkdirs(staging + "/assets");
      for (const AssetFile &asset : assets) {
        fsu::write_sync(staging + "/assets/" + asset.name, asset.bytes, 0644);
      }
      fsu::fsync_path(staging + "/assets");
    }
    fsu::write_sync(staging + "/article.md", markdown, 0644);
    fsu::fsync_path(staging);

    entry.dir = article_dir_name(entry);
    entry.assets = static_cast<int>(assets.size());

    // 上次崩在 rename 之后、追加索引之前会留下一个孤儿目录：内容是完整的，但
    // 索引不认它，所以当作不存在，直接让位给这次的新副本。
    const std::string final_path = path_ + "/" + entry.dir;
    fsu::remove_all(final_path);

    std::error_code ec;
    std::filesystem::rename(staging, final_path, ec);
    WXMD_ASSERT(!ec, "rename 失败: " + staging + " → " + final_path + " (" +
                         ec.message() + ")");
    fsu::fsync_path(path_);
  }

  // 追加索引行是提交的分界点：这一行落盘之后这篇才算入库。
  fsu::append_line(path_ + "/" + kIndexFile, entry_json(entry).dump());

  ids_.insert(entry.id);
  pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                [&entry](const Entry &item) {
                                  return item.id == entry.id;
                                }),
                 pending_.end());
  index_.push_back(std::move(entry));
  flush_pending();
}

void AccountStore::flush_pending() const {
  fsu::write_atomic(path_ + "/" + kPendingFile, jsonl_text(pending_));
}

std::vector<Account> list_accounts(const std::string &root) {
  if (!fsu::exists(root)) {
    return {};
  }

  std::vector<Account> out;
  for (const std::filesystem::directory_entry &item :
       std::filesystem::directory_iterator(root)) {
    if (!item.is_directory()) {
      continue;
    }
    const std::string account_path = item.path().string() + "/" + kAccountFile;
    const std::string text = fsu::read_file(account_path);
    if (text.empty()) {
      warn("缓存目录里没有 account.json，跳过: " + item.path().string());
      continue;
    }
    const nlohmann::json info = nlohmann::json::parse(text, nullptr, false);
    WXMD_ASSERT(info.is_object(), "account.json 不是对象: " + account_path);

    Account account;
    account.biz = info.value("biz", std::string());
    account.nickname = info.value("nickname", std::string());
    WXMD_ASSERT(!account.biz.empty(), "account.json 缺少 biz: " + account_path);
    out.push_back(std::move(account));
  }

  // 目录名由 __biz 清洗而来，排序稳定就够了：同步顺序不影响任何不变式。
  std::sort(out.begin(), out.end(),
            [](const Account &a, const Account &b) { return a.biz < b.biz; });
  return out;
}

} // namespace wxmd
