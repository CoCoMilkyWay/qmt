"""果仁论坛 HTTP 封装。无错误处理，只有断言 — 早失败早好。

接口 (经 forum.js / forumThread.js 确认):
    GET /forum/post/list?page=<n>&tag=<tag>   列表, 返回 total_count/page_size/post_list
    GET /forum/post/<pid>                     正文页 (HTML, 正文在 .post-content, 服务端直出)
    GET /forum/post/detail?pid=<pid>          元信息 (作者/时间/附件/标签)
    GET /forum/comment/list?pid=<pid>&page=n  评论, 每页 20
"""

import time

import requests

UA = (
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
)
BASE = "https://guorn.com"
TIMEOUT = 30
# 瞬时故障 (网络抖动 / 5xx / 429) 重试次数; 404/403 是永久缺失, 不重试。
RETRIES = 3
BACKOFF = 1.5


class Client:
    def __init__(self, delay=0.3):
        self.s = requests.Session()
        self.s.headers["User-Agent"] = UA
        self.delay = delay
        self._last = 0.0

    def _pace(self):
        dt = time.time() - self._last
        if dt < self.delay:
            time.sleep(self.delay - dt)
        self._last = time.time()

    def _get(self, path, missing_ok=False, soft=False):
        """发一个 GET。瞬时故障重试 RETRIES 次。

        missing_ok: 404/403 视作永久缺失, 返回 None (不重试)。
        soft:        重试耗尽后仍失败时返回 None, 而不是断言炸掉 — 图片下载用,
                     一张图抓不到不能让整篇文章卡住。
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

    def _json(self, path):
        r = self._get(path)
        j = r.json()
        assert j.get("status") == "ok", f"{path} -> {j.get('status')}"
        return j["data"]

    def list_page(self, page, tag="all"):
        return self._json(f"/forum/post/list?page={page}&tag={tag}")

    def post_html(self, pid):
        """已删除/不可见的帖子返回 None, 由调用方立墓碑, 不能卡住流水线。"""
        r = self._get(f"/forum/post/{pid}", missing_ok=True)
        return r.text if r is not None else None

    def post_detail(self, pid):
        return self._json(f"/forum/post/detail?pid={pid}")

    def comments(self, pid, page=1):
        return self._json(f"/forum/comment/list?pid={pid}&page={page}")

    def asset(self, path):
        """抓图片。死链或重试耗尽都返回 None, 由调用方保留原 URL, 不卡流水线。"""
        r = self._get(path, missing_ok=True, soft=True)
        if r is None:
            return None, ""
        return r.content, r.headers.get("Content-Type", "")
