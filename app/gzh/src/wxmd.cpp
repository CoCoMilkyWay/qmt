#include "wxmd/wxmd.hpp"

#include "wxmd/cgi.hpp"
#include "wxmd/markdown.hpp"
#include "wxmd/renderer.hpp"

namespace wxmd {

std::string render_article_markdown(const std::string &cgi_script,
                                    const AssetHook &on_asset) {
  const std::string html = render_html(eval_cgi(cgi_script));
  return to_markdown(localize_images(html, on_asset));
}

} // namespace wxmd
