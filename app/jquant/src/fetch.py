"""聚宽社区 HTTP 封装。无错误处理，只有断言 — 早失败早好。

接口 (经 playwright 抓包确认):
    GET /community/post/listV2?limit=N&page=N&cate=N&type=isNewPublish
        列表, 返回 data.list / data.totalCount / data.topCount
    GET /community/post/detailV2?postId=<id>
        详情, data.content 直接是 markdown 正文
    GET /community/post/replyList?page=N&postId=<id>
        评论, 每页 20, 返回 data.replyArr / data.totalCount
公共头: X-Requested-With: XMLHttpRequest
"""

import time

import requests

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
BASE = "https://www.joinquant.com"
TIMEOUT = 40
RETRIES = 3
BACKOFF = 1.5


class Client:
    def __init__(self, delay=0.3):
        self.s = requests.Session()
        self.s.headers["User-Agent"] = UA
        self.s.headers["X-Requested-With"] = "XMLHttpRequest"
        self.s.headers["Accept"] = "application/json, text/plain, */*"
        self.s.headers["Referer"] = BASE + "/view/community/list"
        self.delay = delay
        self._last = 0.0

    def _pace(self):
        dt = time.time() - self._last
        if dt < self.delay:
            time.sleep(self.delay - dt)
        self._last = time.time()

    def _get(self, path, missing_ok=False, soft=False):
        """瞬时故障 (网络抖动 / 5xx / 429) 重试 RETRIES 次。

        missing_ok: 404/403 视作永久缺失, 返回 None (不重试)。
        soft:        重试耗尽后仍失败时返回 None — 图片下载用, 一张图不能卡整篇。
        """
        url = path if path.startswith("http") else BASE + path
        last = None
        for attempt in range(RETRIES + 1):
            self._pace()
            try:
                r = self.s.get(url, timeout=TIMEOUT)
            except requests.RequestException as e:
                last = e
                if attempt < RETRIES:
                    time.sleep(BACKOFF ** attempt)
                continue
            if r.status_code in (403, 404) and missing_ok:
                return None
            if r.status_code == 200:
                return r
            last = f"HTTP {r.status_code}"
            if attempt < RETRIES and (r.status_code >= 500 or r.status_code == 429):
                time.sleep(BACKOFF ** attempt)
                continue
            break
        if soft:
            return None
        assert False, f"GET {url} 失败 ({last}), 重试 {RETRIES} 次仍不行"

    def list_page(self, page, cate=3, limit=200):
        r = self._get_raw("/community/post/listV2",
                          {"limit": limit, "page": page, "cate": cate, "type": "isNewPublish"})
        j = r.json()
        assert j.get("status") == "ok" or j.get("code") == "00000", f"listV2 -> {j.get('code')} {j.get('msg')}"
        return j["data"]

    def detail(self, post_id):
        r = self._get_raw("/community/post/detailV2", {"postId": post_id})
        j = r.json()
        if j.get("code") != "00000":
            return None  # 帖子已删除/不可见: 立墓碑, 不卡流水线
        return j["data"]

    def replies(self, post_id, page=1):
        r = self._get_raw("/community/post/replyList", {"page": page, "postId": post_id})
        j = r.json()
        if j.get("code") != "00000":
            return None
        return j["data"]

    def asset(self, url):
        """抓图片。死链或重试耗尽都返回 None, 由调用方保留原 URL。"""
        r = self._get(url, missing_ok=True, soft=True)
        if r is None:
            return None, ""
        return r.content, r.headers.get("Content-Type", "")

    def _get_raw(self, path, params):
        """带 query 参数的 GET, 走与 _get 相同的节奏/重试, 但要拼 params。"""
        url = BASE + path
        last = None
        for attempt in range(RETRIES + 1):
            self._pace()
            try:
                r = self.s.get(url, params=params, timeout=TIMEOUT)
            except requests.RequestException as e:
                last = e
                if attempt < RETRIES:
                    time.sleep(BACKOFF ** attempt)
                continue
            if r.status_code == 200:
                return r
            last = f"HTTP {r.status_code}"
            if attempt < RETRIES and (r.status_code >= 500 or r.status_code == 429):
                time.sleep(BACKOFF ** attempt)
                continue
            break
        assert False, f"GET {url} {params} 失败 ({last}), 重试 {RETRIES} 次仍不行"
