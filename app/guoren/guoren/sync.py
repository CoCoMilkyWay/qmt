"""主 flow: discover (翻页发现) → drain (逐篇下载入库)。

列表排序键是 max(create_time, last_reply_time) 倒序: 新帖必然在最前面, 老帖被新回复
顶上来也只会往前挪。已入库的帖子视为终态 (不因新回复重抓), 所以增量只需按 pid 去重。

翻页分两档, 由 state.json 的 deep_done 决定:
    deep  — 一路翻到最后一页, 首次全量用; 完成后置 deep_done
    浅    — 连续 STOP_PAGES 页非置顶全已知即早停; 日常增量只花几个请求
翻页本身不稳定 (翻的过程中有人回帖会把条目往后挤, 可能漏掉一条), 所以每页发现即落盘,
漏掉的下次 --deep 能对上账: seen_pids 与 total_count 的差值就是对账口径。
"""

import math
import re

from lxml import html as lxml_html

from . import html2md
from .fetch import Client
from .store import Store


def _entry_from_list(p):
    u = p.get("user") or {}
    return {
        "pid": p["pid"],
        "title": p.get("title", ""),
        "create_time": p.get("create_time", ""),
        "tag": p.get("tag", ""),
        "author": u.get("username", ""),
        "uid": u.get("uid"),
        "reply_count": p.get("reply_count", 0),
        "view_count": p.get("view_count", 0),
        "elite": p.get("elite", 0),
        "top": p.get("top") or "",
        "status": "pending",
    }


# 浅翻时连续多少页「非置顶全已知」才收手。留 2 页余量, 挡住翻页过程中的条目挪位。
STOP_PAGES = 2


def discover(client, store, tag="all", max_pages=None, deep=None):
    if deep is None:
        deep = not store.state().get("deep_done")
    page = 1
    total_pages = None
    total_count = 0
    added = 0
    quiet_pages = 0
    reached_end = False
    while True:
        data = client.list_page(page, tag)
        if total_pages is None:
            total_count = data["total_count"]
            total_pages = math.ceil(total_count / max(data["page_size"], 1))
            mode = "全量" if deep else "增量"
            print(f"{mode}翻页: 共 {total_count} 帖 / {total_pages} 页", flush=True)
        posts = data["post_list"]
        if not posts:
            reached_end = True
            break

        batch = []
        normal_all_known = True
        for p in posts:
            if not p.get("top") and not store.has(p["pid"]):
                # 置顶帖每页都可能冒出来, 不参与早停判断
                normal_all_known = False
            batch.append(_entry_from_list(p))
        added += store.add_pending(batch)  # 发现一页落盘一页
        print(
            f"\r翻页 {page}/{total_pages} (本页 +{len(batch)}, 累计新增 {added})",
            end="", flush=True,
        )

        quiet_pages = quiet_pages + 1 if normal_all_known else 0
        if page >= total_pages:
            reached_end = True
            break
        if max_pages and page >= max_pages:
            break
        if not deep and quiet_pages >= STOP_PAGES:
            break
        page += 1

    if deep and reached_end and not max_pages:
        store.set_state(deep_done=True, total_count=total_count)
    print()  # 结束翻页进度行
    seen = len(store.seen_pids())
    if total_count and seen < total_count:
        print(
            f"注意: 已见 {seen} / 列表报 {total_count} 帖 (差 {total_count - seen})",
            flush=True,
        )
    return added


def _extract_post_content(page_html):
    doc = lxml_html.fromstring(page_html)
    node = doc.find(".//div[@class='post-content']")
    assert node is not None, "未找到 .post-content"
    inner = (node.text or "") + "".join(
        lxml_html.tostring(c, encoding="unicode") for c in node
    )
    return inner


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


def _attachments_md(atts):
    if not atts:
        return ""
    lines = ["## 附件"]
    for a in atts:
        url = a.get("url", "")
        if url.startswith("/"):
            url = "https://guorn.com" + url
        lines.append(f"- [{a.get('name', url)}]({url})")
    return "\n".join(lines) + "\n"


