"""图片本地化的公共零件。两个站点原本各有一份逐字相同的 _ext_from_ctype, 收到这里。

on_image 契约 (html2md 与 mdimg 都按这个签名回调):
    on_image(src) -> str    返回替换后的相对路径; 返回原 src 表示保留外链。

collector() 造出一个 (on_image, assets, holes) 三元组: on_image 边抓边把字节攒进
assets, 抓不到的记进 holes。字节先留在内存里由主线程串行提交, 所以 on_image 可并发。
"""

from .net import MISSING


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


def collector(fetch_asset, absolutize):
    """→ (on_image, assets, holes)。

    fetch_asset(url) -> (bytes, ctype) | (MISSING, "") 图没了 | (None, "") 这次没抓到
    absolutize(src)  -> 绝对 URL, 或 None 表示这个 src 不必抓 (data:/file: 之类)

    图没了 (MISSING) 就保留原链 — 死链重抓也回不来, 不该卡住整篇;
    这次没抓到 (None) 记进 holes, 调用方据此把整篇作空洞, 下轮重来。
    """
    assets = {}
    seen = {}  # 同一地址只抓一次
    holes = []
    counter = {"n": 0}

    def on_image(src):
        full = absolutize(src)
        if full is None:
            return src
        if full in seen:
            return seen[full]
        data, ctype = fetch_asset(full)
        if data is MISSING or data is None:
            if data is None:
                holes.append(full)
            return src
        counter["n"] += 1
        name = f"{counter['n']:03d}{ext_from_ctype(ctype, full)}"
        assets[name] = data
        seen[full] = f"assets/{name}"
        return seen[full]

    return on_image, assets, holes
