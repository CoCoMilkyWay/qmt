#include "wxmd/wxmd.hpp"

#include "wxmd/assert.hpp"

namespace wxmd {
namespace {

std::string field(const nlohmann::json &obj, const std::string &key) {
  if (!obj.is_object() || !obj.contains(key) || !obj[key].is_string()) {
    return {};
  }
  return obj[key].get<std::string>();
}

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

nlohmann::json load_cgi(const std::string &raw_html) {
  WXMD_ASSERT(!raw_html.empty(), "文章 HTML 为空");

  const ValidateResult validated = validate_html(raw_html);
  WXMD_ASSERT(validated.status == ArticleStatus::Success,
              "文章不可用 [" + status_text(validated.status) + "] " +
                  validated.message);

  return eval_cgi(extract_cgi_script(raw_html));
}

} // namespace

std::string render_article_html(const std::string &raw_html) {
  return render_html(load_cgi(raw_html));
}

Article parse_article(const std::string &raw_html) {
  const nlohmann::json cgi = load_cgi(raw_html);

  Article article;
  article.title = extract_title(cgi);
  article.author = field(cgi, "author");
  article.account = field(cgi, "nick_name");
  article.publish_time = field(cgi, "create_time");
  article.link = extract_link(cgi);
  article.markdown = to_markdown(render_html(cgi));
  return article;
}

Article fetch_and_parse(const std::string &url) {
  return parse_article(fetch_article(url));
}

} // namespace wxmd
