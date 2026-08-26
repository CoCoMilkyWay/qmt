"""果仁论坛 HTTP 封装。无错误处理，只有断言 — 早失败早好。

接口 (经 forum.js / forumThread.js 确认):
    GET /forum/post/list?page=<n>&tag=<tag>   列表, 返回 total_count/page_size/post_list
    GET /forum/post/<pid>                     正文页 (HTML, 正文在 .post-content, 服务端直出)
    GET /forum/post/detail?pid=<pid>          元信息 (作者/时间/附件/标签)
    GET /forum/comment/list?pid=<pid>&page=n  评论, 每页 20

出网两条路, 由请求类型决定, 全程不变:
    页面 (列表/正文/detail/评论) — 走隧道: 每请求换一个出口 IP, 防本机 IP 被封。
    图片 — 一律直连: 隧道传二进制大响应会截断 (IncompleteRead), 直连又快
           (0.08s vs 1.65s) 又不占隧道额度。封我们的是果仁论坛, 不是图床。
"""

import threading
import time
from urllib.parse import urlsplit

import requests

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
HOST = "guorn.com"
BASE = f"https://{HOST}"

# 隧道偶尔撞上慢出口, 值得等一会儿; 但 30s × 重试 5 次 = 一个 worker 被一个请求
# 占住 3 分钟, 换 IP 重来比死等划算。
TIMEOUT = 15

# 走不走隧道。True = 快代理隧道; False = 本机直连。没做成命令行开关也不做运行时
# 探测: 出网方式开局定死、全程不变, 比「先试直连再降级」好推理得多。
USE_TUNNEL = True

# 快代理隧道 (按量付费, 每次请求换 IP), 与 gzh/src/config.hpp 是同一个订单。
# 主入口不通就换备用 i970.kdltps.com, 端口相同。
TUNNEL_HOST = "i969.kdltps.com"
TUNNEL_PORT = 15818
TUNNEL_USER = "t18771545382113"
TUNNEL_PASS = "tlh1s2tx"

# 两种出网方式的频率都只用「每秒多少个请求」描述, _pace() 是唯一的限速点。
# 隧道: 订单买的就是 10 次/s, 超了返回 441。
# 直连: 天花板是「别被风控盯上」而不是吞吐 — 本机 IP 已经被封过一次。
TUNNEL_QPS = 10
DIRECT_QPS = 2

# workers 卡的是「能藏住多少单请求延迟」, 与 qps 卡的发牌速率是两回事。
# 实测隧道单请求 ~0.78s (直连 ~0.08s), 要把 10 次/s 发满至少得 8 个请求同时在飞。
# 直连永远是 1: 本机就一个出口 IP, 并发只会让它更显眼。
TUNNEL_WORKERS = 16
DIRECT_WORKERS = 1

_TUNNEL_URL = f"http://{TUNNEL_USER}:{TUNNEL_PASS}@{TUNNEL_HOST}:{TUNNEL_PORT}"
PROXIES = {"http": _TUNNEL_URL, "https": _TUNNEL_URL} if USE_TUNNEL else {}

# 隧道下重试就是「换一个 IP 再来」, 多给几次; 直连重试换不掉出口 IP, 3 次够了。
RETRIES = 5 if USE_TUNNEL else 3
BACKOFF = 1.5

# 图片直连: 快, 也不值多重试 — 抓不到就保留原链, 不卡整篇。
ASSET_TIMEOUT = 10
ASSET_RETRIES = 2

# 404 = 帖子/图片真的没了, 重抓也回不来。必须与「这次没抓到」分开: 403 在隧道下
# 是这个出口 IP 撞了风控, 当成永久缺失会把好帖子写成墓碑, 而 index 里的是终态。
MISSING = object()

# 快代理自己给的码 (官方表 kuaidaili.com/doc/dev/tpshttpresponse), 不是目标站的
# 响应。既然只拿隧道访问 guorn.com 一个域名, 这些码全是死结 —— 账号密钥、实名、
# 白名单、或者这个域名被隧道禁掉 —— 换 IP 重试也一样, 当场炸掉比默默重试 5 次强。
# 剩下的 440/441 (带宽/超频) 与 515-517 (代理抖动) 才是瞬时的, 换个 IP 再来。
TUNNEL_FATAL = {
    407,
    442,
    443,
    445,
    446,
    447,
    448,
    449,
    450,
    452,
    453,
    454,
    455,
    460,
    466,
}


