"""markdown 正文里的图片本地化。

聚宽正文本身就是 markdown (detailV2 直接返回 content), 不需要 html2md。
这里只做一件事: 扫描 ![alt](url) 与 <img src="url">, 对每个 url 调 on_image
拿到本地路径 (或失败时原样保留 url), 重写回正文。
"""

import re

_MD_IMG = re.compile(r'!\[([^\]]*)\]\(([^)\s]+)\)')
_HTML_IMG = re.compile(r'<img[^>]*\bsrc=["\']([^"\']+)["\'][^>]*>', re.IGNORECASE)


def rewrite_images(md, on_image):
    def md_sub(m):
        alt, url = m.group(1), m.group(2)
        new = on_image(url)
        return f"![{alt}]({new or url})"

    def html_sub(m):
        url = m.group(1)
        new = on_image(url)
        if new:
            return m.group(0).replace(url, new, 1)
        return m.group(0)

    md = _MD_IMG.sub(md_sub, md)
    md = _HTML_IMG.sub(html_sub, md)
    return md
