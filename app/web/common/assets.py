"""图片本地化的公共零件, 分两步做。

on_image 契约 (html2md 与 mdimg 都按这个签名回调):
    on_image(src) -> str    返回写进 markdown 的那个字符串。

第一步 collector(): 渲染正文时只登记图片、不下载, 当场返回一个占位 token。
第二步 resolve():   照登记的顺序把图片抓下来, 把 token 换成 assets/NNN.ext。

分两步是为了不让图片拖住出网的并发: 图片是直连的慢活 (一篇十几张、单张最坏十几秒),
如果边渲染边抓, 抓图期间这篇一直占着一个 worker 槽, 而那些槽是用来把隧道额度发满的。
拆开之后正文与评论一抓完就能把这篇交给图片阶段, 隧道那边立刻去领下一篇 (见 sync.drain)。
"""

from .net import MISSING

# 占位 token 用 \x00 前后夹住: 正文里不可能出现, markdown 也不会转义它。前后都有边界
# 字符, 所以 img0001 绝不会误匹配 img00010 那种更长的 token。
_TOKEN = "\x00img{:04d}\x00"


def ext_from_ctype(ctype, url):
    ct = (ctype or "").lower()
    if "jpeg" in ct or "jpg" in ct:
        return ".jpg"
    if "png" in ct:
        return ".png"
    if "gif" in ct:
        return ".gif"
    if "webp" in ct:
        return ".webp"
    if "svg" in ct:
        return ".svg"
    last = url.rsplit(".", 1)[-1].split("?")[0].split("!")[0]
    if last.isalpha() and len(last) <= 4:
        return "." + last.lower()
    return ".bin"


def collector(absolutize):
    """→ (on_image, pending)。只登记不下载, 所以渲染这一步不碰网络。

    absolutize(src) -> 绝对 URL, 或 None 表示这个 src 不必抓 (data:/file: 之类)
    pending 是 [(token, src, url)], 按正文里出现的顺序; 交给 resolve() 去抓。
    """
    pending = []
    seen = {}  # 同一地址只抓一次, 复用同一个 token

    def on_image(src):
        full = absolutize(src)
        if full is None:
            return src
        if full in seen:
            return seen[full]
        token = _TOKEN.format(len(pending))
        pending.append((token, src, full))
        seen[full] = token
        return token

    return on_image, pending


def resolve(fetch_asset, markdown, pending):
    """把占位 token 换成本地相对路径。→ (markdown, assets) | None (这次没抓到)。

    fetch_asset(url) -> (bytes, ctype) | (MISSING, "") 图没了 | (None, "") 这次没抓到

    图没了 (MISSING) 就换回原链 — 死链重抓也回不来, 不该卡住整篇;
    有一张「这次没抓到」就整篇作空洞返回 None, 不入库、下轮重来: index 里的是终态,
    宁可整篇重抓也不堆半成品。第一张没抓到就立刻收手, 剩下的不必再花请求。
    """
    assets = {}
    n = 0
    for token, src, url in pending:
        data, ctype = fetch_asset(url)
        if data is None:
            return None
        if data is MISSING:
            markdown = markdown.replace(token, src)
            continue
        n += 1
        name = f"{n:03d}{ext_from_ctype(ctype, url)}"
        assets[name] = data
        markdown = markdown.replace(token, f"assets/{name}")
    assert "\x00" not in markdown, "占位 token 没换干净, 正文里落下了 \\x00"
    return markdown, assets
