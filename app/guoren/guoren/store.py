"""本地缓存落盘。一个 store 根目录下: index.jsonl + pending.jsonl + 逐篇原子提交。

布局 (对齐 gzh 的 dated 风格):
    store/
    ├── index.jsonl          已入库, 一行一篇; 追加这一行是提交的分界点
    ├── pending.jsonl        已发现未入库的队列, 追加式 (发现一页写一页)
    ├── state.json           {deep_done, total_count} — 是否已完整翻过一遍列表
    ├── <date>_<title>_<pid>/
    │   ├── article.md
    │   └── assets/NNN.ext
    └── .staging/            正在下的那篇; 开局无条件清掉

原子性 (随时可 kill, 下次接着跑):
    - 一篇文章先写满 .staging/<pid>/ 再 rename 到最终目录, 最后追加 index 行。
      index 行落盘才算入库; 卡在 rename 与 append 之间留下的目录 = 孤儿,
      开局按「盘上有目录但 index 里没有」识别并删掉, 重新下一遍。
    - pending 只追加不覆盖: 发现一页立刻落盘, kill 最多丢当前这一页;
      已入库的行由 compact_pending 事后清掉, 期间读队列时按 index 过滤。
"""

import json
import os
import re
import shutil

INDEX = "index.jsonl"
PENDING = "pending.jsonl"
STATE = "state.json"
STAGING = ".staging"

_BAD_CHARS = re.compile(r'[/\\:*?"<>|\r\n\t]')


def _safe_name(s):
    s = _BAD_CHARS.sub("_", s).strip().strip(".")
    return s[:80] or "_"


def _entry_dir(entry):
    date = (entry["create_time"] or "0000-00-00")[:10]
    return f"{date}_{_safe_name(entry['title'])}_{entry['pid']}"


def _pid_of_dir(name):
    """目录名末段就是 pid (pid 形如 p.123.456, 不含下划线, 反推是安全的)。"""
    parts = name.rsplit("_", 1)
    return parts[1] if len(parts) == 2 else None


def _read_jsonl(path):
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [json.loads(ln) for ln in f if ln.strip()]


def _write_jsonl(path, rows):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        for r in rows:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    os.replace(tmp, path)


def _fsync_dir(path):
    fd = os.open(path, os.O_RDONLY)
    os.fsync(fd)
    os.close(fd)


def _append_line(path, row):
    with open(path, "a", encoding="utf-8") as f:
        f.write(json.dumps(row, ensure_ascii=False) + "\n")
        f.flush()
        os.fsync(f.fileno())


class Store:
    def __init__(self, root):
        self.root = root
        os.makedirs(root, exist_ok=True)
        self.index_path = os.path.join(root, INDEX)
        self.pending_path = os.path.join(root, PENDING)
        self.state_path = os.path.join(root, STATE)
        self.staging = os.path.join(root, STAGING)

        self.index = {r["pid"]: r for r in _read_jsonl(self.index_path)}
        self._pending = [
            r for r in _read_jsonl(self.pending_path) if r["pid"] not in self.index
        ]
        # 队列里同一 pid 只留一份: 上一趟被 kill 可能让同一页重复追加过
        self._pending_pids = set()
        deduped = []
        for r in self._pending:
            if r["pid"] in self._pending_pids:
                continue
            self._pending_pids.add(r["pid"])
            deduped.append(r)
        self._pending = deduped

        self._reset_staging()
        self._gc_orphans()

    def _reset_staging(self):
        if os.path.isdir(self.staging):
            shutil.rmtree(self.staging)
        os.makedirs(self.staging, exist_ok=True)

    def _gc_orphans(self):
        """删掉「盘上有目录但 index 里没有」的孤儿 — rename 成功而 index 行未落盘的残留。"""
        n = 0
        for name in os.listdir(self.root):
            path = os.path.join(self.root, name)
            if not os.path.isdir(path) or name == STAGING:
                continue
            pid = _pid_of_dir(name)
            if pid is not None and pid in self.index:
                continue
            shutil.rmtree(path)
            n += 1
        if n:
            print(f"清理 {n} 个未提交的残留目录", flush=True)

    def has(self, pid):
        return pid in self.index

    def seen_pids(self):
        """已入库 + 已发现待下 = 所有见过的 pid, 用于判断列表是否已翻齐。"""
        return set(self.index) | self._pending_pids

    def add_pending(self, rows):
        """追加新发现的帖子到队列, 发现一页就落盘一页。返回真正新增的条数。"""
        fresh = [
            r
            for r in rows
            if r["pid"] not in self.index and r["pid"] not in self._pending_pids
        ]
        if not fresh:
            return 0
        with open(self.pending_path, "a", encoding="utf-8") as f:
            for r in fresh:
                f.write(json.dumps(r, ensure_ascii=False) + "\n")
            f.flush()
            os.fsync(f.fileno())
        for r in fresh:
            self._pending_pids.add(r["pid"])
            self._pending.append(r)
        return len(fresh)

    def pending(self):
        return [r for r in self._pending if r["pid"] not in self.index]

    def compact_pending(self):
        """把已入库的行从队列文件里剔掉。纯优化, 中途 kill 也不影响正确性。"""
        rows = self.pending()
        self._pending = rows
        self._pending_pids = {r["pid"] for r in rows}
        _write_jsonl(self.pending_path, rows)

    def state(self):
        if not os.path.exists(self.state_path):
            return {"deep_done": False, "total_count": 0}
        with open(self.state_path, encoding="utf-8") as f:
            return json.load(f)

    def set_state(self, **kw):
        st = self.state()
        st.update(kw)
        tmp = self.state_path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(st, f, ensure_ascii=False, indent=2)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, self.state_path)

    def commit(self, entry, markdown, assets):
        pid = entry["pid"]
        assert pid not in self.index, f"重复入库 {pid}"
        assert entry.get("status"), f"入库必带状态 {pid}"

        final_dir = os.path.join(self.root, _entry_dir(entry))
        # pid 不在 index 却已有目录 = 上一趟卡在 rename 与 index 追加之间的残留。
        # 开局的 _gc_orphans 已扫过一遍, 这里兜住同一趟内重试的情况。
        if os.path.exists(final_dir):
            shutil.rmtree(final_dir)

        stage = os.path.join(self.staging, pid)
        if os.path.isdir(stage):
            shutil.rmtree(stage)
        os.makedirs(stage)
        with open(os.path.join(stage, "article.md"), "w", encoding="utf-8") as f:
            f.write(markdown)
            f.flush()
            os.fsync(f.fileno())
        if assets:
            adir = os.path.join(stage, "assets")
            os.makedirs(adir, exist_ok=True)
            for name, data in assets.items():
                with open(os.path.join(adir, name), "wb") as f:
                    f.write(data)
                    f.flush()
                    os.fsync(f.fileno())

        _fsync_dir(stage)
        os.replace(stage, final_dir)
        _fsync_dir(self.root)
        entry = dict(entry, dir=os.path.basename(final_dir))
        _append_line(self.index_path, entry)  # 这一行落盘才算入库
        self.index[pid] = entry
        self._pending_pids.discard(pid)
