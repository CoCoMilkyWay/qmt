"""限速 + 重试的 GET 骨架, 与站点无关。无错误处理, 只有断言 — 早失败早好。

抓取结果三态, 所有站点必须一致:
    Response   正常
    MISSING    永久缺失 (帖子/图片真的没了), 重抓也回不来 ⇒ 调用方立墓碑, 是终态
    None       这次没抓到 (网络抖动 / 重试耗尽) ⇒ 调用方作空洞, 留在队列下轮重试

MISSING 与 None 必须分开, 这是硬要求: index 里的是终态不会重抓, 把「这次没抓到」
当成删除会写下一个永远翻不了案的墓碑; 反过来把删除当成没抓到, 那篇会永远卡在队列里。

出网选路在这里定死, 所有站点一律遵守, 站点不得自行决定走哪条路:
    列表翻页 (list_json)          — 直连。列表是主流程的地基, 抓不到就当场断言、整个
        run 都得停, 不能把它架在会截断响应的隧道上 (实测隧道会把 8KB 的列表 JSON 截成
        IncompleteRead)。而列表全站也就几百页、增量两三页, 直连的曝光量可忽略。
    文章正文与评论 (post_get / post_json) — 走隧道, 每请求换一个出口 IP。几万篇 × 每篇
        正文加多页评论, 量大到会招风控, 这才是必须换 IP 的部分。抓不到只是这一篇变空洞,
        下轮重试, 不会停掉整个 run。
    图片 — 直连, 见各站 asset(): 隧道传二进制大响应同样会截断, 而封我们的是论坛不是图床。

登录态只装在直连这条路上, 走隧道的请求一律匿名 (见 _auth)。登录是为了翻过列表的深页墙
(聚宽匿名只能看 10 页), 读单篇文章不需要它 —— 实测匿名与带 cookie 拿到的正文和评论完全
一致。而带着同一套 cookie 满天换出口 IP, 在风控眼里比固定 IP 更像盗号: 隧道换 IP 换来的
匿名性, 会被一个固定的会话身份全部抵消。所以这条是规矩, 不是某个站的特例。

限速跟着出口走, 不跟着站点走: 走隧道的请求由 tunnel.acquire() 按订单额度全局发牌
(果仁和聚宽同一个订单, 各站按自己的 qps 发牌会加起来超频); 只有直连才用本站的 qps。

子类只需给出 _headers (公共头) 与 _auth (登录态), 外加站点常量 (host / qps / workers /
retries)。出网方式不做成运行时开关 — 开局定死、全程不变, 比「先试直连再降级」好推理。
"""

import threading
import time
from urllib.parse import urlsplit

import requests

from . import tunnel

MISSING = object()


