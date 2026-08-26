"""通用 web 同步 flow: discover (翻页发现) → drain (逐篇下载入库)。

与站点无关。站点适配器 (site/*.py) 只需提供:
    site.name                       站点标识, 决定 store/<name>/
    site.default_tags               默认爬哪些栏目
    site.fetcher                    出网器 (给 workers / desc)
    site.list_page(page, tag)       → {"entries": [...], "total_count", "page_size"}
                                      entries 已归一化成本模块认识的键 (见下)
    site.download_post(entry)       → (markdown, assets, status) | None (这次没抓到)

归一化 entry 的键: pid / title / create_time / tag / is_top / reply_count / status,
站点特有的字段照样可以塞进去 (原样落进 index.jsonl), 本模块只认上面这几个。

翻页分两档, 由 state.json 的 deep_done 决定:
    深 — 首次全量, 一律从最旧一页往新扫, 页内也反转 ⇒ 队列 (pending.jsonl) 严格按
         时间由老到新排列。每扫完一页把「页码 + 该页最新一条的 pid」落盘作进度,
         随时 kill 都能接着走。用 pid 当锚点而不是页码, 是因为列表按时间倒序、新帖
         只往前插, 页号会随新帖整体后移, 跨运行不稳定; 而「比锚点更旧的都已发现」
         这条边界是稳的。只记页码的话, 两次运行之间只要有人发帖就会漏。
    浅 — 日常增量, 从第 1 页往后翻, 连续 STOP_PAGES 页非置顶全已知即收手。

翻页本身不稳定 (翻的过程中有新帖会把条目往后挤, 可能漏掉一条), 所以每页发现即落盘,
漏掉的下次 --deep 能对上账: seen_pids 与 total_count 的差值就是对账口径。

drain 抓取并发、提交串行但乱序: 队列本身是老→新的, 但哪篇先抓完就哪篇先入库 —
严格按序 + 队头一篇慢 = 后面抓完的全干等, 不划算。抓不到的那篇不入库、留作空洞,
仍在 pending 里等下一轮; 入库要「连图完整」, 缺图宁可整篇重抓, 也不在 index 里堆
半成品 —— index 里的是终态, 不会再重抓。
"""

import math
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait

from .store import Store

# 浅翻时连续多少页「非置顶全已知」才收手。留 2 页余量, 挡住翻页过程中的条目挪位。
STOP_PAGES = 2
# 重定位锚点时最多往后找几页。足够容纳两次运行之间的新增量即可。
ANCHOR_WINDOW = 30


def _page_total(data):
    return math.ceil(data["total_count"] / max(data["page_size"], 1))


def _absorb(store, entries):
    """把一页条目收进队列, 页内按时间升序排好, 让队列严格由早到新。

    按 create_time 排而不是简单反转: 页内并非总是严格的新→老 — 置顶帖会被钉在每页
    最前面 (聚宽第 1 页那两条置顶是 2017/2026-06 的老帖), 果仁的排序键又是
    max(create_time, last_reply_time), 反转都会把它们甩到队尾。
    """
    return store.add_pending(sorted(entries, key=lambda e: e["create_time"]))


