#include "wxmd/profile.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
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

void collect_item(const nlohmann::json &item, int64_t datetime,
                  std::vector<ProfileEntry> &out) {
  const std::string link =
      str::unescape_html(item.value("content_url", std::string()));

  // 已删除的文章、以及纯文本/分享类消息没有正文链接，不属于可导出的文章。
  if (link.empty()) {
    return;
  }

  ProfileEntry entry;
  entry.title = str::unescape_html(item.value("title", std::string()));
  entry.author = str::unescape_html(item.value("author", std::string()));
  entry.digest = str::unescape_html(item.value("digest", std::string()));
  entry.link = absolute_link(link);
  entry.datetime = datetime;
  entry.item_show_type = item.value("item_show_type", 0);
  out.push_back(std::move(entry));
}

} // namespace

std::string fetch_profile_raw(const Credential &cred, int offset, int count) {
  WXMD_ASSERT(!cred.biz.empty(), "凭证缺少 biz");
  WXMD_ASSERT(!cred.uin.empty(), "凭证缺少 uin");
  WXMD_ASSERT(!cred.key.empty(), "凭证缺少 key");
  WXMD_ASSERT(!cred.pass_ticket.empty(), "凭证缺少 pass_ticket");
  WXMD_ASSERT(offset >= 0, "offset 不能为负");
  WXMD_ASSERT(count > 0, "count 必须为正");

  const httplib::Params params = {
      {"action", "getmsg"},
      {"__biz", cred.biz},
      {"offset", std::to_string(offset)},
      {"count", std::to_string(count)},
      {"uin", cred.uin},
      {"key", cred.key},
      {"pass_ticket", cred.pass_ticket},
      {"f", "json"},
      {"is_ok", "1"},
      {"scene", "124"},
  };

  return fetch_raw(httplib::append_query_params(kEndpoint, params),
                   cred.cookie);
}

ProfilePage fetch_profile_page(const Credential &cred, int offset, int count) {
  const std::string body = fetch_profile_raw(cred, offset, count);
  const nlohmann::json root = parse_json(body, "profile_ext 响应");

  const int ret = root.value("ret", -1);
  WXMD_ASSERT(ret == 0, "profile_ext 返回失败 ret=" + std::to_string(ret) +
                            " errmsg=" + root.value("errmsg", std::string()) +
                            "（key / pass_ticket 多半已过期，需要重新抓包）");

  ProfilePage page;
  page.can_continue = root.value("can_msg_continue", 0) != 0;
  page.next_offset = root.value("next_offset", 0);
  page.msg_count = root.value("msg_count", 0);

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

ProfileList fetch_profile_list(const Credential &cred, int offset, int count,
                               int limit, int interval_ms,
                               const PageCallback &on_page) {
  WXMD_ASSERT(interval_ms >= 0, "interval 不能为负");

  ProfileList result;
  result.next_offset = offset;
  int cursor = offset;

  // 按链接去重：多图文群发里第一篇同时出现在 app_msg_ext_info 和
  // multi_app_msg_item_list，不去重会把每组首篇数两遍；微信偶尔也会在翻页里
  // 重复吐同一批文章。去重后计数才是真实的「不同文章数」。
  std::unordered_set<std::string> seen;

  for (bool first = true;; first = false) {
    if (!first && interval_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    ProfilePage page = fetch_profile_page(cred, cursor, count);
    const size_t raw = page.entries.size();

    std::vector<ProfileEntry> fresh;
    for (ProfileEntry &entry : page.entries) {
      if (seen.insert(entry.link).second) {
        fresh.push_back(std::move(entry));
      }
    }
    page.entries = std::move(fresh);

    result.entries.insert(result.entries.end(), page.entries.begin(),
                          page.entries.end());
    result.next_offset = page.next_offset;
    result.can_continue = page.can_continue;

    if (on_page) {
      on_page(page, cursor);
    }

    // 这页确实吐了文章、但全是见过的 → 微信在原地打转，别陪它死循环。
    if (raw > 0 && page.entries.empty()) {
      warn("本页 " + std::to_string(raw) +
           " 篇全是重复文章，判定已到历史末尾，停止翻页。");
      break;
    }
    if (!page.can_continue) {
      break;
    }
    if (limit > 0 && static_cast<int>(result.entries.size()) >= limit) {
      break;
    }
    WXMD_ASSERT(page.next_offset > cursor,
                "next_offset 没有前进（" + std::to_string(cursor) + " → " +
                    std::to_string(page.next_offset) + "），翻页会死循环");
    cursor = page.next_offset;
  }

  if (limit > 0 && static_cast<int>(result.entries.size()) > limit) {
    result.entries.resize(limit);
  }
  return result;
}

std::string fetch_account_name(const std::string &article_url) {
  const std::string html = fetch_raw(article_url, std::string());

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

Credential load_credential(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  WXMD_ASSERT(input.is_open(), "无法打开凭证文件: " + path);

  std::ostringstream buffer;
  buffer << input.rdbuf();

  const nlohmann::json root = parse_json(buffer.str(), "凭证文件 " + path);
  WXMD_ASSERT(root.is_object(), "凭证文件应当是一个 JSON 对象: " + path);

  Credential cred;
  cred.biz = root.value("biz", std::string());
  cred.uin = root.value("uin", std::string());
  cred.key = root.value("key", std::string());
  cred.pass_ticket = root.value("pass_ticket", std::string());
  cred.cookie = root.value("cookie", std::string());
  return cred;
}

} // namespace wxmd
