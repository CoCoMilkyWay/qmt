#include "wxmd/renderer.hpp"

#include <regex>

#include "strutil.hpp"
#include "wxmd/assert.hpp"
#include "wxmd/dom.hpp"

namespace wxmd {
namespace {

// 与上游 renderer.ts 的 ITEM_SHOW_TYPE 保持一致
constexpr int kShowTypeNormal = 0; // 普通图文
constexpr int kShowTypeImage = 8;  // 图片分享
constexpr int kShowTypeText = 10;  // 文本分享

// cgiDataNew 里字符串字段可能缺失，统一按空串处理。
std::string field(const nlohmann::json &obj, const std::string &key) {
  if (!obj.is_object() || !obj.contains(key) || !obj[key].is_string()) {
    return {};
  }
  return obj[key].get<std::string>();
}

int int_field(const nlohmann::json &obj, const std::string &key) {
  if (!obj.is_object() || !obj.contains(key)) {
    return 0;
  }
  const nlohmann::json &value = obj[key];
  if (value.is_number()) {
    return value.get<int>();
  }
  if (value.is_string()) {
    const std::string text = value.get<std::string>();
    return text.empty() ? 0 : std::stoi(text);
  }
  return 0;
}

nlohmann::json sub_object(const nlohmann::json &obj, const std::string &key) {
  if (!obj.is_object() || !obj.contains(key) || !obj[key].is_object()) {
    return nlohmann::json::object();
  }
  return obj[key];
}

// 未购买的付费文章，content_noencode 里只有占位元素。
bool is_pay_preview_placeholder(const std::string &content) {
  const std::string trimmed = str::trim(content);
  return trimmed.empty() ||
         trimmed.find("mp-pay-preview-filter") != std::string::npos;
}

std::string render_pay_subscribe(const nlohmann::json &cgi) {
  const nlohmann::json pay_info = sub_object(cgi, "pay_subscribe_info");

  std::string desc = field(pay_info, "desc");
  str::replace_all(desc, "\r\n", "<br />");
  str::replace_all(desc, "\n", "<br />");

  const int fee = int_field(pay_info, "fee");
  const int wecoin = int_field(pay_info, "wecoin_amount");

  std::string price = "付费";
  if (fee != 0) {
    price = std::to_string(fee / 100) + " 元";
  } else if (wecoin != 0) {
    price = std::to_string(wecoin) + " 微币";
  }

  std::string out = "<section class=\"pay_subscribe_notice\">\n";
  out += "<div class=\"pay_subscribe_badge\">付费内容 · " + price + "</div>\n";
  if (!desc.empty()) {
    out += "<div class=\"pay_subscribe_desc\">" + desc + "</div>\n";
  }
  out += "<div "
         "class=\"pay_subscribe_hint\">本文为付费文章，完整内容需购买后查看</"
         "div>\n";
  out += "</section>";
  return out;
}

// 图片分享正文里的「图N」锚点缺少 href，补上以便跳转到对应图片。
std::string link_image_refs(const std::string &html) {
  static const std::regex pattern(
      R"RX((<a class="wx_img_refer_link" data-seq="(\d+)" data-refer="图\2" style="[^"]*">)(\s*图\2\s*)(</a>))RX");

  std::string out;
  auto begin = std::sregex_iterator(html.begin(), html.end(), pattern);
  auto end = std::sregex_iterator();

  size_t last = 0;
  for (auto it = begin; it != end; ++it) {
    const std::smatch &match = *it;
    out.append(html, last, static_cast<size_t>(match.position()) - last);

    std::string open_tag = match[1].str();
    const std::string seq = match[2].str();
    WXMD_ASSERT(!open_tag.empty() && open_tag.back() == '>',
                "图片锚点开标签格式异常");
    open_tag.pop_back();
    open_tag += " href=\"#图" + seq + "\">";

    out += open_tag + match[3].str() + match[4].str();
    last = static_cast<size_t>(match.position()) +
           static_cast<size_t>(match.length());
  }
  out.append(html, last, std::string::npos);
  return out;
}

// 图片分享(8)
std::string render_content_image(const nlohmann::json &cgi) {
  std::string text_content = field(cgi, "content_noencode");
  str::replace_all(text_content, "\n", "<br />");
  text_content = link_image_refs(text_content);

  std::string pictures;
  if (cgi.contains("picture_page_info_list") &&
      cgi["picture_page_info_list"].is_array()) {
    int index = 0;
    for (const nlohmann::json &item : cgi["picture_page_info_list"]) {
      ++index;
      const std::string url =
          str::replaced(field(item, "cdn_url"), "&amp;", "&");
      const std::string label = "图" + std::to_string(index);

      if (!pictures.empty()) {
        pictures += "\n";
      }
      pictures += "<div class=\"picture_item\" id=\"" + label + "\">\n";
      pictures += "    <img class=\"picture_item_img\" src=\"" + url +
                  "\" alt=\"" + label + "\" />\n";
      pictures += "    <p class=\"picture_item_label\">" + label + "</p>\n";
      pictures += "</div>";
    }
  }

  return "<section class=\"item_show_type_8\">\n<p class=\"text_content\">" +
         text_content + "</p>\n<div class=\"picture_content\">" + pictures +
         "</div>\n</section>";
}

// 文本分享(10)
std::string render_content_text(const nlohmann::json &cgi) {
  std::string text_content =
      field(sub_object(cgi, "text_page_info"), "content_noencode");
  str::replace_all(text_content, "\n", "<br />");

  return "<section class=\"item_show_type_10\">\n<p class=\"text_content\">" +
         text_content + "</p>\n</section>";
}

// 普通图文(0)
std::string render_content_normal(const nlohmann::json &cgi) {
  const std::string content = field(cgi, "content_noencode");
  const std::string title = field(cgi, "title");

  // 正文实质为空时退化为展示标题，避免导出空文件
  {
    dom::Document probe(content);
    if (str::strip_whitespace(dom::text_content(probe.body())).empty() &&
        !title.empty()) {
      std::string text = str::escape_html(title);
      str::replace_all(text, "\n", "<br />");
      return "<section class=\"item_show_type_0\"><p class=\"text_content\">" +
             text + "</p></section>";
    }
  }

  dom::Document doc(content);

  // 懒加载图片：data-src → src
  for (lxb_dom_node_t *img : doc.query("img[data-src]")) {
    const std::string data_src = dom::attr(img, "data-src");
    if (data_src.empty()) {
      continue;
    }
    dom::set_attr(img, "src", data_src);
    dom::remove_attr(img, "data-src");
    dom::set_attr(img, "loading", "eager");
  }

  // 去掉 height，交给样式按原始比例缩放
  for (lxb_dom_node_t *img : doc.query("img[height]")) {
    dom::remove_attr(img, "height");
  }

  return "<section class=\"item_show_type_0\">" + dom::inner_html(doc.body()) +
         "</section>";
}

std::string render_meta(const nlohmann::json &cgi) {
  const std::string province =
      field(sub_object(cgi, "ip_wording"), "province_name");

  std::string out = "<div class=\"__meta__\">\n";
  out += "    <span class=\"author\">" +
         str::escape_html(field(cgi, "author")) + "</span>\n";
  out += "    <span class=\"nick_name\">" +
         str::escape_html(field(cgi, "nick_name")) + "</span>\n";
  out += "    <span class=\"create_time\">" +
         str::escape_html(field(cgi, "create_time")) + "</span>\n";
  out += "    <span class=\"ip\">" + str::escape_html(province) + "</span>\n";
  out += "</div>";
  return out;
}

std::string render_content(const nlohmann::json &cgi) {
  // 未购买的付费文章优先展示付费提示
  if (int_field(cgi, "is_pay_subscribe") == 1 &&
      is_pay_preview_placeholder(field(cgi, "content_noencode"))) {
    return render_pay_subscribe(cgi);
  }

  const int show_type = int_field(cgi, "item_show_type");
  switch (show_type) {
  case kShowTypeImage:
    return render_content_image(cgi);
  case kShowTypeText:
    return render_content_text(cgi);
  case kShowTypeNormal:
    return render_content_normal(cgi);
  default:
    // 未知类型降级为占位：微信会新增消息类型（如 item_show_type=5），
    // 断言会把整条流水线卡死在某一篇，降级成占位让同步继续，标题与
    // 原文链接仍由外层渲染进正文，至少能定位是哪篇。
    warn("未知的 item_show_type: " + std::to_string(show_type) +
         "，降级为占位");
    return "<section class=\"item_show_type_" + std::to_string(show_type) +
           "\"><p>（未支持的消息类型 item_show_type=" +
           std::to_string(show_type) + "，见原文链接）</p></section>";
  }
}

// 标题：图文与图片分享直接用 title，文本分享没有标题就从正文截一段。
std::string extract_title(const nlohmann::json &cgi) {
  const int show_type = int_field(cgi, "item_show_type");

  if (show_type == kShowTypeImage || show_type == kShowTypeNormal) {
    return field(cgi, "title");
  }

  if (show_type == kShowTypeText) {
    const nlohmann::json text_page_info = sub_object(cgi, "text_page_info");
    if (int_field(text_page_info, "is_user_title") == 1) {
      return field(cgi, "title");
    }

    std::string content = field(text_page_info, "content_noencode");
    if (content.empty()) {
      content = field(cgi, "title");
    }
    str::replace_all(content, "\n", " ");

    // 取前 20 个字符作为标题，按 UTF-8 码点截断避免切碎多字节字符
    size_t bytes = 0;
    int chars = 0;
    while (bytes < content.size() && chars < 20) {
      const unsigned char c = static_cast<unsigned char>(content[bytes]);
      size_t width = 1;
      if ((c & 0xF8) == 0xF0) {
        width = 4;
      } else if ((c & 0xF0) == 0xE0) {
        width = 3;
      } else if ((c & 0xE0) == 0xC0) {
        width = 2;
      }
      bytes += width;
      ++chars;
    }
    std::string title = content.substr(0, std::min(bytes, content.size()));
    return title.empty() ? "(无标题)" : title;
  }

  // 未知类型：退回 title 字段，拿不到就空，由外层兜底。
  return field(cgi, "title");
}

// cgi 里的 link 带 &amp; 实体，还原为 &。
std::string extract_link(const nlohmann::json &cgi) {
  return str::replaced(field(cgi, "link"), "&amp;", "&");
}

} // namespace

std::string render_html(const nlohmann::json &cgi) {
  WXMD_ASSERT(cgi.is_object(), "cgiDataNew 不是对象");

  const std::string title = extract_title(cgi);
  const std::string link = extract_link(cgi);

  std::string out = "<!DOCTYPE html>\n<html lang=\"zh_CN\">\n<head>\n";
  out += "    <meta charset=\"utf-8\">\n";
  out += "    <title>" + str::escape_html(title) + "</title>\n";
  out += "</head>\n<body>\n<div class=\"__page_content__\">\n";
  out += "<h1 class=\"title\">" + str::escape_html(title) + "</h1>\n";
  out += render_meta(cgi) + "\n";
  if (!link.empty()) {
    out += "<blockquote class=\"source\">原文地址: <a href=\"" +
           str::escape_html(link) + "\">" + str::escape_html(link) +
           "</a></blockquote>\n";
  }
  out += render_content(cgi) + "\n";
  out += "</div>\n</body>\n</html>";
  return out;
}

} // namespace wxmd
