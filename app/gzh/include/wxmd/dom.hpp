#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

// lexbor 的类型只在实现里用，这里前置声明，避免把 C 头文件泄露给上层。
struct lxb_html_document;
typedef struct lxb_html_document lxb_html_document_t;
struct lxb_dom_node;
typedef struct lxb_dom_node lxb_dom_node_t;

namespace wxmd::dom {

// 解析后的 HTML 文档，析构时释放 lexbor 资源。
class Document {
public:
  explicit Document(const std::string &html);
  ~Document();

  Document(const Document &) = delete;
  Document &operator=(const Document &) = delete;

  lxb_dom_node_t *root() const;
  lxb_dom_node_t *body() const;

  // CSS 选择器查询，返回匹配到的节点。
  std::vector<lxb_dom_node_t *> query(const std::string &selector) const;

  // 仅取第一个匹配节点，无匹配返回 nullptr。
  lxb_dom_node_t *query_first(const std::string &selector) const;

private:
  lxb_html_document_t *doc_ = nullptr;
};

// 节点的纯文本内容（等价 DOM textContent）。
std::string text_content(lxb_dom_node_t *node);

// 序列化节点自身及其子树（等价 outerHTML）。
std::string outer_html(lxb_dom_node_t *node);

// 序列化节点的子树（等价 innerHTML）。
std::string inner_html(lxb_dom_node_t *node);

// 元素属性，属性不存在时返回空字符串。
std::string attr(lxb_dom_node_t *node, const std::string &name);
bool has_attr(lxb_dom_node_t *node, const std::string &name);
void set_attr(lxb_dom_node_t *node, const std::string &name,
              const std::string &value);
void remove_attr(lxb_dom_node_t *node, const std::string &name);

// 元素标签名（大写，如 "DIV"）；非元素节点返回空字符串。
std::string tag_name(lxb_dom_node_t *node);

// -------------------------------------------------------------- MdNode 树
// 从 lexbor DOM 抽出的轻量可变树。turndown 的预处理（空白折叠、节点删除）
// 需要频繁改写文本与结构，在自有树上做比直接改 lexbor DOM 简单得多。

struct MdNode {
  enum class Type { Element, Text };

  Type type = Type::Element;
  std::string name; // 元素标签名，大写
  std::string text; // 文本节点内容
  std::vector<std::pair<std::string, std::string>> attrs;
  std::vector<std::unique_ptr<MdNode>> children;
  MdNode *parent = nullptr;

  std::string attr(const std::string &key) const;
  bool has_attr(const std::string &key) const;

  // 递归拼接所有后代文本节点。
  std::string text_content() const;

  // 在父节点 children 中的下标；无父节点返回 0。
  size_t index_in_parent() const;
  MdNode *previous_sibling() const;
  MdNode *next_sibling() const;

  // 最后一个元素类型的子节点，没有则返回 nullptr。
  MdNode *last_element_child() const;
};

// 以 node 为根构建 MdNode 树（node 本身作为根，只保留元素与文本节点）。
std::unique_ptr<MdNode> build_tree(lxb_dom_node_t *node);

// 深度优先删除满足 pred 的节点（根节点不参与判断）。
void remove_if(MdNode &root, const std::function<bool(const MdNode &)> &pred);

} // namespace wxmd::dom