def discover(site, store, tag, max_pages=None, deep=None):
    # deep=True 是显式 --deep: 对账用, 丢开锚点从最旧一页重扫; deep=None 则看进度自动决定
    forced = deep is True
    if deep is None:
        deep = not store.state(tag).get("deep_done")
    added = (
        _discover_deep(site, store, tag, max_pages, restart=forced)
        if deep
        else _discover_shallow(site, store, tag, max_pages)
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


def _locate_anchor(site, store, tag, anchor, hint, total_pages):
    """找出锚点帖现在落在第几页。找不到 (锚点帖被删了) 就退回最旧一页重扫 — 慢但不会错。

    条目只会被新帖往后 (页号变大) 挤, 所以从上次记录的页号往后找即可。这里只认路不
    入队: 路过的页都比锚点新, 紧接着的深翻循环 (从 at-1 往 1) 正好会按老→新的顺序
    再走一遍, 顺带入队反而会把新帖插到老帖前面, 破坏队列的时序。
    """
    for page in range(hint, min(total_pages, hint + ANCHOR_WINDOW) + 1):
        data = site.list_page(page, tag)
        if any(e["pid"] == anchor for e in data["entries"]):
            return page
    return total_pages


def _discover_deep(site, store, tag, max_pages, restart=False):
    """从最旧一页往新扫, 每扫完一页就把「页码 + 锚点」落盘, 所以随时 kill 都能接着走。

    锚点取该页最新的一条: 它标记了「比这条更旧的都已发现」这条边界。
    """
    head = site.list_page(1, tag)
    total_count = head["total_count"]
    total_pages = _page_total(head)
    st = store.state(tag)
    anchor = None if restart else st.get("deep_anchor")

    added = 0
    if anchor:
        at = _locate_anchor(
            site, store, tag, anchor, st.get("deep_page") or total_pages, total_pages
        )
        # 从锚点那一页本身接着扫, 而不是它的下一页: 锚点是上次那页里最新的一条,
        # 页号后移后它多半落在页中间, 同页里比它新的那几条从没扫过。重扫一页的代价
        # 只是几条重复条目 (add_pending 按 pid 去重), 跳过则是静悄悄地漏帖。
        start = at
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
        data = site.list_page(page, tag)
        entries = data["entries"]
        if not entries:
            break
        added += _absorb(store, entries)
        # 进度落盘: 这一页扫完了, 边界推到本页最新的那条
        store.set_state(
            tag, deep_page=page, deep_anchor=entries[0]["pid"], total_count=total_count
        )
        scanned += 1
        # 补空格到定宽: 页号由大到小走, 行会变短, 不补的话 \r 盖不住上一行的尾巴
        line = f"[{tag}] 翻页 {page}/{total_pages} (剩 {page - 1} 页, 累计新增 {added})"
        print(f"\r{line:<70}", end="", flush=True)
        page -= 1
        if max_pages and scanned >= max_pages:
            break

    if page < 1:
        store.set_state(tag, deep_done=True)
        print(f"\n[{tag}] 全量翻页完成, 之后只做增量", flush=True)
    else:
        print(f"\n[{tag}] 翻到第 {page + 1} 页, 下次接着往新扫", flush=True)
    return added


def _discover_shallow(site, store, tag, max_pages):
    """日常增量: 新帖必然在最前面, 从第 1 页往后翻到连续 STOP_PAGES 页全已知就收手。

    翻的方向只能是新→老 (新帖都挤在第 1 页), 与队列要求的老→新正好相反, 所以本轮
    扫到的先全攒着, 收手后按 create_time 升序一次性入队 —— pending.jsonl 因此始终
    严格由老到新。代价是本轮中途被 kill 会丢掉这一轮的发现, 但增量通常只有两三页,
    下次重扫一遍就回来了; 深翻那边仍是逐页落盘, 不受影响。
    """
    page = 1
    total_pages = None
    total_count = 0
    quiet_pages = 0
    found = []
    while True:
        data = site.list_page(page, tag)
        if total_pages is None:
            total_count = data["total_count"]
            total_pages = _page_total(data)
            print(
                f"[{tag}] 增量翻页: 共 {total_count} 帖 / {total_pages} 页", flush=True
            )
        entries = data["entries"]
        if not entries:
            break

        # 置顶帖每页都可能冒出来, 不参与早停判断。判据是「见过」而非「已入库」:
        # 深翻发现但还没下载的帖子只在队列里, 用 has() 会让早停永远不成立。
        all_known = all(e["is_top"] or store.seen(e["pid"]) for e in entries)
        found.extend(entries)
        line = f"[{tag}] 翻页 {page}/{total_pages} (扫到 {len(found)} 条)"
        print(f"\r{line:<70}", end="", flush=True)

        quiet_pages = quiet_pages + 1 if all_known else 0
        if page >= total_pages or quiet_pages >= STOP_PAGES:
            break
        if max_pages and page >= max_pages:
            break
        page += 1

    found.sort(key=lambda e: e["create_time"])
    added = store.add_pending(found)
    store.set_state(tag, total_count=total_count)
    print()
    return added


def drain(site, store, limit=None):
    """抓取并发、提交串行但乱序。返回 (入库数, 空洞数)。

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

    workers = site.fetcher.workers
    seq = 0
    holes = 0
    todo = iter(queue)
    flying = {}
    ex = ThreadPoolExecutor(max_workers=workers)
    try:

        def refill():
            while len(flying) < workers:
                entry = next(todo, None)
                if entry is None:
                    return
                flying[ex.submit(site.download_post, entry)] = entry

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


def run(site, store_root, tags=None, max_pages=None, limit=None, deep=None):
    if tags is None:
        tags = site.default_tags
    print(site.fetcher.desc(), flush=True)
    store = Store(store_root)
    print(f"已入库 {len(store.index)} 篇, 队列剩 {len(store.pending())} 篇", flush=True)

    total_added = 0
    for tag in tags:
        added = discover(site, store, tag=tag, max_pages=max_pages, deep=deep)
        total_added += added
        print(f"[{tag}] 新发现 {added} 篇", flush=True)
    print(f"合计新发现 {total_added} 篇, 待下 {len(store.pending())} 篇", flush=True)

    n, holes = drain(site, store, limit=limit)
    tail = f", 空洞 {holes} 篇 (下轮补)" if holes else ""
    print(f"本轮入库 {n} 篇{tail}, 累计 {len(store.index)} 篇", flush=True)