class Fetcher:
    # 只出网到这个域名 (图片除外, 走 asset)。写死是为了不会因为某个链接被改写而
    # 悄悄把请求发到别处去。
    host = None
    # 只卡直连的那部分: 走隧道的请求由 tunnel.acquire() 按订单额度发牌。直连的
    # 天花板是「别被风控盯上」而不是吞吐。
    qps = 2
    # workers 卡「能藏住多少单请求延迟」, 与 qps 是两回事: 要把额度发满, 同时在飞的
    # 请求得有 qps × 单请求延迟 个。全直连的站点一律 1 — 本机就一个出口 IP。
    workers = 1
    # 图片阶段的并发, 与 workers 分开 (见 sync.drain 的两级流水线): 图片直连、不限速,
    # 慢的是对方图床, 多开几条同时抓就好, 但绝不能占 workers 的槽。
    asset_workers = 16
    timeout = 20
    retries = 3
    backoff = 1.5
    # 哪些状态码算永久缺失。403 不算: 走隧道时它多半是这个出口 IP 撞了风控, 换个 IP
    # 就好, 当成缺失会把好帖子写成永远翻不了案的墓碑。
    missing_status = (404,)

    def __init__(self):
        self._interval = 1.0 / self.qps
        self._lock = threading.Lock()
        self._next = 0.0
        self._local = threading.local()

    def desc(self):
        route = (
            f"隧道 {tunnel.HOST}:{tunnel.PORT} (每请求换 IP) "
            f"{tunnel.QPS} 次/s (订单额度, 全站共享)"
            if tunnel.ENABLED
            else "直连 (本机 IP)"
        )
        return (
            f"出网: 文章走{route} {self.workers} 并发; "
            f"列表直连 {self.qps} 次/s; 图片直连不限速"
        )

    # ---- 三种请求, 出网走哪条路由方法名定死, 站点只管挑对方法 ----

    def list_json(self, url, params=None):
        """列表翻页, 直连。→ dict | MISSING | None"""
        return self._json(url, params, tunneled=False)

    def post_get(self, url, params=None):
        """文章正文/评论, 走隧道。→ Response | MISSING | None"""
        return self._get(url, params, tunneled=True)

    def post_json(self, url, params=None):
        """文章正文/评论, 走隧道。→ dict | MISSING | None"""
        return self._json(url, params, tunneled=True)

    # ---- 以下是两条路的实现, 站点不必也不该覆盖 ----

    def _headers(self, s):
        """子类钩子: 往这条 session 上装站点公共头 (UA 之类), 两条路都装。"""

    def _auth(self, s):
        """子类钩子: 往这条 session 上装登录态 (cookie), 只有直连那条路装。

        走隧道的请求一律匿名 —— 理由见模块头: 登录只为翻列表深页, 读文章不需要, 而
        固定会话 + 轮换 IP 比固定 IP 更招风控。
        """

    def _direct_session(self):
        """直连留着 keep-alive; 每个线程一条, requests 的 Session 不保证跨线程安全。"""
        s = getattr(self._local, "s", None)
        if s is None:
            s = requests.Session()
            self._headers(s)
            self._auth(s)
            self._local.s = s
        return s

    def _tunneled(self, tunneled):
        return tunneled and tunnel.ENABLED

    def _pace(self, tunneled):
        """每个请求 (含重试) 出网前的唯一限速点, 不攒桶爆发 — 爆发是隧道最不能忍的。

        额度属于出口而不是站点: 走隧道就去 tunnel 那个进程级闸领牌 (整个订单共享),
        直连才按本站 qps 发牌。
        """
        if self._tunneled(tunneled):
            tunnel.acquire()
            return
        with self._lock:
            slot = max(time.monotonic(), self._next)
            self._next = slot + self._interval
        wait = slot - time.monotonic()
        if wait > 0:
            time.sleep(wait)

    def _fetch(self, url, params, tunneled):
        assert (
            self.host is None or urlsplit(url).hostname == self.host
        ), f"只对 {self.host} 出网: {url}"
        if not self._tunneled(tunneled):
            return self._direct_session().get(url, params=params, timeout=self.timeout)
        # 隧道下每请求一条新连接: 复用连接就换不出新的出口 IP, 重试也就白重试了;
        # 顺便每请求一条独立 session, 几十个 worker 各发各的, 不碰同一个 Session。
        # 这里刻意不调 _auth: 走隧道就得是干干净净的匿名请求。
        with requests.Session() as s:
            self._headers(s)
            s.headers["Connection"] = "close"
            s.proxies.update(tunnel.PROXIES)
            return s.get(url, params=params, timeout=self.timeout)

    def _retry_wait(self, attempt, status, tunneled):
        """→ 重试前等几秒, 或 None 表示这个状态码不值得重试。

        走隧道时状态码一概重试且不必退避: 每请求换 IP ⇒ 重试就是换一个出口再来,
        撞上被风控的出口 (403) 换几次就过。直连换不掉 IP, 状态码是真实信号, 只对
        瞬时故障重试并指数退避。
        """
        if self._tunneled(tunneled):
            return 0.0
        if status >= 500 or status == 429:
            return self.backoff**attempt
        return None

    def _get(self, url, params, tunneled):
        last = None
        for attempt in range(self.retries + 1):
            self._pace(tunneled)
            try:
                r = self._fetch(url, params, tunneled)
            except requests.RequestException as e:
                last = e
                if attempt < self.retries:
                    time.sleep(self.backoff**attempt)
                continue
            if r.status_code == 200:
                return r
            if r.status_code in self.missing_status:
                return MISSING
            assert not (self._tunneled(tunneled) and r.status_code in tunnel.FATAL), (
                f"隧道拒绝服务: HTTP {r.status_code} — 账号或配置的问题, 查"
                f" kuaidaili.com/doc/dev/tpshttpresponse"
            )
            last = f"HTTP {r.status_code}"
            wait = self._retry_wait(attempt, r.status_code, tunneled)
            if attempt < self.retries and wait is not None:
                if wait:
                    time.sleep(wait)
                continue
            break
        print(f"  ! {url} 抓不到 ({last}), 重试 {self.retries} 次仍不行", flush=True)
        return None

    def _json(self, url, params, tunneled):
        """→ Response.json() | MISSING | None。JSON 解析失败当场炸: 接口变了。"""
        r = self._get(url, params, tunneled)
        if r is MISSING or r is None:
            return r
        return r.json()
