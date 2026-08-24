"""HTML → Markdown 转换器 (基于 lxml, 零额外依赖)。

果仁正文是 summernote 编辑器输出, 标签集较简单: p/br/b/strong/i/em/u/s/a/img/
ul/ol/li/pre/code/blockquote/h1-h6/hr/span/font/div/table。本转换器只覆盖这些,
未识别的标签按 inline 透传。img 的 src 可经 on_image 钩子改写成本地相对路径。
"""

import re

from lxml import html as lxml_html

# 块级元素: 渲染产出以 \n\n 收尾
_BLOCK = {
    "p", "div", "section", "article", "figure", "figcaption",
    "h1", "h2", "h3", "h4", "h5", "h6",
    "ul", "ol", "pre", "blockquote", "table", "hr",
}
_INLINE_PASSTHROUGH = {
    "span", "font", "u", "small", "big", "sub", "sup", "mark", "label",
    "tt", "abbr", "cite", "q", "time", "wbr",
}


def to_markdown(content_html, on_image=None):
    """把一段 HTML 正文转成 Markdown。on_image(src)->str 用于改写 img 的 src。"""
    wrapped = f"<div>{content_html}</div>"
    root = lxml_html.fragment_fromstring(wrapped)
    out = _inner(root, on_image)
    out = re.sub(r"\n{3,}", "\n\n", out).strip()
    return out + "\n" if out else ""


def _inner(el, on_image):
    """el 的内部内容 = escaped(el.text) + Σ(_render(child) + escaped(child.tail))。"""
    s = _esc(_text(el.text or ""))
    for child in el:
        if isinstance(child.tag, str):
            s += _render(child, on_image)
        s += _esc(_text(child.tail or ""))
    return s


def _render(el, on_image):
    tag = el.tag
    if tag in ("script", "style", "noscript", "meta", "link", "title"):
        return ""
    if tag == "br":
        return "  \n"
    if tag == "img":
        src = (el.get("src") or "").strip()
        alt = (el.get("alt") or "").strip()
        if on_image and src:
            src = on_image(src) or src
        return f"![{alt}]({src})"
    if tag == "a":
        inner = _inner(el, on_image).strip()
        href = (el.get("href") or "").strip()
        if not href or href.startswith("javascript:"):
            return inner
        return f"[{inner}]({href})"
    if tag in ("b", "strong"):
        inner = _inner(el, on_image).strip()
        return f"**{inner}**" if inner else ""
    if tag in ("i", "em"):
        inner = _inner(el, on_image).strip()
        return f"*{inner}*" if inner else ""
    if tag in ("s", "del", "strike"):
        inner = _inner(el, on_image).strip()
        return f"~~{inner}~~" if inner else ""
    if tag == "code":
        # 行内 code: 取纯文本, 不递归格式化
        txt = el.text_content()
        return f"`{txt}`"
    if tag == "pre":
        code = el.text_content().rstrip("\n")
        lang = _guess_lang(el)
        fence = "```"
        return f"{fence}{lang}\n{code}\n{fence}\n\n"
    if tag in ("h1", "h2", "h3", "h4", "h5", "h6"):
        n = int(tag[1])
        inner = _inner(el, on_image).strip()
        return f"{'#' * n} {inner}\n\n" if inner else ""
    if tag == "hr":
        return "---\n\n"
    if tag == "blockquote":
        inner = _inner(el, on_image).strip()
        if not inner:
            return ""
        quoted = "\n".join("> " + ln if ln else ">" for ln in inner.splitlines())
        return quoted + "\n\n"
    if tag in ("ul", "ol"):
        return _render_list(el, on_image, ordered=(tag == "ol")) + "\n"
    if tag == "table":
        return _render_table(el, on_image) + "\n"
    if tag in _INLINE_PASSTHROUGH:
        return _inner(el, on_image)
    if tag in _BLOCK:
        inner = _inner(el, on_image).strip()
        return inner + "\n\n" if inner else ""
    # 未知标签: 透传内部
    return _inner(el, on_image)


def _render_list(el, on_image, ordered, depth=0):
    lines = []
    idx = 1
    for li in el:
        if li.tag != "li":
            continue
        marker = f"{idx}. " if ordered else "- "
        idx += 1
        # 取 li 的非列表子内容作为正文, 嵌套列表单独处理
        body_parts = []
        nested = []
        body_parts.append(_esc(_text(li.text or "")))
        for sub in li:
            if sub.tag in ("ul", "ol"):
                nested.append(_render_list(sub, on_image, sub.tag == "ol", depth + 1))
            else:
                if isinstance(sub.tag, str):
                    body_parts.append(_render(sub, on_image))
                body_parts.append(_esc(_text(sub.tail or "")))
        body = re.sub(r"\s+", " ", "".join(body_parts)).strip()
        indent = "  " * depth
        lines.append(f"{indent}{marker}{body}")
        for block in nested:
            for ln in block.splitlines():
                if ln:
                    lines.append(ln)
    return "\n".join(lines) + "\n"


def _render_table(el, on_image):
    rows = []
    for tr in el.iter("tr"):
        cells = []
        for c in tr:
            if c.tag not in ("td", "th"):
                continue
            cells.append(_inner(c, on_image).strip().replace("\n", " "))
        if cells:
            rows.append(cells)
    if not rows:
        return ""
    out = "| " + " | ".join(rows[0]) + " |\n"
    out += "| " + " | ".join("---" for _ in rows[0]) + " |\n"
    for r in rows[1:]:
        # 补齐列数
        while len(r) < len(rows[0]):
            r.append("")
        out += "| " + " | ".join(r) + " |\n"
    return out


def _guess_lang(pre):
    cls = pre.get("class") or ""
    m = re.search(r"language-(\w+)", cls)
    return m.group(1) if m else ""


def _text(s, preserve=False):
    if not s:
        return ""
    if preserve:
        return s
    return re.sub(r"\s+", " ", s.replace("\xa0", " "))


def _esc(s):
    s = s.replace("\\", "\\\\")
    for ch in ("`", "*", "_", "[", "]"):
        s = s.replace(ch, "\\" + ch)
    return s
