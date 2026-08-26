#include "wxmd/html.hpp"

#include <vector>

#include "wxmd/assert.hpp"
#include "wxmd/dom.hpp"

namespace wxmd {
namespace {

// 折叠空白并去掉首尾空格，对齐原实现里的 .trim().replace(/\n+/g,'').replace(/
// +/g,' ')
std::string squash(const std::string &in) {
  std::string out;
  bool space = false;
  for (char c : in) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      space = true;
      continue;
    }
    if (space && !out.empty()) {
      out += ' ';
    }
    space = false;
    out += c;
  }
  return out;
}

// cgi 数据所在的脚本带 h5only 标记，内容里定义 window.cgiDataNew。
std::string cgi_script_of(const dom::Document &doc) {
  for (lxb_dom_node_t *script : doc.query("script")) {
    if (!dom::has_attr(script, "h5only")) {
      continue;
    }
    std::string code = dom::inner_html(script);
    if (code.find("window.cgiDataNew = {") != std::string::npos) {
      return code;
    }
  }
  WXMD_ASSERT(false, "未找到包含 cgiDataNew 的 script 脚本");
}

} // namespace

std::string status_text(ArticleStatus status) {
  switch (status) {
  case ArticleStatus::Success:
    return "Success";
  case ArticleStatus::Deleted:
    return "Deleted";
  case ArticleStatus::Exception:
    return "Exception";
  case ArticleStatus::Error:
    return "Error";
  }
  return "Unknown";
}

ArticlePage parse_article(const std::string &html) {
  WXMD_ASSERT(!html.empty(), "文章 HTML 为空");

  dom::Document doc(html);

  if (doc.query("#js_article").size() == 1) {
    return {ArticleStatus::Success, {}, cgi_script_of(doc)};
  }

  std::vector<lxb_dom_node_t *> weui_msg = doc.query(".weui-msg");
  if (weui_msg.size() == 1) {
    lxb_dom_node_t *title = doc.query_first(".weui-msg .weui-msg__title");
    std::string msg =
        title == nullptr ? std::string() : squash(dom::text_content(title));

    if (msg == "The content has been deleted by the author." ||
        msg == "该内容已被发布者删除") {
      return {ArticleStatus::Deleted, {}, {}};
    }
    return {ArticleStatus::Exception, msg, {}};
  }

  std::vector<lxb_dom_node_t *> msg_block = doc.query(".mesg-block");
  if (msg_block.size() == 1) {
    return {ArticleStatus::Exception,
            squash(dom::text_content(msg_block.front())),
            {}};
  }

  return {ArticleStatus::Error, {}, {}};
}

} // namespace wxmd