def _comments_md(client, pid, on_image):
    page = 1
    cmts = []
    while True:
        data = client.comments(pid, page)
        cmts.extend(data.get("comment_list", []))
        if len(cmts) >= data.get("total_count", 0) or not data.get("comment_list"):
            break
        page += 1
    if not cmts:
        return ""
    lines = [f"## 评论 ({len(cmts)})"]
    for c in cmts:
        u = c.get("user") or {}
        name = u.get("username", "")
        t = c.get("create_time", "")
        content = html2md.to_markdown(c.get("content", "") or "", on_image=on_image)
        lines.append(f"\n### {name} · {t}\n")
        lines.append(content.rstrip())
    return "\n".join(lines) + "\n"


def _assemble(entry, body_md, attach_md, comments_md):
    meta = (
        f"# {entry['title']}\n\n"
        f"> 作者: {entry['author']} (uid={entry['uid']})  \n"
        f"> 时间: {entry['create_time']}  \n"
        f"> 标签: {entry['tag']}  \n"
        f"> 链接: https://guorn.com/forum/post/{entry['pid']}\n"
    )
    parts = [meta, body_md, attach_md, comments_md]
    return "\n\n".join(p.rstrip() for p in parts if p) + "\n"


def download_post(client, entry):
    """返回 (markdown, assets, status)。帖子已删除时 status="deleted", 只留墓碑。"""
    html = client.post_html(entry["pid"])
    if html is None:
        return _assemble(entry, "_(帖子已删除或不可见)_", "", ""), {}, "deleted"

    assets = {}
    seen = {}  # 同一地址只抓一次
    counter = {"n": 0}

    def on_image(src):
        if not src or src.startswith("data:"):
            return src
        full = (
            src
            if src.startswith("http")
            else ("https://guorn.com" + src if src.startswith("/") else src)
        )
        if full in seen:
            return seen[full]
        data, ctype = client.asset(full)
        counter["n"] += 1
        name = f"{counter['n']:03d}{_ext_from_ctype(ctype, full)}"
        assets[name] = data
        seen[full] = f"assets/{name}"
        return seen[full]

    body_md = html2md.to_markdown(_extract_post_content(html), on_image=on_image)
    detail = client.post_detail(entry["pid"])
    attach_md = _attachments_md(detail.get("attachments") or [])
    # reply_count 为 0 就不必再问一趟评论接口
    comments_md = (
        _comments_md(client, entry["pid"], on_image)
        if (entry.get("reply_count") or 0) > 0
        else ""
    )
    return _assemble(entry, body_md, attach_md, comments_md), assets, "done"


def drain(client, store, limit=None):
    """逐篇下载入库。每篇独立提交, 随时 kill 都只丢当前这一篇。"""
    queue = store.pending()
    if limit:
        queue = queue[:limit]
    total = len(queue)
    done = 0
    for entry in queue:
        if store.has(entry["pid"]):  # 上一趟已入库, 队列文件还没压缩
            continue
        done += 1
        print(f"[{done}/{total}] {entry['create_time'][:10]} {entry['title'][:40]}", flush=True)
        markdown, assets, status = download_post(client, entry)
        store.commit(dict(entry, status=status), markdown, assets)
    store.compact_pending()
    return done


def run(store_root, tag="all", delay=0.3, max_pages=None, limit=None, deep=None):
    client = Client(delay=delay)
    store = Store(store_root)
    backlog = len(store.pending())
    print(f"已入库 {len(store.index)} 篇, 队列剩 {backlog} 篇", flush=True)

    added = discover(client, store, tag=tag, max_pages=max_pages, deep=deep)
    print(f"新发现 {added} 篇, 待下 {len(store.pending())} 篇", flush=True)

    n = drain(client, store, limit=limit)
    print(f"本轮入库 {n} 篇, 累计 {len(store.index)} 篇", flush=True)
