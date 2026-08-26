#!/usr/bin/env python3
"""论坛帖子同步入口, 一个 flow 管所有站点。跑什么、怎么跑都在下面几个常量里改。

随时 Ctrl-C / kill 都安全: 每篇文章独立原子提交, 下次运行从队列接着下。
已入库的帖子视为终态, 不会重抓; 这一轮没抓到的留作空洞, 下一轮接着重试。

深翻一律从最旧一页往新扫, 队列因此严格按时间由老到新; 下载本身并发、哪篇先完
哪篇先入库 (乱序)。出网方式、频率与并发写死在各站 site/<name>.py 顶部。
"""

import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(ROOT))

from web.common.sync import run
from web.site import make

# 这一趟爬哪些站, 按顺序跑。可选 guoren / jquant。
SITES = ["jquant", "guoren"]

# 栏目。None = 用各站 site/<name>.py 里的 default_tags
#   果仁: share=知识共享 / usage / other / elite=精华 / all
#   聚宽: "3"=文章 / "10"=问答 / "13"=公告 / "14"=精华 / "16"=精选
TAGS = None

# True = 强制翻完整个列表对账 (补漏); None = 看 state.json 的进度自动决定深翻还是增量
DEEP = None

# 调试用限流, None = 不限。只翻 N 页列表 / 只下 N 篇。
MAX_PAGES = None
LIMIT = None

# 缓存根目录。None = <本文件目录>/store/<site>/
STORE = None


def main():
    assert not (
        STORE and len(SITES) > 1
    ), "STORE 写死时只能跑一个站, 否则两站会共用一个库"
    try:
        for name in SITES:
            site = make(name)
            store_root = STORE or os.path.join(ROOT, "store", site.name)
            run(
                site,
                store_root,
                tags=TAGS,
                max_pages=MAX_PAGES,
                limit=LIMIT,
                deep=DEEP,
            )
    except KeyboardInterrupt:
        print("\n已中断; 下次运行从队列接着下。", flush=True)
        sys.exit(130)


if __name__ == "__main__":
    main()
