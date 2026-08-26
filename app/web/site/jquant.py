"""聚宽社区 (joinquant.com) 适配器。

接口 (经 playwright 抓包确认):
    GET /community/post/listV2?limit=N&page=N&cate=N&type=isNewPublish
        列表, 返回 data.list / data.totalCount / data.topCount
    GET /community/post/detailV2?postId=<key>
        详情, data.content 直接是 markdown 正文 (所以不需要 html2md)
    GET /community/post/replyList?page=N&postId=<key>
        评论, 每页 20, 返回 data.replyArr / data.totalCount
公共头: X-Requested-With: XMLHttpRequest

帖子的身份用 uniqueKey, 不是 postId: postId 每次请求都重新随机生成 (同一篇文章连查
三次是三个值), 只是个一次性别名; uniqueKey 跨请求恒定。上面两个接口的 postId 参数
uniqueKey 也照吃, 所以全程只认 uniqueKey。userId / euid 同样是每请求轮换的。

列表按 addTime 倒序 (type=isNewPublish), 排序稳定 — 新帖只往前插, 老帖不动。

登录态只有列表翻页需要: 匿名请求被平台卡在 page 10 (每页最多 200 ⇒ 只能看到最新 2000
篇), 第 11 页起返回业务码 400「未授权」, 所以全量 4 万篇必须带 cookie 才翻得到。但读单篇
文章不需要 —— 实测匿名的 detailV2 / replyList 与带 cookie 拿到的载荷完全一致, 所以走隧道
那条路一律匿名 (见 net.py)。cookie 放在 web/cookies/jquant.txt, 一行一条 Name=Value,
# 开头是注释, 过期了改这个文件。
拿法: 浏览器登录后 F12 → Application → Storage → Cookies → www.joinquant.com。

出网选路与限速一律照 common/net.py 的规矩 (列表直连、文章走隧道、图片直连), 本文件
只挑对方法: 列表用 list_json, 正文与评论用 post_json。聚宽这边尤其不能让列表走隧道 ——
limit=200 一页约 8KB (gzip), 实测会被隧道截成 IncompleteRead。
"""

import os
import time

import requests

from ..common import mdimg
from ..common.assets import collector
from ..common.net import MISSING, Fetcher
from ..common.tunnel import ENABLED as TUNNEL

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
BASE = "https://www.joinquant.com"
COOKIE_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "cookies", "jquant.txt"
)
PAGE_SIZE = 200

# 图片直连 (与 common/net.py 的规矩一致), 也不走 _pace 限速: 图床不是论坛, 不会因为
# 并发盯上本机 IP。聚宽图床在 cdn.joinquant.com。
ASSET_TIMEOUT = 15
ASSET_RETRIES = 2
BACKOFF = 1.5


class Client(Fetcher):
    host = "www.joinquant.com"
    timeout = 40
    # 列表是串行翻页, 3 次/s 足够 205 页, 也不至于让本机 IP 显眼。
    qps = 3
    # 隧道单请求实测 2~4s (换 IP + 转发), 聚宽一篇还要连发 detail + 多页评论。要把
    # 10 次/s 发满, 同时在飞的请求得有 qps × 延迟 ≈ 30 个, 所以给 30。
    # 全直连时本机就一个出口 IP, 并发只会更显眼, 老老实实 1。
    workers = 30 if TUNNEL else 1
    # 隧道下重试就是换一个 IP 再来, 多给几次; 全直连换不掉 IP, 3 次够。
    retries = 5 if TUNNEL else 3

    def __init__(self):
        super().__init__()
        self._cookies = self._load_cookies()

    def _load_cookies(self):
        assert os.path.exists(COOKIE_PATH), (
            f"缺 {COOKIE_PATH} — 聚宽匿名只能看到最新 2000 篇。浏览器登录后 F12 →"
            f" Application → Cookies → www.joinquant.com, 每条写成一行 Name=Value"
        )
        cookies = {}
        with open(COOKIE_PATH, encoding="utf-8") as f:
            for ln in f:
                ln = ln.strip()
                if not ln or ln.startswith("#") or "=" not in ln:
                    continue
                name, value = ln.split("=", 1)
                cookies[name.strip()] = value.strip()
        assert cookies, f"{COOKIE_PATH} 里没有有效 cookie 行"
        return cookies

    def _headers(self, s):
        s.headers["User-Agent"] = UA
        s.headers["X-Requested-With"] = "XMLHttpRequest"
        s.headers["Accept"] = "application/json, text/plain, */*"
        s.headers["Referer"] = BASE + "/view/community/list"

    def _auth(self, s):
        # 只有直连 (列表) 会走到这里, 隧道那条路是匿名的 —— 见 net.py 的规矩。
        for k, v in self._cookies.items():
            s.cookies.set(k, v, domain=self.host)

    def desc(self):
        return f"{super().desc()}; 列表带 {len(self._cookies)} 条 cookie"

    def asset(self, url):
        """→ (bytes, ctype) | (MISSING, "") 图没了 | (None, "") 这次没抓到。

        图片直连、不限速 (见上面的 ASSET 注释)。抓不到就保留原链, 不卡整篇。
        """
        last = None
        for attempt in range(ASSET_RETRIES + 1):
            try:
                r = self._direct_session().get(url, timeout=ASSET_TIMEOUT)
            except requests.RequestException as e:
                last = e
                if attempt < ASSET_RETRIES:
                    time.sleep(BACKOFF**attempt)
                continue
            if r.status_code == 200:
                return r.content, r.headers.get("Content-Type", "")
            if r.status_code in self.missing_status:
                return MISSING, ""
            last = f"HTTP {r.status_code}"
            if attempt < ASSET_RETRIES and r.status_code >= 500:
                time.sleep(BACKOFF**attempt)
                continue
            break
        print(f"  ! {url} 抓不到 ({last}), 试了 {ASSET_RETRIES + 1} 次", flush=True)
        return None, ""


