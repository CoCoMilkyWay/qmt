"""限速 + 重试的 GET 骨架, 与站点无关。无错误处理, 只有断言 — 早失败早好。

抓取结果三态, 所有站点必须一致:
    Response   正常
    MISSING    永久缺失 (帖子/图片真的没了), 重抓也回不来 ⇒ 调用方立墓碑, 是终态
    None       这次没抓到 (网络抖动 / 重试耗尽) ⇒ 调用方作空洞, 留在队列下轮重试

MISSING 与 None 必须分开, 这是硬要求: index 里的是终态不会重抓, 把「这次没抓到」
当成删除会写下一个永远翻不了案的墓碑; 反过来把删除当成没抓到, 那篇会永远卡在队列里。

子类只需给出 _fetch (怎么发这一个 GET) 与 _retry_wait (什么码值得再来一次):
出网方式 (直连/隧道)、频率、并发都是各站自己的事, 不做成运行时开关 — 开局定死、
全程不变, 比「先试直连再降级」好推理得多。
"""

import threading
import time

import requests

MISSING = object()


class Fetcher:
    # qps 卡发牌速率, workers 卡「能藏住多少单请求延迟」, 是两回事。直连站点
    # 一律 workers=1: 本机就一个出口 IP, 并发只会让它更显眼。
    qps = 2
    workers = 1
    timeout = 20
    retries = 3
    backoff = 1.5
    # 哪些状态码算永久缺失。403 是否算取决于出网方式: 走隧道时 403 多半是这个出口
    # IP 撞了风控 (换个 IP 就好), 直连时才是稳定的拒绝。
    missing_status = (404,)

    def __init__(self):
        self._interval = 1.0 / self.qps
        self._lock = threading.Lock()
        self._next = 0.0
        self._local = threading.local()

    def desc(self):
        return f"出网: {self.qps} 次/s {self.workers} 并发"

    def _pace(self):
        """唯一的限速点: 按 qps 均匀发牌, 不攒桶爆发 — 持续超频会被直接拒。"""
        with self._lock:
            slot = max(time.monotonic(), self._next)
            self._next = slot + self._interval
        wait = slot - time.monotonic()
        if wait > 0:
            time.sleep(wait)

    def _fetch(self, url, params):
        """发一个 GET 并返回 Response。子类决定用什么 session / 走不走代理。"""
        raise NotImplementedError

    def _retry_wait(self, attempt, status):
        """→ 重试前等几秒, 或 None 表示这个状态码不值得重试。"""
        if status >= 500 or status == 429:
            return self.backoff**attempt
        return None

    def _check_status(self, r):
        """子类钩子: 遇到「重试也没用的死结」就在这里当场断言。"""

    def get(self, url, params=None):
        last = None
        for attempt in range(self.retries + 1):
            self._pace()
            try:
                r = self._fetch(url, params)
            except requests.RequestException as e:
                last = e
                if attempt < self.retries:
                    time.sleep(self.backoff**attempt)
                continue
            if r.status_code == 200:
                return r
            if r.status_code in self.missing_status:
                return MISSING
            self._check_status(r)
            last = f"HTTP {r.status_code}"
            wait = self._retry_wait(attempt, r.status_code)
            if attempt < self.retries and wait is not None:
                if wait:
                    time.sleep(wait)
                continue
            break
        print(f"  ! {url} 抓不到 ({last}), 重试 {self.retries} 次仍不行", flush=True)
        return None

    def json(self, url, params=None):
        """→ Response.json() | MISSING | None。JSON 解析失败当场炸: 接口变了。"""
        r = self.get(url, params)
        if r is MISSING or r is None:
            return r
        return r.json()
