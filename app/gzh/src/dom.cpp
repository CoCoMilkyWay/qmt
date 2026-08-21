#include "wxmd/dom.hpp"

#include <lexbor/css/css.h>
#include <lexbor/html/html.h>
#include <lexbor/selectors/selectors.h>

#include <algorithm>

#include "wxmd/assert.hpp"

namespace wxmd::dom {
namespace {

lxb_status_t collect_str_cb(const lxb_char_t *data, size_t len, void *ctx) {
  static_cast<std::string *>(ctx)->append(reinterpret_cast<const char *>(data),
                                          len);
  return LXB_STATUS_OK;
}

lxb_status_t collect_node_cb(lxb_dom_node_t *node,
                             lxb_css_selector_specificity_t, void *ctx) {
  static_cast<std::vector<lxb_dom_node_t *> *>(ctx)->push_back(node);
  return LXB_STATUS_OK;
}

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return s;
}

} // namespace

Document::Document(const std::string &html) {
  doc_ = lxb_html_document_create();
  WXMD_ASSERT(doc_ != nullptr, "lxb_html_document_create 失败");

  lxb_status_t status = lxb_html_document_parse(
      doc_, reinterpret_cast<const lxb_char_t *>(html.data()), html.size());
  WXMD_ASSERT(status == LXB_STATUS_OK, "HTML 解析失败");
}

Document::~Document() {
  if (doc_ != nullptr) {
    lxb_html_document_destroy(doc_);
  }
}

lxb_dom_node_t *Document::root() const { return lxb_dom_interface_node(doc_); }

lxb_dom_node_t *Document::body() const {
  lxb_html_body_element_t *body = lxb_html_document_body_element(doc_);
  WXMD_ASSERT(body != nullptr, "文档缺少 body 元素");
  return lxb_dom_interface_node(body);
}

std::vector<lxb_dom_node_t *>
Document::query(const std::string &selector) const {
  std::vector<lxb_dom_node_t *> found;

  lxb_css_parser_t *parser = lxb_css_parser_create();
  WXMD_ASSERT(parser != nullptr, "lxb_css_parser_create 失败");
  WXMD_ASSERT(lxb_css_parser_init(parser, nullptr) == LXB_STATUS_OK,
              "lxb_css_parser_init 失败");

  lxb_selectors_t *selectors = lxb_selectors_create();
  WXMD_ASSERT(selectors != nullptr, "lxb_selectors_create 失败");
  WXMD_ASSERT(lxb_selectors_init(selectors) == LXB_STATUS_OK,
              "lxb_selectors_init 失败");

  lxb_css_selector_list_t *list = lxb_css_selectors_parse(
      parser, reinterpret_cast<const lxb_char_t *>(selector.data()),
      selector.size());
  WXMD_ASSERT(parser->status == LXB_STATUS_OK,
              "CSS 选择器解析失败: " + selector);

  lxb_status_t status =
      lxb_selectors_find(selectors, root(), list, collect_node_cb, &found);
  WXMD_ASSERT(status == LXB_STATUS_OK, "选择器查询失败: " + selector);

  lxb_selectors_destroy(selectors, true);
  lxb_css_parser_destroy(parser, true);
  lxb_css_selector_list_destroy_memory(list);

  return found;
}

lxb_dom_node_t *Document::query_first(const std::string &selector) const {
  std::vector<lxb_dom_node_t *> found = query(selector);
  return found.empty() ? nullptr : found.front();
}

std::string text_content(lxb_dom_node_t *node) {
  WXMD_ASSERT(node != nullptr, "text_content 收到空节点");

  size_t len = 0;
  lxb_char_t *text = lxb_dom_node_text_content(node, &len);
  if (text == nullptr) {
    return {};
  }

  std::string out(reinterpret_cast<const char *>(text), len);
  lxb_dom_document_destroy_text(node->owner_document, text);
  return out;
}

std::string outer_html(lxb_dom_node_t *node) {
  WXMD_ASSERT(node != nullptr, "outer_html 收到空节点");
  std::string out;
  WXMD_ASSERT(lxb_html_serialize_tree_cb(node, collect_str_cb, &out) ==
                  LXB_STATUS_OK,
              "序列化失败");
  return out;
}

std::string inner_html(lxb_dom_node_t *node) {
  WXMD_ASSERT(node != nullptr, "inner_html 收到空节点");
  std::string out;
  WXMD_ASSERT(lxb_html_serialize_deep_cb(node, collect_str_cb, &out) ==
                  LXB_STATUS_OK,
              "序列化失败");
  return out;
}

std::string attr(lxb_dom_node_t *node, const std::string &name) {
  WXMD_ASSERT(node != nullptr, "attr 收到空节点");
  if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return {};
  }

  size_t len = 0;
  const lxb_char_t *value = lxb_dom_element_get_attribute(
      lxb_dom_interface_element(node),
      reinterpret_cast<const lxb_char_t *>(name.data()), name.size(), &len);
  if (value == nullptr) {
    return {};
  }
  return std::string(reinterpret_cast<const char *>(value), len);
}

bool has_attr(lxb_dom_node_t *node, const std::string &name) {
  WXMD_ASSERT(node != nullptr, "has_attr 收到空节点");
  if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return false;
  }
  return lxb_dom_element_has_attribute(
      lxb_dom_interface_element(node),
      reinterpret_cast<const lxb_char_t *>(name.data()), name.size());
}

