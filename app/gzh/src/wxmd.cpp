#include "wxmd/wxmd.hpp"

#include "wxmd/assert.hpp"
#include "wxmd/cgi.hpp"
#include "wxmd/html.hpp"
#include "wxmd/markdown.hpp"
#include "wxmd/renderer.hpp"

namespace wxmd {
namespace {

// 前三级：抓下来的 html → 状态判定 → cgiDataNew。
// 不可用的文章（已删除 / 违规 / 风控）在这里带原因终止——同步流程要的是墓碑，
// 所以它在调用本函数之前就自己 validate_html 过一遍了。
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

std::string render_article_markdown(const std::string &raw_html,
                                    const AssetHook &on_asset) {
  std::string html = render_html(load_cgi(raw_html));
  if (on_asset) {
    html = localize_images(html, on_asset);
  }
  return to_markdown(html);
}

} // namespace wxmd
