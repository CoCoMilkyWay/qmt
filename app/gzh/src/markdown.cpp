#include "wxmd/markdown.hpp"

#include <algorithm>
#include <array>
#include <string_view>

#include "strutil.hpp"
#include "wxmd/assert.hpp"
#include "wxmd/dom.hpp"

// turndown 的规则与算法在此逐条移植，命名尽量保留原名以便对照。
// 配置固定为上游 markdown.ts 所用的一组选项。

namespace wxmd {
namespace {

using dom::MdNode;

constexpr std::string_view kBulletListMarker = "-";
constexpr std::string_view kEmDelimiter = "_";
constexpr std::string_view kStrongDelimiter = "**";
constexpr std::string_view kHorizontalRule = "* * *";
constexpr std::string_view kLineBreak = "  ";
constexpr char kFenceChar = '`';
constexpr std::string_view kNbsp = "\xC2\xA0";

constexpr std::string_view kBlockElements[] = {
    "ADDRESS",  "ARTICLE",  "ASIDE",      "AUDIO",  "BLOCKQUOTE", "BODY",
    "CANVAS",   "CENTER",   "DD",         "DIR",    "DIV",        "DL",
    "DT",       "FIELDSET", "FIGCAPTION", "FIGURE", "FOOTER",     "FORM",
    "FRAMESET", "H1",       "H2",         "H3",     "H4",         "H5",
    "H6",       "HEADER",   "HGROUP",     "HR",     "HTML",       "ISINDEX",
    "LI",       "MAIN",     "MENU",       "NAV",    "NOFRAMES",   "NOSCRIPT",
    "OL",       "OUTPUT",   "P",          "PRE",    "SECTION",    "TABLE",
    "TBODY",    "TD",       "TFOOT",      "TH",     "THEAD",      "TR",
    "UL"};

constexpr std::string_view kVoidElements[] = {
    "AREA",  "BASE",   "BR",   "COL",  "COMMAND", "EMBED",  "HR",    "IMG",
    "INPUT", "KEYGEN", "LINK", "META", "PARAM",   "SOURCE", "TRACK", "WBR"};

constexpr std::string_view kMeaningfulWhenBlankElements[] = {
    "A",  "TABLE",  "THEAD",  "TBODY", "TFOOT", "TH",
    "TD", "IFRAME", "SCRIPT", "AUDIO", "VIDEO"};

// markdown.ts 里显式移除的标签：head 内元素在转换时会残留文本，必须丢弃。
constexpr std::string_view kRemovedElements[] = {"STYLE", "SCRIPT", "NOSCRIPT",
                                                 "LINK",  "META",   "TITLE"};

template <size_t N>
bool contains(const std::string_view (&list)[N], const std::string &name) {
  return std::find(std::begin(list), std::end(list), name) != std::end(list);
}

bool is_block(const std::string &name) {
  return contains(kBlockElements, name);
}

bool is_block(const MdNode *node) {
  return node->type == MdNode::Type::Element && is_block(node->name);
}

bool is_void(const MdNode *node) {
  return node->type == MdNode::Type::Element &&
         contains(kVoidElements, node->name);
}

bool is_meaningful_when_blank(const MdNode *node) {
  return node->type == MdNode::Type::Element &&
         contains(kMeaningfulWhenBlankElements, node->name);
}

bool is_pre(const MdNode *node) {
  return node->type == MdNode::Type::Element && node->name == "PRE";
}

// 子树中是否存在指定标签之一（对应 turndown 的 hasVoid /
// hasMeaningfulWhenBlank）。
template <size_t N>
bool has_descendant(const MdNode *node, const std::string_view (&list)[N]) {
  for (const auto &child : node->children) {
    if (child->type != MdNode::Type::Element) {
      continue;
    }
    if (contains(list, child->name) || has_descendant(child.get(), list)) {
      return true;
    }
  }
  return false;
}

bool is_blank(const MdNode *node) {
  if (node->type != MdNode::Type::Element) {
    return false;
  }
  return !is_void(node) && !is_meaningful_when_blank(node) &&
         str::strip_whitespace(node->text_content()).empty() &&
         !has_descendant(node, kVoidElements) &&
         !has_descendant(node, kMeaningfulWhenBlankElements);
}

bool is_code(const MdNode *node) {
  for (const MdNode *cursor = node; cursor != nullptr;
       cursor = cursor->parent) {
    if (cursor->type == MdNode::Type::Element && cursor->name == "CODE") {
      return true;
    }
  }
  return false;
}

bool starts_with(const std::string &text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

// -------------------------------------------------------- 空白折叠（预处理）

bool is_ws_ascii(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// 返回该位置空白字符的字节宽度，0 表示非空白。JS 的 \s 含 U+00A0，需按 UTF-8
// 判断。
size_t ws_width(const std::string &text, size_t pos) {
  const char c = text[pos];
  if (is_ws_ascii(c) || c == '\f' || c == '\v') {
    return 1;
  }
  if (text.compare(pos, kNbsp.size(), kNbsp) == 0) {
    return kNbsp.size();
  }
  return 0;
}

// 若 pos 处的字节是 nbsp 的后半字节，回退到该字符起始位置。
size_t ws_char_begin(const std::string &text, size_t pos, size_t lower_bound) {
  if (pos > lower_bound && text.compare(pos - 1, kNbsp.size(), kNbsp) == 0) {
    return pos - 1;
  }
  return pos;
}

// JS 的 String.prototype.trim() 会去掉 U+00A0，C++ 里必须自己实现，
// 否则嵌套的 <span>&nbsp;...&nbsp;</span> 每层都会把 nbsp 再累加一次。
std::string trim_js(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size()) {
    const size_t width = ws_width(text, begin);
    if (width == 0) {
      break;
    }
    begin += width;
  }

  size_t end = text.size();
  while (end > begin) {
    const size_t candidate = ws_char_begin(text, end - 1, begin);
    if (ws_width(text, candidate) == 0) {
      break;
    }
    end = candidate;
  }

  return text.substr(begin, end - begin);
}

// [ \r\n\t]+ → 单个空格
std::string collapse_runs(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  bool in_run = false;
  for (char c : text) {
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
      if (!in_run) {
        out += ' ';
        in_run = true;
      }
      continue;
    }
    in_run = false;
    out += c;
  }
  return out;
}

void strip_one_trailing_space(std::string &text) {
  if (!text.empty() && text.back() == ' ') {
    text.pop_back();
  }
}

// 删除节点并返回遍历序列中的下一个节点（对应 turndown 的 remove）。
MdNode *remove_node(MdNode *node) {
  MdNode *parent = node->parent;
  WXMD_ASSERT(parent != nullptr, "不能删除根节点");

  MdNode *next = node->next_sibling();
  if (next == nullptr) {
    next = parent;
  }

  const size_t index = node->index_in_parent();
  parent->children.erase(parent->children.begin() + static_cast<long>(index));
  return next;
}

// 对应 turndown 的 next(prev, current, isPre)
MdNode *next_node(MdNode *prev, MdNode *current) {
  if ((prev != nullptr && prev->parent == current) || is_pre(current)) {
    MdNode *sibling = current->next_sibling();
    return sibling != nullptr ? sibling : current->parent;
  }

  if (!current->children.empty()) {
    return current->children.front().get();
  }
  MdNode *sibling = current->next_sibling();
  return sibling != nullptr ? sibling : current->parent;
}

void collapse_whitespace(MdNode *element) {
  if (element->children.empty() || is_pre(element)) {
    return;
  }

  MdNode *prev_text = nullptr;
  bool keep_leading_ws = false;
  MdNode *prev = nullptr;
  MdNode *node = next_node(prev, element);

  while (node != element && node != nullptr) {
    if (node->type == MdNode::Type::Text) {
      std::string text = collapse_runs(node->text);

      const bool prev_ends_with_space = prev_text != nullptr &&
                                        !prev_text->text.empty() &&
                                        prev_text->text.back() == ' ';
      if ((prev_text == nullptr || prev_ends_with_space) && !keep_leading_ws &&
          !text.empty() && text.front() == ' ') {
        text.erase(0, 1);
      }

      if (text.empty()) {
        node = remove_node(node);
        continue;
      }

      node->text = text;
      prev_text = node;
    } else {
      if (is_block(node) || node->name == "BR") {
        if (prev_text != nullptr) {
          strip_one_trailing_space(prev_text->text);
        }
        prev_text = nullptr;
        keep_leading_ws = false;
      } else if (is_void(node) || is_pre(node)) {
        // 非块级 void 元素与内联 PRE 周围的空格需要保留
        prev_text = nullptr;
        keep_leading_ws = true;
      } else if (prev_text != nullptr) {
        keep_leading_ws = false;
      }
    }

    MdNode *upcoming = next_node(prev, node);
    prev = node;
    node = upcoming;
  }

  if (prev_text != nullptr) {
    strip_one_trailing_space(prev_text->text);
    if (prev_text->text.empty()) {
      remove_node(prev_text);
    }
  }
}

// ------------------------------------------------------------------ 转义

std::string escape_markdown(const std::string &input) {
  std::string text = input;

  str::replace_all(text, "\\", "\\\\");
  str::replace_all(text, "*", "\\*");

  if (starts_with(text, "-")) {
    text.insert(0, "\\");
  }
  if (starts_with(text, "+ ")) {
    text.insert(0, "\\");
  }
  if (starts_with(text, "=")) {
    text.insert(0, "\\");
  }
  {
    size_t hashes = 0;
    while (hashes < text.size() && text[hashes] == '#') {
      ++hashes;
    }
    if (hashes >= 1 && hashes <= 6 && hashes < text.size() &&
        text[hashes] == ' ') {
      text.insert(0, "\\");
    }
  }

  str::replace_all(text, "`", "\\`");

  if (starts_with(text, "~~~")) {
    text.insert(0, "\\");
  }

  str::replace_all(text, "[", "\\[");
  str::replace_all(text, "]", "\\]");

  if (starts_with(text, ">")) {
    text.insert(0, "\\");
  }

  str::replace_all(text, "_", "\\_");

  {
    size_t digits = 0;
    while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9') {
      ++digits;
    }
    if (digits > 0 && digits + 1 < text.size() && text[digits] == '.' &&
        text[digits + 1] == ' ') {
      text.insert(digits, "\\");
    }
  }

