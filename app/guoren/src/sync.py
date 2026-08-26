"""主 flow: discover (翻页发现) → drain (逐篇下载入库)。

列表排序键是 max(create_time, last_reply_time) 倒序: 新帖必然在最前面, 老帖被新回复
顶上来也只会往前挪。已入库的帖子视为终态 (不因新回复重抓), 所以增量只需按 pid 去重。

翻页分两档, 由 state.json 的 deep_done 决定:
    深 — 首次全量, 从最旧一页往新扫。每扫完一页把「页码 + 该页最新一条的 pid」落盘作
         进度, 随时 kill 都能接着走, 不会从头再来。用 pid 当锚点是因为新帖会让整个列表
         的页号后移, 页号跨运行不稳定; 而「比锚点更旧的都已发现」这条边界是稳的。
    浅 — 日常增量, 从第 1 页往后翻, 连续 STOP_PAGES 页非置顶全已知即收手, 只花几个请求。

翻页本身不稳定 (翻的过程中有人回帖会把条目往后挤, 可能漏掉一条), 所以每页发现即落盘,
漏掉的下次 --deep 能对上账: seen_pids 与 total_count 的差值就是对账口径。

drain 抓取并发、提交串行但乱序 (走隧道时单请求 ~0.78s, 串行等于把额度扔掉)。抓不到的
那篇不入库、留作空洞, 仍在 pending 里等下一轮 drain 重试; 入库要「连图完整」, 缺图宁可
整篇重抓, 也不在 index 里堆半成品 —— index 里的是终态, 不会再重抓。
"""

import math
import re
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait

from lxml import html as lxml_html

from . import html2md
from .fetch import BASE, MISSING, Client
from .store import Store


def _entry_from_list(p):
    u = p.get("user") or {}
    return {
        "pid": p["pid"],
        # 一律落成字符串: 接口给的 title/tag 可能是 null, 留着 None 会在落盘取名
        # 或截断标题时才炸, 离发现处已经很远了
        "title": p.get("title") or "",
        "create_time": p.get("create_time") or "",
        "tag": p.get("tag") or "",
        "author": u.get("username") or "",
        "uid": u.get("uid"),
        "reply_count": p.get("reply_count", 0),
        "view_count": p.get("view_count", 0),
        "elite": p.get("elite", 0),
        "top": p.get("top") or "",
        "status": "pending",
    }


# 浅翻时连续多少页「非置顶全已知」才收手。留 2 页余量, 挡住翻页过程中的条目挪位。
STOP_PAGES = 2
# 重定位锚点时最多往后找几页。一页 20 帖, 30 页足够容纳两次运行之间的新增量。
ANCHOR_WINDOW = 30


def _page_total(data):
    return math.ceil(data["total_count"] / max(data["page_size"], 1))


def discover(client, store, tag, max_pages=None, deep=None):
    # deep=True 是显式 --deep: 对账用, 丢开锚点从最旧一页重扫; deep=None 则看进度自动决定
    forced = deep is True
    if deep is None:
        deep = not store.state(tag).get("deep_done")
    added = (
        _discover_deep(client, store, tag, max_pages, restart=forced)
        if deep
        else _discover_shallow(client, store, tag, max_pages)
    )
    st = store.state(tag)
    total_count = st.get("total_count") or 0
    seen = len(store.seen_pids())
    if total_count and seen < total_count:
        print(
            f"注意: 已见 {seen} / 列表报 {total_count} 帖 (差 {total_count - seen}), "
            f"可跑 --deep 对账",
            flush=True,
        )
    return added


def _absorb(store, posts, oldest_first):
    """把一页条目收进队列。oldest_first 时页内也反转成由早到新, 让队列大致按时序。"""
    batch = [_entry_from_list(p) for p in posts]
    if oldest_first:
        batch.reverse()
    return store.add_pending(batch)


def _locate_anchor(client, store, tag, anchor, hint, total_pages):
    """找出锚点帖现在落在第几页, 返回 (页号, 顺带新收的条数)。

    条目只会被新活动往后 (页号变大) 挤, 所以从上次记录的页号往后找即可; 顺带把路过的
    页也收进队列。找不到 (锚点帖被删了) 就退回最旧一页重扫 — 慢但不会错。
    """
    added = 0
    for page in range(hint, min(total_pages, hint + ANCHOR_WINDOW) + 1):
        data = client.list_page(page, tag)
        added += _absorb(store, data["post_list"], oldest_first=True)
        if any(p["pid"] == anchor for p in data["post_list"]):
            return page, added
    return total_pages, added


