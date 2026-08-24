#include "wxmd/assets.hpp"

#include <unordered_map>

#include "wxmd/assert.hpp"
#include "wxmd/dom.hpp"

namespace wxmd {

std::string localize_images(const std::string &html,
                            const AssetHook &on_asset) {
  WXMD_ASSERT(static_cast<bool>(on_asset), "localize_images 收到空钩子");

  dom::Document doc(html);

  // 同一张图在正文里可能出现多次，按地址缓存，避免重复下载。
  std::unordered_map<std::string, std::string> localized;

  for (lxb_dom_node_t *img : doc.query("img[src]")) {
    const std::string src = dom::attr(img, "src");
    // data: 内联图、以及别的非网络地址没得可下，原样留着。
    if (src.rfind("http://", 0) != 0 && src.rfind("https://", 0) != 0 &&
        src.rfind("//", 0) != 0) {
      continue;
    }

    auto found = localized.find(src);
    if (found == localized.end()) {
      found = localized.emplace(src, on_asset(src)).first;
    }
    if (!found->second.empty()) {
      dom::set_attr(img, "src", found->second);
    }
  }

  // 文档节点的「子节点」就是 doctype + <html>，等价于整份文档。
  return dom::inner_html(doc.root());
}

} // namespace wxmd