def _entry(p, cate):
    u = p.get("user") or {}
    # 主键必须用 uniqueKey 而不是 postId: postId 每次请求都重新随机生成 (同一篇文章
    # 连查三次能拿到三个不同的 postId, userId/euid 同理), 拿它当主键会让去重、seen()
    # 与深翻锚点全部失效 —— 每轮都把整个列表当成新帖重新入队。uniqueKey 跨请求恒定,
    # 而且 detailV2 / replyList 的 postId 参数直接吃它, 所以 postId 根本不用留。
    assert p.get("uniqueKey"), f"列表条目缺 uniqueKey: {p.get('title')}"
    return {
        "pid": p["uniqueKey"],
        "title": p.get("title") or "",
        "create_time": p.get("addTime") or "",
        "tag": str(cate),
        "author": (u.get("alias") or ""),
        "uid": u.get("userId"),
        "reply_count": int(p.get("replyCount") or 0),
        "view_count": int(p.get("viewCount") or 0),
        "is_top": p.get("isTop") == "1",
        "status": "pending",
    }


def _assemble(entry, body_md, comments_md):
    meta = (
        f"# {entry['title']}\n\n"
        f"> 作者: {entry['author']} (uid={entry['uid']})  \n"
        f"> 时间: {entry['create_time']}  \n"
        f"> 栏目: cate={entry['tag']}  \n"
        f"> 链接: {BASE}/community/post/detailV2?postId={entry['pid']}\n"
    )
    parts = [meta, body_md, comments_md]
    return "\n\n".join(p.rstrip() for p in parts if p) + "\n"


def _absolutize(src):
    if not src or src.startswith("data:") or src.startswith("file:"):
        return None
    return BASE + src if src.startswith("/") else src