void set_attr(lxb_dom_node_t *node, const std::string &name,
              const std::string &value) {
  WXMD_ASSERT(node != nullptr, "set_attr 收到空节点");
  WXMD_ASSERT(node->type == LXB_DOM_NODE_TYPE_ELEMENT,
              "set_attr 只能作用于元素节点");

  lxb_dom_attr_t *attribute = lxb_dom_element_set_attribute(
      lxb_dom_interface_element(node),
      reinterpret_cast<const lxb_char_t *>(name.data()), name.size(),
      reinterpret_cast<const lxb_char_t *>(value.data()), value.size());
  WXMD_ASSERT(attribute != nullptr, "设置属性失败: " + name);
}

void remove_attr(lxb_dom_node_t *node, const std::string &name) {
  WXMD_ASSERT(node != nullptr, "remove_attr 收到空节点");
  WXMD_ASSERT(node->type == LXB_DOM_NODE_TYPE_ELEMENT,
              "remove_attr 只能作用于元素节点");

  lxb_status_t status = lxb_dom_element_remove_attribute(
      lxb_dom_interface_element(node),
      reinterpret_cast<const lxb_char_t *>(name.data()), name.size());
  WXMD_ASSERT(status == LXB_STATUS_OK, "移除属性失败: " + name);
}

std::string tag_name(lxb_dom_node_t *node) {
  WXMD_ASSERT(node != nullptr, "tag_name 收到空节点");
  if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) {
    return {};
  }

  size_t len = 0;
  const lxb_char_t *name =
      lxb_dom_element_local_name(lxb_dom_interface_element(node), &len);
  if (name == nullptr) {
    return {};
  }
  return upper(std::string(reinterpret_cast<const char *>(name), len));
}

// -------------------------------------------------------------- MdNode 树

std::string MdNode::attr(const std::string &key) const {
  for (const auto &kv : attrs) {
    if (kv.first == key) {
      return kv.second;
    }
  }
  return {};
}

bool MdNode::has_attr(const std::string &key) const {
  for (const auto &kv : attrs) {
    if (kv.first == key) {
      return true;
    }
  }
  return false;
}

std::string MdNode::text_content() const {
  if (type == Type::Text) {
    return text;
  }
  std::string out;
  for (const auto &child : children) {
    out += child->text_content();
  }
  return out;
}

size_t MdNode::index_in_parent() const {
  if (parent == nullptr) {
    return 0;
  }
  for (size_t i = 0; i < parent->children.size(); ++i) {
    if (parent->children[i].get() == this) {
      return i;
    }
  }
  WXMD_ASSERT(false, "节点不在其父节点的 children 中");
}

MdNode *MdNode::previous_sibling() const {
  if (parent == nullptr) {
    return nullptr;
  }
  size_t idx = index_in_parent();
  return idx == 0 ? nullptr : parent->children[idx - 1].get();
}

MdNode *MdNode::next_sibling() const {
  if (parent == nullptr) {
    return nullptr;
  }
  size_t idx = index_in_parent();
  return idx + 1 >= parent->children.size() ? nullptr
                                            : parent->children[idx + 1].get();
}

MdNode *MdNode::last_element_child() const {
  for (size_t i = children.size(); i > 0; --i) {
    if (children[i - 1]->type == Type::Element) {
      return children[i - 1].get();
    }
  }
  return nullptr;
}

namespace {

void collect_attrs(lxb_dom_node_t *node, MdNode &out) {
  lxb_dom_element_t *element = lxb_dom_interface_element(node);
  lxb_dom_attr_t *attribute = lxb_dom_element_first_attribute(element);

  while (attribute != nullptr) {
    size_t name_len = 0;
    const lxb_char_t *name = lxb_dom_attr_qualified_name(attribute, &name_len);

    size_t value_len = 0;
    const lxb_char_t *value = lxb_dom_attr_value(attribute, &value_len);

    if (name != nullptr) {
      out.attrs.emplace_back(
          std::string(reinterpret_cast<const char *>(name), name_len),
          value == nullptr
              ? std::string()
              : std::string(reinterpret_cast<const char *>(value), value_len));
    }

    attribute = lxb_dom_element_next_attribute(attribute);
  }
}

std::unique_ptr<MdNode> build_node(lxb_dom_node_t *node, MdNode *parent) {
  auto out = std::make_unique<MdNode>();
  out->parent = parent;

  if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
    out->type = MdNode::Type::Text;
    out->text = text_content(node);
    return out;
  }

  out->type = MdNode::Type::Element;
  out->name = tag_name(node);
  collect_attrs(node, *out);

  for (lxb_dom_node_t *child = node->first_child; child != nullptr;
       child = child->next) {
    if (child->type != LXB_DOM_NODE_TYPE_ELEMENT &&
        child->type != LXB_DOM_NODE_TYPE_TEXT) {
      continue;
    }
    out->children.push_back(build_node(child, out.get()));
  }

  return out;
}

} // namespace

std::unique_ptr<MdNode> build_tree(lxb_dom_node_t *node) {
  WXMD_ASSERT(node != nullptr, "build_tree 收到空节点");
  return build_node(node, nullptr);
}

void remove_if(MdNode &root, const std::function<bool(const MdNode &)> &pred) {
  for (size_t i = root.children.size(); i > 0; --i) {
    MdNode *child = root.children[i - 1].get();
    if (pred(*child)) {
      root.children.erase(root.children.begin() + static_cast<long>(i - 1));
      continue;
    }
    remove_if(*child, pred);
  }
}

} // namespace wxmd::dom
