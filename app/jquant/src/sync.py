"""主 flow: discover (翻页发现) → drain (逐篇下载入库)。

聚宽列表按 addTime 倒序 (isNewPublish), 排序稳定 — 新帖只往前插, 老帖不动。
所以不需要果仁那套锚点漂移逻辑: 深翻就是从第 1 页往后翻到最旧, 增量就是从第 1 页
往后翻到连续 STOP_PAGES 页全已知。

平台硬限制: page<=10 (第 11 页起返回空 list)。代码按「假设能玩」写成翻到 total_pages,
遇到空页就停 — 真到不了底时 deep_done 不置位, 下趟接着从 deep_page+1 重试那堵墙,
一旦接口放开就能自然续上。
"""

import math

from . import md as md_mod
from .fetch import Client
from .store import Store

PAGE_SIZE = 200
STOP_PAGES = 2


def _entry_from_list(p, cate):
    u = p.get("user") or {}
    return {
        "pid": p["postId"],
        "title": p.get("title", "") or "",
        "create_time": p.get("addTime", "") or "",
        "cate": cate,
        "author": u.get("alias", "") or "",
        "uid": u.get("userId"),
        "reply_count": int(p.get("replyCount") or 0),
        "view_count": int(p.get("viewCount") or 0),
        "is_top": p.get("isTop") == "1",
        "status": "pending",
    }


def _total_pages(data):
    total = int(data.get("totalCount") or 0)
    return math.ceil(total / PAGE_SIZE) if total else 0


def discover(client, store, cate, max_pages=None, deep=None):
    tag = str(cate)
    forced = deep is True
    if deep is None:
        deep = not store.state(tag).get("deep_done")
    added = (
        _discover_deep(client, store, cate, max_pages, restart=forced)
        if deep
        else _discover_shallow(client, store, cate, max_pages)
    )
    st = store.state(tag)
    total_count = int(st.get("total_count") or 0)
    seen = len(store.seen_pids())
    if total_count and seen < total_count:
        print(
            f"注意: 已见 {seen} / 列表报 {total_count} 帖 (差 {total_count - seen}), "
            f"可跑 --deep 对账",
            flush=True,
        )
    return added


def _absorb(store, posts, cate):
    return store.add_pending([_entry_from_list(p, cate) for p in posts])


def _discover_deep(client, store, cate, max_pages, restart=False):
    tag = str(cate)
    head = client.list_page(1, cate)
    total_count = int(head.get("totalCount") or 0)
    total_pages = _total_pages(head)
    st = store.state(tag)
    start = 1 if restart else (int(st.get("deep_page") or 0) + 1)
    print(
        f"[cate={cate}] 全量翻页: 共 {total_count} 帖 / {total_pages} 页, 从第 {start} 页开始",
        flush=True,
    )

    added = 0
    page = start
    scanned = 0
    reached_end = False
    while page <= total_pages:
        data = client.list_page(page, cate)
        posts = data.get("list") or []
        if not posts:
            # 空页 = 平台墙 (page>10) 或真的到底。没到 total_pages 就停, 不置 deep_done。
            break
        added += _absorb(store, posts, cate)
        store.set_state(tag, deep_page=page, total_count=total_count)
        scanned += 1
        print(
            f"\r[cate={cate}] 翻页 {page}/{total_pages} (累计新增 {added})",
            end="", flush=True,
        )
        if page >= total_pages:
            reached_end = True
            break
        if max_pages and scanned >= max_pages:
            break
        page += 1

    if reached_end:
        store.set_state(tag, deep_done=True)
        print(f"\n[cate={cate}] 全量翻页完成, 之后只做增量", flush=True)
    else:
        print(f"\n[cate={cate}] 翻到第 {page} 页停 (空页或到顶), 下趟从 {page + 1} 接着试", flush=True)
    return added


def _discover_shallow(client, store, cate, max_pages):
    tag = str(cate)
    page = 1
    total_pages = None
    total_count = 0
    added = 0
    quiet = 0
    while True:
        data = client.list_page(page, cate)
        if total_pages is None:
            total_count = int(data.get("totalCount") or 0)
            total_pages = _total_pages(data)
            print(f"[cate={cate}] 增量翻页: 共 {total_count} 帖 / {total_pages} 页", flush=True)
        posts = data.get("list") or []
        if not posts:
            break
        all_known = all(p.get("isTop") == "1" or store.seen(p["postId"]) for p in posts)
        added += _absorb(store, posts, cate)
        print(f"\r[cate={cate}] 翻页 {page}/{total_pages} (累计新增 {added})", end="", flush=True)
        quiet = quiet + 1 if all_known else 0
        if page >= total_pages or quiet >= STOP_PAGES:
            break
        if max_pages and page >= max_pages:
            break
        page += 1
    store.set_state(tag, total_count=total_count)
    print()
    return added