class Site:
    name = "jquant"
    # 3=文章 / 10=问答 / 13=公告 / 14=精华 / 16=精选
    default_tags = ["3"]

    def __init__(self):
        self.fetcher = Client()

    def list_page(self, page, tag):
        """翻页是主流程的地基, 抓不到就当场炸: 拿半张列表比不拿更糟。"""
        j = self.fetcher.list_json(
            f"{BASE}/community/post/listV2",
            {"limit": PAGE_SIZE, "page": page, "cate": tag, "type": "isNewPublish"},
        )
        assert (
            j is not MISSING and j is not None
        ), f"列表第 {page} 页 (cate={tag}) 抓不到"
        # 匿名 (或 cookie 过期) 时第 11 页起就是这个码。当场炸而不是当空页收手:
        # 静悄悄地少翻 190 页, 比报错难发现得多。
        assert not _unauthorized(
            j
        ), f"listV2 第 {page} 页未授权 — cookie 失效或未登录, 刷新 {COOKIE_PATH}"
        assert (
            j.get("status") == "ok" or j.get("code") == "00000"
        ), f"listV2 -> {j.get('code')} {j.get('msg')}"
        data = j["data"]
        return {
            "entries": [_entry(p, tag) for p in (data.get("list") or [])],
            "total_count": int(data.get("totalCount") or 0),
            "page_size": PAGE_SIZE,
        }

    def _data(self, path, params):
        """→ data | MISSING (帖子没了/不可见) | None (这次没抓到)。

        聚宽对已删除的帖子返回 HTTP 200 + 业务错误码, 所以业务码不对 = 永久缺失,
        只有传输层失败才是「这次没抓到」。这条映射是墓碑与空洞的分界, 别弄反。
        """
        j = self.fetcher.post_json(BASE + path, params)
        if j is MISSING or j is None:
            return j
        if j.get("code") != "00000":
            return MISSING
        return j["data"]

    def _replies_md(self, pid, on_image):
        """→ Markdown | None (评论没抓全, 整篇作空洞: 缺一页评论的半成品不入库)。"""
        page = 1
        out = []
        total = None
        while True:
            data = self._data(
                "/community/post/replyList", {"page": page, "postId": pid}
            )
            if data is None:
                return None
            if data is MISSING:
                break
            arr = data.get("replyArr") or []
            if total is None:
                total = int(data.get("totalCount") or 0)
            out.extend(arr)
            if len(out) >= total or not arr:
                break
            page += 1
        if not out:
            return ""
        lines = [f"## 评论 ({len(out)})"]
        for r in out:
            u = r.get("user") or {}
            content = mdimg.rewrite_images(r.get("content", "") or "", on_image)
            lines.append(f"\n### {u.get('alias', '')} · {r.get('addTime', '')}\n")
            lines.append(content.rstrip())
            subs = self._subreplies(pid, r)
            if subs is None:
                return None
            for s in subs:
                su = s.get("user") or {}
                scontent = mdimg.rewrite_images(s.get("content", "") or "", on_image)
                lines.append(
                    f"\n> **{su.get('alias', '')}** · {s.get('addTime', '')}\n>"
                )
                lines.append("> " + scontent.rstrip().replace("\n", "\n> "))
        return "\n".join(lines) + "\n"

    def _subreplies(self, pid, reply):
        """→ 这条父回复的全部子回复 | None (这次没抓到)。

        subReply 是 {"list": [...], "leaveCount": N} 而不是数组, leaveCount 是没随本次
        返回的剩余条数。补全就是前端「点击展开」那一发 (chunk 里的 handleShowReplyMore):
            GET /community/post/replyList?postId=<key>&oReplyId=<父回复 replyId>
        它返回这条父回复的全量子回复 (开头几条与已给的一致), 所以整体替换而非追加,
        而且返回结果里不再有 leaveCount, 不必递归。

        oReplyId 只能用当前这个响应里的 replyId 就地用掉: 它和 postId 一样每请求轮换
        (实测同一条回复连查两次是两个值), 缓存或落盘都会失效。
        """
        sub = reply.get("subReply") or {}
        have = sub.get("list") or []
        if not sub.get("leaveCount"):
            return have
        data = self._data(
            "/community/post/replyList",
            {"postId": pid, "oReplyId": reply["replyId"]},
        )
        if data is None:
            return None
        assert data is not MISSING, (
            f"子回复补全被拒 (leaveCount={sub['leaveCount']}), replyList 的 oReplyId"
            f" 用法变了"
        )
        arr = data.get("replyArr") or []
        assert len(arr) >= len(
            have
        ), f"子回复补全反而更少 ({len(arr)} < 已给 {len(have)}), 接口语义变了"
        return arr

    def download_post(self, entry):
        """→ (markdown, images, status) | None (这次没抓到, 作空洞留在队列下轮补)。

        只管出网拿正文与评论, 图片一张都不抓: on_image 只登记, 由 drain 的第二级去抓
        (见 assets.collector)。这样本函数一返回, 出网的那个槽就能去领下一篇。
        """
        detail = self._data("/community/post/detailV2", {"postId": entry["pid"]})
        if detail is None:
            return None
        if detail is MISSING:
            return _assemble(entry, "_(帖子已删除或不可见)_", ""), [], "deleted"

        on_image, images = collector(_absolutize)
        # 聚宽正文本身就是 markdown, 只需把图片链接换成本地相对路径
        body_md = mdimg.rewrite_images(detail.get("content", "") or "", on_image)
        comments_md = (
            self._replies_md(entry["pid"], on_image)
            if (entry.get("reply_count") or 0) > 0
            else ""
        )
        if comments_md is None:
            return None
        return _assemble(entry, body_md, comments_md), images, "done"


def _unauthorized(j):
    return j.get("code") in ("400", 400) and j.get("msg") == "未授权"
