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

登录态是硬要求: 匿名请求被平台卡在 page 10 (每页最多 200 ⇒ 只能看到最新 2000 篇),
第 11 页起返回业务码 400「未授权」。全量 4 万篇必须带 cookie。cookie 放在
web/cookies/jquant.txt, 一行一条 Name=Value, # 开头是注释, 过期了改这个文件。
拿法: 浏览器登录后 F12 → Application → Storage → Cookies → www.joinquant.com。

出网直连本机 IP: 聚宽没像果仁那样封过我们, 不必动隧道那套。
"""

import os

import requests

from ..common import mdimg
from ..common.assets import collector
from ..common.net import MISSING, Fetcher

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
BASE = "https://www.joinquant.com"
COOKIE_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "cookies", "jquant.txt"
)
PAGE_SIZE = 200


class Client(Fetcher):
    timeout = 40
    # 直连本机 IP: 天花板是「别被风控盯上」而不是吞吐, 慢一点无所谓。
    qps = 3
    workers = 4
    retries = 3
    # 直连时 403 是稳定的拒绝 (图床防盗链), 与 404 一样当永久缺失。
    missing_status = (403, 404)

    def __init__(self):
        super().__init__()
        self.s = requests.Session()
        self.s.headers["User-Agent"] = UA
        self.s.headers["X-Requested-With"] = "XMLHttpRequest"
        self.s.headers["Accept"] = "application/json, text/plain, */*"
        self.s.headers["Referer"] = BASE + "/view/community/list"
        self.n_cookies = self._load_cookies()

    def _load_cookies(self):
        assert os.path.exists(COOKIE_PATH), (
            f"缺 {COOKIE_PATH} — 聚宽匿名只能看到最新 2000 篇。浏览器登录后 F12 →"
            f" Application → Cookies → www.joinquant.com, 每条写成一行 Name=Value"
        )
        n = 0
        with open(COOKIE_PATH, encoding="utf-8") as f:
            for ln in f:
                ln = ln.strip()
                if not ln or ln.startswith("#") or "=" not in ln:
                    continue
                name, value = ln.split("=", 1)
                self.s.cookies.set(
                    name.strip(), value.strip(), domain="www.joinquant.com"
                )
                n += 1
        assert n, f"{COOKIE_PATH} 里没有有效 cookie 行"
        return n

    def desc(self):
        return (
            f"出网: 直连 (本机 IP) {self.qps} 次/s {self.workers} 并发; "
            f"已载入 {self.n_cookies} 条 cookie"
        )

    def _fetch(self, url, params):
        return self.s.get(url, params=params, timeout=self.timeout)

    def asset(self, url):
        """→ (bytes, ctype) | (MISSING, "") 图没了 | (None, "") 这次没抓到。"""
        r = self.get(url)
        if r is MISSING or r is None:
            return r, ""
        return r.content, r.headers.get("Content-Type", "")


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
        j = self.fetcher.json(
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
        j = self.fetcher.json(BASE + path, params)
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
            # subReply 是 {"list": [...], "leaveCount": N} 而不是数组; leaveCount 是
            # 没随本次返回的剩余子回复数。采样所见一直是 0, 真碰上非 0 就当场炸:
            # 说明还有个「展开更多」的翻页参数没摸清, 默默少写几条比报错难发现得多。
            sub = r.get("subReply") or {}
            assert not sub.get("leaveCount"), (
                f"子回复没给全 (leaveCount={sub.get('leaveCount')}, 已给"
                f" {len(sub.get('list') or [])} 条), replyList 还有翻页参数没摸清"
            )
            for s in sub.get("list") or []:
                su = s.get("user") or {}
                scontent = mdimg.rewrite_images(s.get("content", "") or "", on_image)
                lines.append(
                    f"\n> **{su.get('alias', '')}** · {s.get('addTime', '')}\n>"
                )
                lines.append("> " + scontent.rstrip().replace("\n", "\n> "))
        return "\n".join(lines) + "\n"

    def download_post(self, entry):
        """→ (markdown, assets, status) | None (这次没抓到, 作空洞留在队列下轮补)。"""
        detail = self._data("/community/post/detailV2", {"postId": entry["pid"]})
        if detail is None:
            return None
        if detail is MISSING:
            return _assemble(entry, "_(帖子已删除或不可见)_", ""), {}, "deleted"

        on_image, assets, holes = collector(self.fetcher.asset, _absolutize)
        # 聚宽正文本身就是 markdown, 只需把图片链接换成本地相对路径
        body_md = mdimg.rewrite_images(detail.get("content", "") or "", on_image)
        if holes:
            return None  # 早退: 已经缺图了, 不必再花评论那几个请求
        comments_md = (
            self._replies_md(entry["pid"], on_image)
            if (entry.get("reply_count") or 0) > 0
            else ""
        )
        if comments_md is None or holes:
            return None
        return _assemble(entry, body_md, comments_md), assets, "done"


def _unauthorized(j):
    return j.get("code") in ("400", 400) and j.get("msg") == "未授权"