def _ext_from_ctype(ctype, url):
    ct = (ctype or "").lower()
    if "jpeg" in ct or "jpg" in ct:
        return ".jpg"
    if "png" in ct:
        return ".png"
    if "gif" in ct:
        return ".gif"
    if "webp" in ct:
        return ".webp"
    if "svg" in ct:
        return ".svg"
    last = url.rsplit(".", 1)[-1].split("?")[0].split("!")[0]
    if last.isalpha() and len(last) <= 4:
        return "." + last.lower()
    return ".bin"


def _replies_md(client, pid, on_image):
    page = 1
    out = []
    total = None
    while True:
        data = client.replies(pid, page)
        if data is None:
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
        name = u.get("alias", "") or ""
        t = r.get("addTime", "") or ""
        content = md_mod.rewrite_images(r.get("content", "") or "", on_image)
        lines.append(f"\n### {name} · {t}\n")
        lines.append(content.rstrip())
        for sub in r.get("subReply") or []:
            su = sub.get("user") or {}
            sname = su.get("alias", "") or ""
            st = sub.get("addTime", "") or ""
            scontent = md_mod.rewrite_images(sub.get("content", "") or "", on_image)
            lines.append(f"\n> **{sname}** · {st}\n>")
            lines.append("> " + scontent.rstrip().replace("\n", "\n> "))
    return "\n".join(lines) + "\n"


def _assemble(entry, body_md, comments_md):
    meta = (
        f"# {entry['title']}\n\n"
        f"> 作者: {entry['author']} (uid={entry['uid']})  \n"
        f"> 时间: {entry['create_time']}  \n"
        f"> 栏目: cate={entry['cate']}  \n"
        f"> 链接: https://www.joinquant.com/community/post/detailV2?postId={entry['pid']}\n"
    )
    parts = [meta, body_md, comments_md]
    return "\n\n".join(p.rstrip() for p in parts if p) + "\n"


def download_post(client, entry):
    """返回 (markdown, assets, status)。帖子已删除时 status="deleted", 只留墓碑。"""
    detail = client.detail(entry["pid"])
    if detail is None:
        return _assemble(entry, "_(帖子已删除或不可见)_", ""), {}, "deleted"

    assets = {}
    seen = {}
    counter = {"n": 0}

    def on_image(src):
        if not src or src.startswith("data:"):
            return src
        full = src if src.startswith("http") else (
            "https://www.joinquant.com" + src if src.startswith("/") else src
        )
        if full in seen:
            return seen[full]
        data, ctype = client.asset(full)
        if data is None:
            return src
        counter["n"] += 1
        name = f"{counter['n']:03d}{_ext_from_ctype(ctype, full)}"
        assets[name] = data
        seen[full] = f"assets/{name}"
        return seen[full]

    body_md = md_mod.rewrite_images(detail.get("content", "") or "", on_image)
    comments_md = (
        _replies_md(client, entry["pid"], on_image)
        if (entry.get("reply_count") or 0) > 0
        else ""
    )
    return _assemble(entry, body_md, comments_md), assets, "done"


def drain(client, store, limit=None):
    queue = store.pending()
    if limit:
        queue = queue[:limit]
    total = len(queue)
    done = 0
    for entry in queue:
        if store.has(entry["pid"]):
            continue
        done += 1
        print(
            f"[{done}/{total}] {entry['create_time'][:10]} {entry['title'][:40]}",
            flush=True,
        )
        markdown, assets, status = download_post(client, entry)
        store.commit(dict(entry, status=status), markdown, assets)
    store.compact_pending()
    return done


def run(store_root, cates=None, delay=0.3, max_pages=None, limit=None, deep=None):
    if cates is None:
        cates = [3]
    client = Client(delay=delay)
    store = Store(store_root)
    print(f"已入库 {len(store.index)} 篇, 队列剩 {len(store.pending())} 篇", flush=True)

    total_added = 0
    for cate in cates:
        added = discover(client, store, cate=cate, max_pages=max_pages, deep=deep)
        total_added += added
        print(f"[cate={cate}] 新发现 {added} 篇", flush=True)
    print(f"合计新发现 {total_added} 篇, 待下 {len(store.pending())} 篇", flush=True)

    n = drain(client, store, limit=limit)
    print(f"本轮入库 {n} 篇, 累计 {len(store.index)} 篇", flush=True)