class Client:
    def __init__(self):
        self.qps = TUNNEL_QPS if USE_TUNNEL else DIRECT_QPS
        self.workers = TUNNEL_WORKERS if USE_TUNNEL else DIRECT_WORKERS
        self._interval = 1.0 / self.qps
        self._lock = threading.Lock()
        self._next = 0.0
        self._local = threading.local()

    def desc(self):
        route = (
            f"隧道 {TUNNEL_HOST}:{TUNNEL_PORT} (每请求换 IP)"
            if USE_TUNNEL
            else "直连 (本机 IP)"
        )
        return f"出网: 页面走{route} {self.qps} 次/s {self.workers} 并发; 图片直连"

    def _pace(self):
        """唯一的限速点: 按 qps 均匀发牌, 不攒桶爆发 — 隧道对持续超频直接拒 441。"""
        with self._lock:
            slot = max(time.monotonic(), self._next)
            self._next = slot + self._interval
        wait = slot - time.monotonic()
        if wait > 0:
            time.sleep(wait)

    def _direct_session(self):
        """直连留着 keep-alive; 每个线程一条, requests 的 Session 不保证跨线程安全。"""
        s = getattr(self._local, "s", None)
        if s is None:
            s = requests.Session()
            s.headers["User-Agent"] = UA
            self._local.s = s
        return s

    def _fetch(self, url):
        """隧道下每请求一条新连接: 复用连接就换不出新的出口 IP, 重试也就白重试了。"""
        if not USE_TUNNEL:
            return self._direct_session().get(url, timeout=TIMEOUT)
        with requests.Session() as s:
            s.headers["User-Agent"] = UA
            s.headers["Connection"] = "close"
            s.proxies.update(PROXIES)
            return s.get(url, timeout=TIMEOUT)

    def _get(self, path):
        """→ Response | MISSING (404, 永久缺失) | None (这次没抓到, 调用方作空洞)。

        隧道下状态码也进重试: 每请求换 IP ⇒ 重试就是换一个 IP 再来, 撞上被果仁
        拉黑的出口 (403) 或隧道自己抖动 (5xx/超时) 换几次就过, 也不必退避。
        直连只在没拿到响应时重试且指数退避: 同一个 IP 上状态码是真实信号。
        """
        url = path if path.startswith("http") else BASE + path
        assert urlsplit(url).hostname == HOST, f"只对 {HOST} 出网: {url}"
        last = None
        for attempt in range(RETRIES + 1):
            self._pace()
            try:
                r = self._fetch(url)
            except requests.RequestException as e:
                last = e
                if not USE_TUNNEL and attempt < RETRIES:
                    time.sleep(BACKOFF**attempt)
                continue
            if r.status_code == 200:
                return r
            if r.status_code == 404:
                return MISSING
            assert not (USE_TUNNEL and r.status_code in TUNNEL_FATAL), (
                f"隧道拒绝服务: HTTP {r.status_code} — 账号或配置的问题, 查"
                f" kuaidaili.com/doc/dev/tpshttpresponse ({url})"
            )
            last = f"HTTP {r.status_code}"
            if USE_TUNNEL:
                continue  # 换一个出口 IP 再来
            if attempt < RETRIES and (r.status_code >= 500 or r.status_code == 429):
                time.sleep(BACKOFF**attempt)
                continue
            break
        print(f"  ! {url} 抓不到 ({last}), 重试 {RETRIES} 次仍不行", flush=True)
        return None

    def _json(self, path):
        """→ data | None (这次没抓到)。status 不是 ok 就是接口变了, 当场断言。"""
        r = self._get(path)
        if r is None:
            return None
        assert r is not MISSING, f"GET {path} -> 404"
        j = r.json()
        assert j.get("status") == "ok", f"{path} -> {j.get('status')}"
        return j["data"]

    def list_page(self, page, tag="all"):
        """翻页是串行主流程的地基, 抓不到就当场炸: 拿半张列表比不拿更糟。"""
        data = self._json(f"/forum/post/list?page={page}&tag={tag}")
        assert data is not None, f"列表第 {page} 页 (tag={tag}) 抓不到"
        return data

    def post_html(self, pid):
        """→ HTML | MISSING (帖子没了, 调用方立墓碑) | None (这次没抓到, 作空洞)。"""
        r = self._get(f"/forum/post/{pid}")
        return r if r is MISSING or r is None else r.text

    def post_detail(self, pid):
        return self._json(f"/forum/post/detail?pid={pid}")

    def comments(self, pid, page=1):
        return self._json(f"/forum/comment/list?pid={pid}&page={page}")

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