def _discover_deep(client, store, tag, max_pages, restart=False):
    """从最旧一页往新扫, 每扫完一页就把「页码 + 锚点」落盘, 所以随时 kill 都能接着走。

    锚点取该页最新的一条: 它标记了「比这条更旧的都已发现」这条边界。用 pid 而不是页码
    当进度, 是因为新帖会让页号整体后移, 页号跨运行不稳定。
    """
    head = client.list_page(1, tag)
    total_count = head["total_count"]
    total_pages = _page_total(head)
    st = store.state(tag)
    anchor = None if restart else st.get("deep_anchor")

    added = 0
    if anchor:
        at, added = _locate_anchor(
            client, store, tag, anchor, st.get("deep_page") or total_pages, total_pages
        )
        start = at - 1  # 锚点那页已扫过, 从更新的一页接着来
        print(
            f"[{tag}] 全量翻页: 共 {total_count} 帖 / {total_pages} 页, 续扫第 {start} 页",
            flush=True,
        )
    else:
        start = total_pages
        print(
            f"[{tag}] 全量翻页: 共 {total_count} 帖 / {total_pages} 页, 从最旧一页开始",
            flush=True,
        )

    scanned = 0
    page = start
    while page >= 1:
        data = client.list_page(page, tag)
        posts = data["post_list"]
        if not posts:
            break
        added += _absorb(store, posts, oldest_first=True)
        # 进度落盘: 这一页扫完了, 边界推到本页最新的那条
        store.set_state(
            tag, deep_page=page, deep_anchor=posts[0]["pid"], total_count=total_count
        )
        scanned += 1
        print(
            f"\r[{tag}] 翻页 {page}/{total_pages} (剩 {page - 1} 页, 累计新增 {added})",
            end="",
            flush=True,
        )
        page -= 1
        if max_pages and scanned >= max_pages:
            break

    if page < 1:
        store.set_state(tag, deep_done=True)
        print(f"\n[{tag}] 全量翻页完成, 之后只做增量", flush=True)
    else:
        print(f"\n[{tag}] 翻到第 {page + 1} 页, 下次接着往新扫", flush=True)
    return added


def _discover_shallow(client, store, tag, max_pages):
    """日常增量: 新帖必然在最前面, 从第 1 页往后翻到连续 STOP_PAGES 页全已知就收手。"""
    page = 1
    total_pages = None
    total_count = 0
    added = 0
    quiet_pages = 0
    while True:
        data = client.list_page(page, tag)
        if total_pages is None:
            total_count = data["total_count"]
            total_pages = _page_total(data)
            print(
                f"[{tag}] 增量翻页: 共 {total_count} 帖 / {total_pages} 页", flush=True
            )
        posts = data["post_list"]
        if not posts:
            break

        # 置顶帖每页都可能冒出来, 不参与早停判断。判据是「见过」而非「已入库」:
        # 深翻发现但还没下载的帖子只在队列里, 用 has() 会让早停永远不成立。
        all_known = all(p.get("top") or store.seen(p["pid"]) for p in posts)
        added += _absorb(store, posts, oldest_first=False)
        print(
            f"\r[{tag}] 翻页 {page}/{total_pages} (累计新增 {added})",
            end="",
            flush=True,
        )

        quiet_pages = quiet_pages + 1 if all_known else 0
        if page >= total_pages or quiet_pages >= STOP_PAGES:
            break
        if max_pages and page >= max_pages:
            break
        page += 1

    store.set_state(tag, total_count=total_count)
    print()
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
    """→ Markdown | None (评论没抓全, 整篇作空洞: 缺一页评论的半成品不入库)。"""
    page = 1
    cmts = []
    while True:
        data = client.comments(pid, page)
        if data is None:
            return None
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
    """→ (markdown, assets, status) | None (这次没抓到, 作空洞留在队列下轮补)。

    帖子被删掉 (404) 是终态, 立墓碑 status="deleted"; 只有「这次没抓到」才作空洞。
    抓完不落盘, 字节先攒在内存里, 由主线程串行提交 — 本函数可并发, 不碰 store。
    """
    html = client.post_html(entry["pid"])
    if html is MISSING:
        return _assemble(entry, "_(帖子已删除或不可见)_", "", ""), {}, "deleted"
    if html is None:
        return None

    assets = {}
    seen = {}  # 同一地址只抓一次
    counter = {"n": 0}
    hole = []  # 站内图这次没抓到 ⇒ 整篇作空洞: 入库要「连图完整」

    def on_image(src):
        # 相对路径拼到果仁, 绝对路径原样用; data: 和 file:/// 这种本地伪 URL 直接留着。
        # 图片一律直连 (见 fetch.asset), 不走隧道, 所以不占隧道额度。
        if not src or src.startswith("data:") or src.startswith("file:"):
            return src
        full = BASE + src if src.startswith("/") else src
        if full in seen:
            return seen[full]
        data, ctype = client.asset(full)
        if data is MISSING or data is None:
            # 图没了 (404) 就保留原链; 这次没抓到才把整篇作空洞, 下轮重来
            if data is None:
                hole.append(full)
            return src
        counter["n"] += 1
        name = f"{counter['n']:03d}{_ext_from_ctype(ctype, full)}"
        assets[name] = data
        seen[full] = f"assets/{name}"
        return seen[full]

    body_md = html2md.to_markdown(_extract_post_content(html), on_image=on_image)
    if hole:
        return None  # 早退: 已经缺图了, 不必再花 detail 与评论那几个请求
    detail = client.post_detail(entry["pid"])
    if detail is None:
        return None
    attach_md = _attachments_md(detail.get("attachments") or [])
    # reply_count 为 0 就不必再问一趟评论接口
    comments_md = (
        _comments_md(client, entry["pid"], on_image)
        if (entry.get("reply_count") or 0) > 0
        else ""
    )
    if comments_md is None or hole:
        return None
    return _assemble(entry, body_md, attach_md, comments_md), assets, "done"


