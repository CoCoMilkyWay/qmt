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

    def _get(self, path, missing_ok=False):
        self._pace()
        url = path if path.startswith("http") else BASE + path
        r = self.s.get(url, timeout=TIMEOUT)
        if missing_ok and r.status_code in (403, 404):
            return None
        assert r.status_code == 200, f"GET {url} -> {r.status_code}"
        return r

    def _json(self, path):
        j = self._get(path).json()
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
        r = self._get(path)
        return r.content, r.headers.get("Content-Type", "")
