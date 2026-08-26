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

出网走隧道 (与果仁同一个订单): 页面请求每次换出口 IP, 图片直连。限速不在本文件,
由 common/tunnel.acquire() 全局统一发牌 —— 额度是订单的, 两个站加起来才 10 次/s。
"""

import os
import time

import requests

from ..common import mdimg, tunnel
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

# 走隧道换 IP, 与果仁同一个订单。直连本机 IP 在 10 次/s 下迟早被风控盯上 (果仁已被封
# 过一次)。出网方式开局定死、全程不变。
USE_TUNNEL = True

# 图片直连不走隧道: 隧道传二进制大响应会截断 (IncompleteRead), 这是果仁踩过的坑,
# 跟站点无关。聚宽图床在 cdn.joinquant.com, 直连没被封过也不值得为它占隧道额度。
ASSET_TIMEOUT = 15
ASSET_RETRIES = 2
BACKOFF = 1.5


class Client(Fetcher):
    timeout = 40
    # 只在直连时生效: 走隧道时限速由 tunnel.acquire() 全局管 (见 _pace)。直连的
    # 天花板是「别被风控盯上」而不是吞吐。
    qps = 3
    # 隧道单请求实测 2~4s (换 IP + 转发), 聚宽一篇还要连发 detail + 多页评论。要把
    # 10 次/s 发满, 同时在飞的请求得有 qps × 延迟 ≈ 30 个, 所以给 30。
    # 直连时本机就一个出口 IP, 并发只会更显眼, 老老实实 1。
    workers = 30 if USE_TUNNEL else 1
    # 隧道下重试就是换一个 IP 再来, 多给几次; 直连换不掉 IP, 3 次够。
    retries = 5 if USE_TUNNEL else 3
    # 隧道下 403 多半是这个出口 IP 撞了风控, 换 IP 就好, 不能当永久缺失 (否则好帖子
    # 被写成墓碑)。直连时 403 才是稳定的拒绝 (图床防盗链)。
    missing_status = (404,) if USE_TUNNEL else (403, 404)

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

    def _set_headers(self, s):
        s.headers["User-Agent"] = UA
        s.headers["X-Requested-With"] = "XMLHttpRequest"
        s.headers["Accept"] = "application/json, text/plain, */*"
        s.headers["Referer"] = BASE + "/view/community/list"
        for k, v in self._cookies.items():
            s.cookies.set(k, v, domain="www.joinquant.com")

    def desc(self):
        route = (
            f"隧道 {tunnel.HOST}:{tunnel.PORT} (每请求换 IP) "
            f"{tunnel.QPS} 次/s (订单额度, 全站共享)"
            if USE_TUNNEL
            else f"直连 (本机 IP) {self.qps} 次/s"
        )
        return (
            f"出网: {route} {self.workers} 并发; "
            f"已载入 {len(self._cookies)} 条 cookie"
        )

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
            self._set_headers(s)
            self._local.s = s
        return s

    def _fetch(self, url, params):
        # 隧道下每请求一条新连接: 复用连接就换不出新的出口 IP, 重试也就白重试了;
        # 顺便每请求独立 session, 30 个 worker 各发各的, 不碰同一个 Session 对象。
        # cookie 在 _cookies 里, 每条新 session 都灌一遍, 跟连接无关。
        if not USE_TUNNEL:
            return self._direct_session().get(url, params=params, timeout=self.timeout)
        with requests.Session() as s:
            self._set_headers(s)
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
        # 撞上被风控的出口 (403) 换几次就过。直连只对瞬时故障重试并指数退避。
        if USE_TUNNEL:
            return 0.0
        return super()._retry_wait(attempt, status)

    def asset(self, url):
        """→ (bytes, ctype) | (MISSING, "") 图没了 | (None, "") 这次没抓到。

        图片直连不走隧道 (见上面的 ASSET 注释), 也不走 _pace 限速: 图床不是论坛,
        不会因为并发盯上本机 IP。抓不到就保留原链, 不卡整篇。
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