def drain(client, store, limit=None):
    """抓取并发、提交串行但乱序: 哪篇抓完哪篇先入库, 不死等队头。返回 (入库数, 空洞数)。

    严格按序 + 队头一篇慢 = 后面抓完的全干等; 放开顺序后慢的那篇自己留作空洞
    (仍在 pending, 下轮 drain 再抓), 其余照常前进。
    派发窗口取 workers ⇒ 「在抓的 + 等提交的」合计不超过 workers 篇, 否则几十篇
    的图片字节会一起挂在内存里。
    """
    # 队列是从 pending.jsonl 读回来的, 老行里可能留着 null (发现处如今一律落成
    # 字符串了)。进流水线前规范化一次, 下游打印/取名/渲染就都能当字符串使。
    queue = [
        dict(e, title=e.get("title") or "", tag=e.get("tag") or "")
        for e in store.pending()
        if not store.has(e["pid"])
    ]
    if limit:
        queue = queue[:limit]
    total = len(queue)
    if not total:
        return 0, 0

    seq = 0
    holes = 0
    todo = iter(queue)
    flying = {}
    ex = ThreadPoolExecutor(max_workers=client.workers)
    try:

        def refill():
            while len(flying) < client.workers:
                entry = next(todo, None)
                if entry is None:
                    return
                flying[ex.submit(download_post, client, entry)] = entry

        refill()
        while flying:
            finished, _ = wait(list(flying), return_when=FIRST_COMPLETED)
            for f in finished:
                entry = flying.pop(f)
                got = f.result()
                seq += 1
                if got is None:
                    holes += 1
                    print(
                        f"[{seq}/{total}] 空洞 {entry['title'][:40]} (留在队列下轮补)",
                        flush=True,
                    )
                    continue
                markdown, assets, status = got
                # commit 逼出串行: .staging 与 index 的追加都只有一处
                store.commit(dict(entry, status=status), markdown, assets)
                print(
                    f"[{seq}/{total}] {entry['create_time'][:10]} {entry['title'][:40]}",
                    flush=True,
                )
            refill()
    finally:
        ex.shutdown(wait=False, cancel_futures=True)
    store.compact_pending()
    return seq - holes, holes


def run(store_root, tags=None, max_pages=None, limit=None, deep=None):
    if tags is None:
        tags = ["share", "elite"]
    client = Client()
    print(client.desc(), flush=True)
    store = Store(store_root)
    print(f"已入库 {len(store.index)} 篇, 队列剩 {len(store.pending())} 篇", flush=True)

    total_added = 0
    for tag in tags:
        added = discover(client, store, tag=tag, max_pages=max_pages, deep=deep)
        total_added += added
        print(f"[{tag}] 新发现 {added} 篇", flush=True)
    print(f"合计新发现 {total_added} 篇, 待下 {len(store.pending())} 篇", flush=True)

    n, holes = drain(client, store, limit=limit)
    tail = f", 空洞 {holes} 篇 (下轮补)" if holes else ""
    print(f"本轮入库 {n} 篇{tail}, 累计 {len(store.index)} 篇", flush=True)
