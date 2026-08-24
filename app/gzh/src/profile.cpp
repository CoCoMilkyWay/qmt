#include "wxmd/profile.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <unordered_set>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "wxmd/assert.hpp"
#include "wxmd/fetch.hpp"

#include "strutil.hpp"

namespace wxmd {
namespace {

constexpr const char *kEndpoint = "https://mp.weixin.qq.com/mp/profile_ext";

// 每页条数与翻页间隔。没做成命令行开关：日常用不到，太大容易撞风控，
// 改了要重编译，正好逼着想清楚再改。
constexpr int kPageSize = 20;
constexpr int kIntervalMs = 1000;

// 一页历史消息。翻页逻辑之外没人关心这些字段，所以不出这个文件。
struct Page {
  std::vector<Entry> entries;
  bool can_continue = false;
  int next_offset = 0;
};

// 响应体不是 JSON 时（key 失效会返回风控页），截一段原文放进断言消息里。
std::string head_of(const std::string &text) {
  constexpr size_t kMax = 300;
  return text.size() <= kMax ? text : text.substr(0, kMax) + " …";
}

nlohmann::json parse_json(const std::string &body, const std::string &what) {
  nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
  WXMD_ASSERT(!parsed.is_discarded(),
              what + " 不是合法 JSON，原文开头：" + head_of(body));
  return parsed;
}

// general_msg_list 本身是一段 JSON 字符串。上游 TS 把它标注成数组，但那处调用
// 是没跑通的调试代码；真实抓包里常见的是 {"list": [...]}。两种都接受，
// 都不是就在这里失败，不去猜第三种。
const nlohmann::json &msg_list_of(const nlohmann::json &parsed) {
  if (parsed.is_array()) {
    return parsed;
  }
  WXMD_ASSERT(parsed.is_object() && parsed.contains("list") &&
                  parsed["list"].is_array(),
              "general_msg_list 结构无法识别：" + head_of(parsed.dump()));
  return parsed["list"];
}

// 部分 content_url 是以 / 开头的站内路径，补全成绝对链接。
std::string absolute_link(std::string link) {
  if (link.rfind("//", 0) == 0) {
    return "https:" + link;
  }
  if (link.rfind('/', 0) == 0) {
    return "https://mp.weixin.qq.com" + link;
  }
  return link;
}

// 文章身份：mid + idx。这两个值直接就是「第几次群发的第几篇」，跨轮次不变；
// sn 只是兜底，正常链接不会走到那一支。
std::string article_id(const std::string &link) {
  const std::string mid = str::query_param(link, "mid");
  const std::string idx = str::query_param(link, "idx");
  if (!mid.empty() && !idx.empty()) {
    return mid + "_" + idx;
  }

  const std::string sn = str::query_param(link, "sn");
  WXMD_ASSERT(!sn.empty(),
              "链接里既没有 mid+idx 也没有 sn，认不出这是哪篇文章: " + link);
  return "sn" + sn;
}

void collect_item(const nlohmann::json &item, int64_t datetime,
                  std::vector<Entry> &out) {
  const std::string link =
      str::unescape_html(item.value("content_url", std::string()));

  // 已删除的文章、以及纯文本/分享类消息没有正文链接，不属于可导出的文章。
  if (link.empty()) {
    return;
  }

  Entry entry;
  entry.id = article_id(link);
  entry.datetime = datetime;
  entry.title = str::unescape_html(item.value("title", std::string()));
  entry.link = absolute_link(link);
  out.push_back(std::move(entry));
}

std::string request(const Account &account, int offset, int count) {
  WXMD_ASSERT(!account.biz.empty(), "凭证缺少 biz");
  WXMD_ASSERT(account.has_credential(),
              "凭证不全（uin / key / pass_ticket 缺一不可）: " + account.biz);
  WXMD_ASSERT(offset >= 0, "offset 不能为负");
  WXMD_ASSERT(count > 0, "count 必须为正");

  const httplib::Params params = {
      {"action", "getmsg"},
      {"__biz", account.biz},
      {"offset", std::to_string(offset)},
      {"count", std::to_string(count)},
      {"uin", account.uin},
      {"key", account.key},
      {"pass_ticket", account.pass_ticket},
      {"f", "json"},
      {"is_ok", "1"},
      {"scene", "124"},
  };

  return fetch_raw(httplib::append_query_params(kEndpoint, params),
                   account.cookie);
}

Page fetch_page(const Account &account, int offset) {
  const std::string body = request(account, offset, kPageSize);
  const nlohmann::json root = parse_json(body, "profile_ext 响应");

  const int ret = root.value("ret", -1);
  WXMD_ASSERT(ret == 0, "profile_ext 返回失败 ret=" + std::to_string(ret) +
                            " errmsg=" + root.value("errmsg", std::string()) +
                            "（key / pass_ticket 多半已过期，需要重新抓包）");

  Page page;
  page.can_continue = root.value("can_msg_continue", 0) != 0;
  page.next_offset = root.value("next_offset", 0);

  const std::string list_text = root.value("general_msg_list", std::string());
  WXMD_ASSERT(!list_text.empty(), "响应里没有 general_msg_list");

  const nlohmann::json parsed = parse_json(list_text, "general_msg_list");
  for (const nlohmann::json &msg : msg_list_of(parsed)) {
    if (!msg.contains("app_msg_ext_info")) {
      continue; // 非图文消息
    }
    const nlohmann::json &info = msg["app_msg_ext_info"];
    const int64_t datetime =
        msg.contains("comm_msg_info")
            ? msg["comm_msg_info"].value("datetime", int64_t{0})
            : 0;

    collect_item(info, datetime, page.entries);

    // 一次群发可以带多篇文章，第一篇在 app_msg_ext_info 上，其余在这个列表里。
    if (info.value("is_multi", 0) != 0 &&
        info.contains("multi_app_msg_item_list")) {
      for (const nlohmann::json &sub : info["multi_app_msg_item_list"]) {
        collect_item(sub, datetime, page.entries);
      }
    }
  }

  return page;
}

} // namespace

bool probe_credential(const Account &account) {
  const std::string body = request(account, 0, 1);
  const nlohmann::json root = nlohmann::json::parse(body, nullptr, false);
  return !root.is_discarded() && root.value("ret", -1) == 0;
}

std::vector<Entry>
fetch_new_entries(const Account &account, int64_t since,
                  const std::function<bool(const std::string &id)> &known) {
  std::vector<Entry> out;

  // 按文章 id 去重：多图文群发里第一篇同时出现在 app_msg_ext_info 和
  // multi_app_msg_item_list，不去重会把每组首篇数两遍；微信偶尔也会在翻页里
  // 重复吐同一批文章。
  std::unordered_set<std::string> seen;

  for (int cursor = 0;;) {
    Page page = fetch_page(account, cursor);

    bool crossed = false; // 本页出现了早于水位线的文章
    size_t filtered = 0;  // 被水位线或「已入库」挡掉的篇数
    size_t fresh = 0;

    for (Entry &entry : page.entries) {
      if (since > 0 && entry.datetime < since) {
        crossed = true;
        ++filtered;
        continue;
      }
      if (known && known(entry.id)) {
        ++filtered;
        continue;
      }
      if (seen.insert(entry.id).second) {
        out.push_back(std::move(entry));
        ++fresh;
      }
    }
    std::fprintf(stderr, "\r  发现 %zu 篇新文章（offset %d）  ", out.size(),
                 cursor);

    // 撞上水位线：倒序意味着后面只会更早，没有再翻的必要。
    if (crossed) {
      break;
    }
    // 这页确实吐了文章、既没被水位线挡掉、又全是本轮见过的
    // → 微信在原地打转，别陪它死循环。
    if (!page.entries.empty() && fresh == 0 && filtered == 0) {
      warn("本页 " + std::to_string(page.entries.size()) +
           " 篇全是重复文章，判定已到历史末尾，停止翻页。");
      break;
    }
    if (!page.can_continue) {
      break;
    }
    WXMD_ASSERT(page.next_offset > cursor,
                "next_offset 没有前进（" + std::to_string(cursor) + " → " +
                    std::to_string(page.next_offset) + "），翻页会死循环");
    cursor = page.next_offset;
    std::this_thread::sleep_for(std::chrono::milliseconds(kIntervalMs));
  }

  std::fprintf(stderr, "\r%40s\r", "");
  return out;
}

std::string fetch_account_name(const std::string &article_url) {
  const std::string html = fetch_raw(article_url);

  // 从 from 起，取第一个引号（单/双）包起来的字符串并做 HTML
  // 反转义；没有返回空。
  const auto quoted_after = [&html](size_t from) -> std::string {
    size_t quote_begin = std::string::npos;
    char quote = '\0';
    for (size_t i = from; i < html.size() && i < from + 64; ++i) {
      if (html[i] == '"' || html[i] == '\'') {
        quote_begin = i;
        quote = html[i];
        break;
      }
    }
    if (quote_begin == std::string::npos) {
      return {};
    }
    const size_t quote_end = html.find(quote, quote_begin + 1);
    if (quote_end == std::string::npos) {
      return {};
    }
    return str::unescape_html(
        html.substr(quote_begin + 1, quote_end - quote_begin - 1));
  };

  // 老模板：var nickname = "名字" / var nickname = htmlDecode("名字")。
  const size_t at = html.find("var nickname");
  if (at != std::string::npos) {
    const size_t equal = html.find('=', at);
    if (equal != std::string::npos) {
      const std::string name = quoted_after(equal + 1);
      if (!name.empty()) {
        return name;
      }
    }
  }

  // 新模板：名字在 JS 对象里的 nick_name: '名字'。页面里 nick_name 出现多次
  // （含空串与纯代码引用），只认后面紧跟冒号的，取第一个非空值。
  for (size_t pos = 0; (pos = html.find("nick_name", pos)) != std::string::npos;
       pos += 9) {
    size_t colon = pos + 9; // strlen("nick_name")
    while (colon < html.size() && (html[colon] == ' ' || html[colon] == '\t')) {
      ++colon;
    }
    if (colon < html.size() && html[colon] == ':') {
      const std::string name = quoted_after(colon + 1);
      if (!name.empty()) {
        return name;
      }
    }
  }

  warn("文章页里没解析到公众号名字（var nickname 与 nick_name 都没命中），"
       "微信页面结构可能又变了：" +
       article_url);
  return {};
}

} // namespace wxmd
