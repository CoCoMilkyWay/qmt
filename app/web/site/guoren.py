"""果仁论坛 (guorn.com) 适配器。

接口 (经 forum.js / forumThread.js 确认):
    GET /forum/post/list?page=<n>&tag=<tag>   列表, 返回 total_count/page_size/post_list
    GET /forum/post/<pid>                     正文页 (HTML, 正文在 .post-content, 服务端直出)
    GET /forum/post/detail?pid=<pid>          元信息 (作者/时间/附件/标签)
    GET /forum/comment/list?pid=<pid>&page=n  评论, 每页 20

出网两条路, 由请求类型决定, 全程不变:
    页面 (列表/正文/detail/评论) — 走隧道: 每请求换一个出口 IP, 防本机 IP 被封。
                                  限速由 common/tunnel.acquire() 全局统一发牌 ——
                                  额度是订单的, 果仁与聚宽加起来才 10 次/s。
    图片 — 一律直连: 隧道传二进制大响应会截断 (IncompleteRead), 直连又快
           (0.08s vs 1.65s) 又不占隧道额度。封我们的是果仁论坛, 不是图床。

列表排序键是 max(create_time, last_reply_time) 倒序: 新帖必然在最前面, 老帖被新回复
顶上来也只会往前挪。已入库的帖子视为终态 (不因新回复重抓), 所以增量只需按 pid 去重。
"""

import time
from urllib.parse import urlsplit

import requests
from lxml import html as lxml_html

from ..common import html2md, tunnel
from ..common.assets import collector
from ..common.net import MISSING, Fetcher

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
HOST = "guorn.com"
BASE = f"https://{HOST}"

# 走不走隧道。True = 快代理隧道; False = 本机直连。没做成命令行开关也不做运行时
# 探测: 出网方式开局定死、全程不变, 比「先试直连再降级」好推理得多。
USE_TUNNEL = True

# 图片直连: 快, 也不值多重试 — 抓不到就保留原链, 不卡整篇。
ASSET_TIMEOUT = 10
ASSET_RETRIES = 2
BACKOFF = 1.5


class Client(Fetcher):
    # 隧道偶尔撞上慢出口, 值得等一会儿; 但 30s × 重试 5 次 = 一个 worker 被一个请求
    # 占住 3 分钟, 换 IP 重来比死等划算。
    timeout = 15
    # 只在直连时生效: 走隧道时限速由 tunnel.acquire() 全局管 (见 _pace)。直连的
    # 天花板是「别被风控盯上」而不是吞吐 — 本机 IP 已经被封过一次。
    qps = 2
    # 实测隧道单请求 ~0.78s (直连 ~0.08s), 要把 10 次/s 发满至少得 8 个请求同时在飞。
    # 直连永远是 1: 本机就一个出口 IP, 并发只会让它更显眼。
    workers = 16 if USE_TUNNEL else 1
    # 隧道下重试就是「换一个 IP 再来」, 多给几次; 直连重试换不掉出口 IP, 3 次够了。
    retries = 5 if USE_TUNNEL else 3
    # 403 在隧道下是这个出口 IP 撞了风控, 不是永久缺失: 当成缺失会把好帖子写成墓碑。
    missing_status = (404,)

    def desc(self):
        route = (
            f"隧道 {tunnel.HOST}:{tunnel.PORT} (每请求换 IP) "
            f"{tunnel.QPS} 次/s (订单额度, 全站共享)"
            if USE_TUNNEL
            else f"直连 (本机 IP) {self.qps} 次/s"
        )
        return f"出网: 页面走{route} {self.workers} 并发; 图片直连"

    def _pace(self):
        """走隧道时把限速交给 tunnel: 额度属于订单, 不属于本站。"""
        if USE_TUNNEL:
            tunnel.acquire()
        else:
            super()._pace()

    def _direct_session(self):
        """直连留着 keep-alive; 每个线程一条, requests 的 Session 不保证跨线程安全。"""
        s = getattr(self._local, "s", None)
        if s is None:
            s = requests.Session()
            s.headers["User-Agent"] = UA
            self._local.s = s
        return s

    def _fetch(self, url, params):
        """隧道下每请求一条新连接: 复用连接就换不出新的出口 IP, 重试也就白重试了。"""
        assert urlsplit(url).hostname == HOST, f"只对 {HOST} 出网: {url}"
        if not USE_TUNNEL:
            return self._direct_session().get(url, params=params, timeout=self.timeout)
        with requests.Session() as s:
            s.headers["User-Agent"] = UA
            s.headers["Connection"] = "close"
            s.proxies.update(tunnel.PROXIES)
            return s.get(url, params=params, timeout=self.timeout)

    def _check_status(self, r):
        assert not (USE_TUNNEL and r.status_code in tunnel.FATAL), (
            f"隧道拒绝服务: HTTP {r.status_code} — 账号或配置的问题, 查"
            f" kuaidaili.com/doc/dev/tpshttpresponse"
        )

    def _retry_wait(self, attempt, status):
        # 隧道下状态码也进重试且不必退避: 每请求换 IP ⇒ 重试就是换一个 IP 再来,
        # 撞上被果仁拉黑的出口 (403) 换几次就过。直连只对瞬时故障重试并指数退避:
        # 同一个 IP 上状态码是真实信号。
        if USE_TUNNEL:
            return 0.0
        return super()._retry_wait(attempt, status)

    def asset(self, url):
        """抓一张图, 直连不走隧道。任何 host 都行 — 封我们的是果仁论坛, 不是图床。

        → (bytes, ctype) | (MISSING, "") 图没了 (404), 保留原链 | (None, "") 这次没抓到。
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
            if r.status_code == 404:
                return MISSING, ""
            last = f"HTTP {r.status_code}"
            if attempt < ASSET_RETRIES and r.status_code >= 500:
                time.sleep(BACKOFF**attempt)
                continue
            break
        print(f"  ! {url} 抓不到 ({last}), 试了 {ASSET_RETRIES + 1} 次", flush=True)
        return None, ""


def _entry(p):
    u = p.get("user") or {}
    return {
        # 一律落成字符串: 接口给的 title/tag 可能是 null, 留着 None 会在落盘取名
        # 或截断标题时才炸, 离发现处已经很远了
        "pid": p["pid"],
        "title": p.get("title") or "",
        "create_time": p.get("create_time") or "",
        "tag": p.get("tag") or "",
        "author": u.get("username") or "",
        "uid": u.get("uid"),
        "reply_count": p.get("reply_count", 0),
        "view_count": p.get("view_count", 0),
        "elite": p.get("elite", 0),
        "is_top": bool(p.get("top")),
        "status": "pending",
    }


def _extract_post_content(page_html):
    doc = lxml_html.fromstring(page_html)
    node = doc.find(".//div[@class='post-content']")
    assert node is not None, "未找到 .post-content"
    return (node.text or "") + "".join(
        lxml_html.tostring(c, encoding="unicode") for c in node
    )


def _attachments_md(atts):
    if not atts:
        return ""
    lines = ["## 附件"]
    for a in atts:
        url = a.get("url", "")
        if url.startswith("/"):
            url = BASE + url
        lines.append(f"- [{a.get('name', url)}]({url})")
    return "\n".join(lines) + "\n"


def _assemble(entry, body_md, attach_md, comments_md):
    meta = (
        f"# {entry['title']}\n\n"
        f"> 作者: {entry['author']} (uid={entry['uid']})  \n"
        f"> 时间: {entry['create_time']}  \n"
        f"> 标签: {entry['tag']}  \n"
        f"> 链接: {BASE}/forum/post/{entry['pid']}\n"
    )
    parts = [meta, body_md, attach_md, comments_md]
    return "\n\n".join(p.rstrip() for p in parts if p) + "\n"


def _absolutize(src):
    """→ 绝对 URL, 或 None 表示不必抓。data: 和 file:/// 这种本地伪 URL 直接留着。"""
    if not src or src.startswith("data:") or src.startswith("file:"):
        return None
    return BASE + src if src.startswith("/") else src