  return text;
}

// -------------------------------------------------------- 首尾空白（flanking）

struct Edges {
  std::string leading;
  std::string leading_ascii;
  std::string leading_non_ascii;
  std::string trailing;
  std::string trailing_non_ascii;
  std::string trailing_ascii;
};

// 对应 turndown 的 edgeWhitespace：区分 ASCII 空白与 nbsp 之类的非 ASCII 空白。
Edges edge_whitespace(const std::string &text) {
  Edges edges;

  size_t ascii_end = 0;
  while (ascii_end < text.size() && is_ws_ascii(text[ascii_end])) {
    ++ascii_end;
  }

  size_t leading_end = ascii_end;
  while (leading_end < text.size()) {
    const size_t width = ws_width(text, leading_end);
    if (width == 0) {
      break;
    }
    leading_end += width;
  }

  edges.leading_ascii = text.substr(0, ascii_end);
  edges.leading_non_ascii = text.substr(ascii_end, leading_end - ascii_end);
  edges.leading = text.substr(0, leading_end);

  // 全为空白时，leading 吞掉整个字符串，trailing 为空
  if (leading_end >= text.size()) {
    return edges;
  }

  // 找到最后一个非空白字符之后的位置
  size_t content_end = text.size();
  while (content_end > leading_end) {
    const size_t candidate = ws_char_begin(text, content_end - 1, leading_end);
    if (ws_width(text, candidate) == 0) {
      break;
    }
    content_end = candidate;
  }

  size_t ascii_begin = text.size();
  while (ascii_begin > content_end && is_ws_ascii(text[ascii_begin - 1])) {
    --ascii_begin;
  }

  edges.trailing = text.substr(content_end);
  edges.trailing_non_ascii =
      text.substr(content_end, ascii_begin - content_end);
  edges.trailing_ascii = text.substr(ascii_begin);
  return edges;
}

bool is_flanked_by_whitespace(bool left, const MdNode *node) {
  const MdNode *sibling =
      left ? node->previous_sibling() : node->next_sibling();
  if (sibling == nullptr) {
    return false;
  }

  std::string text;
  if (sibling->type == MdNode::Type::Text) {
    text = sibling->text;
  } else if (!is_block(sibling)) {
    text = sibling->text_content();
  } else {
    return false;
  }

  if (text.empty()) {
    return false;
  }
  return left ? text.back() == ' ' : text.front() == ' ';
}

struct Flanking {
  std::string leading;
  std::string trailing;
};

Flanking flanking_whitespace(const MdNode *node) {
  if (is_block(node)) {
    return {};
  }

  Edges edges = edge_whitespace(node->text_content());

  if (!edges.leading_ascii.empty() && is_flanked_by_whitespace(true, node)) {
    edges.leading = edges.leading_non_ascii;
  }
  if (!edges.trailing_ascii.empty() && is_flanked_by_whitespace(false, node)) {
    edges.trailing = edges.trailing_non_ascii;
  }

  return {edges.leading, edges.trailing};
}

// -------------------------------------------------------------------- 规则

std::string trim_leading_newlines(const std::string &text) {
  size_t pos = 0;
  while (pos < text.size() && text[pos] == '\n') {
    ++pos;
  }
  return text.substr(pos);
}

std::string trim_trailing_newlines(const std::string &text) {
  size_t end = text.size();
  while (end > 0 && text[end - 1] == '\n') {
    --end;
  }
  return text.substr(0, end);
}

// 对应 turndown 的 join：按两端换行数量决定用几个换行分隔。
std::string join(const std::string &output, const std::string &replacement) {
  const std::string s1 = trim_trailing_newlines(output);
  const std::string s2 = trim_leading_newlines(replacement);
  const size_t newlines =
      std::max(output.size() - s1.size(), replacement.size() - s2.size());
  const std::string separator =
      std::string("\n\n").substr(0, std::min<size_t>(newlines, 2));
  return s1 + separator + s2;
}

std::string clean_attribute(const std::string &value) {
  // (\n+\s*)+ → \n
  std::string out;
  size_t i = 0;
  while (i < value.size()) {
    if (value[i] != '\n') {
      out += value[i];
      ++i;
      continue;
    }
    while (i < value.size()) {
      const size_t width = ws_width(value, i);
      if (width == 0) {
        break;
      }
      i += width;
    }
    out += '\n';
  }
  return out;
}

std::string element_index_prefix(const MdNode *node) {
  const MdNode *parent = node->parent;
  if (parent == nullptr || parent->name != "OL") {
    return std::string(kBulletListMarker) + "   ";
  }

  // OL 的序号基于元素子节点的下标，start 属性可改变起始值
  int index = 0;
  for (const auto &child : parent->children) {
    if (child.get() == node) {
      break;
    }
    if (child->type == MdNode::Type::Element) {
      ++index;
    }
  }

  const std::string start = parent->attr("start");
  const int number = start.empty() ? index + 1 : std::stoi(start) + index;
  return std::to_string(number) + ".  ";
}

std::string apply_list_item(const MdNode *node, std::string content) {
  const std::string prefix = element_index_prefix(node);

  content = trim_leading_newlines(content);

  // 尾部连续换行折叠为一个；原本没有换行时不补
  const std::string without_trailing = trim_trailing_newlines(content);
  content = without_trailing.size() == content.size() ? content
                                                      : without_trailing + "\n";

  str::replace_all(content, "\n", "\n" + std::string(prefix.size(), ' '));

  const bool has_next = node->next_sibling() != nullptr;
  const bool ends_with_newline = !content.empty() && content.back() == '\n';
  return prefix + content + ((has_next && !ends_with_newline) ? "\n" : "");
}

bool is_fenced_code_block(const MdNode *node) {
  if (node->name != "PRE" || node->children.empty()) {
    return false;
  }
  const MdNode *first = node->children.front().get();
  return first->type == MdNode::Type::Element && first->name == "CODE";
}

std::string apply_fenced_code_block(const MdNode *node) {
  const MdNode *code = node->children.front().get();

  // class="language-xxx" 里的语言标识
  std::string language;
  const std::string class_name = code->attr("class");
  const size_t marker = class_name.find("language-");
  if (marker != std::string::npos) {
    size_t end = marker + 9;
    while (end < class_name.size() && !is_ws_ascii(class_name[end])) {
      ++end;
    }
    language = class_name.substr(marker + 9, end - marker - 9);
  }

  std::string text = code->text_content();

  // 正文里若已出现更长的反引号栅栏，需要加长外层栅栏
  size_t fence_size = 3;
  size_t line_begin = 0;
  while (line_begin <= text.size()) {
    size_t run = 0;
    while (line_begin + run < text.size() &&
           text[line_begin + run] == kFenceChar) {
      ++run;
    }
    if (run >= fence_size) {
      fence_size = run + 1;
    }
    const size_t newline = text.find('\n', line_begin);
    if (newline == std::string::npos) {
      break;
    }
    line_begin = newline + 1;
  }

  if (!text.empty() && text.back() == '\n') {
    text.pop_back();
  }

  const std::string fence(fence_size, kFenceChar);
  return "\n\n" + fence + language + "\n" + text + "\n" + fence + "\n\n";
}

std::string apply_inline_code(std::string content) {
  if (content.empty()) {
    return {};
  }

  str::replace_all(content, "\r\n", " ");
  str::replace_all(content, "\n", " ");
  str::replace_all(content, "\r", " ");

  // 内容以反引号开头/结尾，或首尾都是空格且中间有实义字符时，需要补一个空格
  bool needs_space = content.front() == '`' || content.back() == '`';
  if (!needs_space && content.size() >= 2 && content.front() == ' ' &&
      content.back() == ' ' &&
      content.find_first_not_of(' ') != std::string::npos) {
    needs_space = true;
  }
  const std::string extra = needs_space ? " " : "";

  // 分隔符需比内容里最长的反引号串更长
  std::string delimiter = "`";
  while (true) {
    bool clash = false;
    for (size_t i = 0; i < content.size();) {
      if (content[i] != '`') {
        ++i;
        continue;
      }
      size_t run = 0;
      while (i + run < content.size() && content[i + run] == '`') {
        ++run;
      }
      if (run == delimiter.size()) {
        clash = true;
        break;
      }
      i += run;
    }
    if (!clash) {
      break;
    }
    delimiter += '`';
  }

  return delimiter + extra + content + extra + delimiter;
}

std::string apply_image(const MdNode *node) {
  const std::string src = node->attr("src");
  if (src.empty()) {
    return {};
  }
  const std::string alt = clean_attribute(node->attr("alt"));
  const std::string title = clean_attribute(node->attr("title"));
  const std::string title_part = title.empty() ? "" : " \"" + title + "\"";
  return "![" + alt + "](" + src + title_part + ")";
}

std::string apply_inline_link(const MdNode *node, const std::string &content) {
  std::string href = node->attr("href");
  str::replace_all(href, "(", "\\(");
  str::replace_all(href, ")", "\\)");

  std::string title = clean_attribute(node->attr("title"));
  if (!title.empty()) {
    str::replace_all(title, "\"", "\\\"");
    title = " \"" + title + "\"";
  }
  return "[" + content + "](" + href + title + ")";
}

std::string apply_blockquote(const std::string &content) {
  std::string inner = trim_trailing_newlines(trim_leading_newlines(content));

  // 每一行都加上 "> " 前缀
  std::string out = "> ";
  for (char c : inner) {
    out += c;
    if (c == '\n') {
      out += "> ";
    }
  }
  return "\n\n" + out + "\n\n";
}

std::string process(const MdNode *parent);

std::string apply_rule(const MdNode *node, const std::string &content) {
  const std::string &name = node->name;

  if (is_blank(node)) {
    return is_block(node) ? "\n\n" : "";
  }

  if (name == "P") {
    return "\n\n" + content + "\n\n";
  }
  if (name == "BR") {
    return std::string(kLineBreak) + "\n";
  }
  if (name.size() == 2 && name[0] == 'H' && name[1] >= '1' && name[1] <= '6') {
    const int level = name[1] - '0';
    return "\n\n" + std::string(static_cast<size_t>(level), '#') + " " +
           content + "\n\n";
  }
  if (name == "BLOCKQUOTE") {
    return apply_blockquote(content);
  }
  if (name == "UL" || name == "OL") {
    const MdNode *parent = node->parent;
    if (parent != nullptr && parent->name == "LI" &&
        parent->last_element_child() == node) {
      return "\n" + content;
    }
    return "\n\n" + content + "\n\n";
  }
  if (name == "LI") {
    return apply_list_item(node, content);
  }
  if (is_fenced_code_block(node)) {
    return apply_fenced_code_block(node);
  }
  if (name == "HR") {
    return "\n\n" + std::string(kHorizontalRule) + "\n\n";
  }
  if (name == "A" && node->has_attr("href") && !node->attr("href").empty()) {
    return apply_inline_link(node, content);
  }
  if (name == "EM" || name == "I") {
    if (trim_js(content).empty()) {
      return {};
    }
    return std::string(kEmDelimiter) + content + std::string(kEmDelimiter);
  }
  if (name == "STRONG" || name == "B") {
    if (trim_js(content).empty()) {
      return {};
    }
    return std::string(kStrongDelimiter) + content +
           std::string(kStrongDelimiter);
  }
  if (name == "CODE") {
    const bool has_siblings =
        node->previous_sibling() != nullptr || node->next_sibling() != nullptr;
    const bool is_code_block =
        node->parent != nullptr && node->parent->name == "PRE" && !has_siblings;
    if (!is_code_block) {
      return apply_inline_code(content);
    }
  }
  if (name == "IMG") {
    return apply_image(node);
  }

  return is_block(node) ? "\n\n" + content + "\n\n" : content;
}

std::string replacement_for_node(const MdNode *node) {
  std::string content = process(node);
  const Flanking whitespace = flanking_whitespace(node);

  if (!whitespace.leading.empty() || !whitespace.trailing.empty()) {
    content = trim_js(content);
  }
  return whitespace.leading + apply_rule(node, content) + whitespace.trailing;
}

std::string process(const MdNode *parent) {
  std::string output;
  for (const auto &child : parent->children) {
    const MdNode *node = child.get();

    std::string replacement;
    if (node->type == MdNode::Type::Text) {
      replacement = is_code(node) ? node->text : escape_markdown(node->text);
    } else {
      replacement = replacement_for_node(node);
    }
    output = join(output, replacement);
  }
  return output;
}

bool should_remove(const MdNode &node) {
  if (node.type != MdNode::Type::Element) {
    return false;
  }
  if (contains(kRemovedElements, node.name)) {
    return true;
  }
  // 底部互动栏的图标是内联 data URI，转换后是大段乱码
  return node.attr("class").find("__bottom-bar__") != std::string::npos;
}

} // namespace

std::string to_markdown(const std::string &html) {
  if (html.empty()) {
    return {};
  }

  dom::Document doc(html);
  std::unique_ptr<MdNode> root = dom::build_tree(doc.body());

  dom::remove_if(*root, should_remove);
  collapse_whitespace(root.get());

  std::string output = process(root.get());

  // 对应 postProcess 的收尾：去掉首部换行与尾部空白
  size_t begin = 0;
  while (begin < output.size() &&
         (output[begin] == '\t' || output[begin] == '\r' ||
          output[begin] == '\n')) {
    ++begin;
  }
  size_t end = output.size();
  while (end > begin) {
    const size_t candidate = end - 1;
    if (!is_ws_ascii(output[candidate]) && output[candidate] != '\f' &&
        output[candidate] != '\v') {
      break;
    }
    end = candidate;
  }
  return output.substr(begin, end - begin);
}

} // namespace wxmd