class Site:
    name = "guoren"
    default_tags = ["share", "elite"]

    def __init__(self):
        self.fetcher = Client()

    def list_page(self, page, tag):
        """翻页是串行主流程的地基, 抓不到就当场炸: 拿半张列表比不拿更糟。"""
        j = self.fetcher.json(f"{BASE}/forum/post/list", {"page": page, "tag": tag})
        assert (
            j is not MISSING and j is not None
        ), f"列表第 {page} 页 (tag={tag}) 抓不到"
        assert j.get("status") == "ok", f"post/list -> {j.get('status')}"
        data = j["data"]
        return {
            "entries": [_entry(p) for p in data["post_list"]],
            "total_count": data["total_count"],
            "page_size": data["page_size"],
        }

    def _json(self, path, params=None):
        """→ data | MISSING (没了) | None (这次没抓到)。status 不是 ok 就是接口变了。"""
        j = self.fetcher.json(BASE + path, params)
        if j is MISSING or j is None:
            return j
        assert j.get("status") == "ok", f"{path} -> {j.get('status')}"
        return j["data"]

    def _comments_md(self, pid, on_image):
        """→ Markdown | None (评论没抓全, 整篇作空洞: 缺一页评论的半成品不入库)。"""
        page = 1
        cmts = []
        while True:
            data = self._json("/forum/comment/list", {"pid": pid, "page": page})
            if data is None:
                return None
            if data is MISSING:
                break
            cmts.extend(data.get("comment_list", []))
            if len(cmts) >= data.get("total_count", 0) or not data.get("comment_list"):
                break
            page += 1
        if not cmts:
            return ""
        lines = [f"## 评论 ({len(cmts)})"]
        for c in cmts:
            u = c.get("user") or {}
            content = html2md.to_markdown(c.get("content", "") or "", on_image=on_image)
            lines.append(
                f"\n### {u.get('username', '')} · {c.get('create_time', '')}\n"
            )
            lines.append(content.rstrip())
        return "\n".join(lines) + "\n"

    def download_post(self, entry):
        """→ (markdown, assets, status) | None (这次没抓到, 作空洞留在队列下轮补)。

        帖子被删掉 (404) 是终态, 立墓碑 status="deleted"; 只有「这次没抓到」才作空洞。
        抓完不落盘, 字节先攒在内存里, 由主线程串行提交 — 本函数可并发, 不碰 store。
        """
        r = self.fetcher.get(f"{BASE}/forum/post/{entry['pid']}")
        if r is MISSING:
            return _assemble(entry, "_(帖子已删除或不可见)_", "", ""), {}, "deleted"
        if r is None:
            return None

        on_image, assets, holes = collector(self.fetcher.asset, _absolutize)
        body_md = html2md.to_markdown(_extract_post_content(r.text), on_image=on_image)
        if holes:
            return None  # 早退: 已经缺图了, 不必再花 detail 与评论那几个请求

        detail = self._json("/forum/post/detail", {"pid": entry["pid"]})
        if detail is None:
            return None
        attach_md = (
            ""
            if detail is MISSING
            else _attachments_md(detail.get("attachments") or [])
        )
        # reply_count 为 0 就不必再问一趟评论接口
        comments_md = (
            self._comments_md(entry["pid"], on_image)
            if (entry.get("reply_count") or 0) > 0
            else ""
        )
        if comments_md is None or holes:
            return None
        return _assemble(entry, body_md, attach_md, comments_md), assets, "done"
