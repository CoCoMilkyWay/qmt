#!/usr/bin/env python3
"""聚宽社区帖子同步入口。

用法:
    ./run.py                            增量更新 store/ (默认爬 文章 cate=3)
    ./run.py --cate 3 10                指定栏目 (3=文章/10=问答/13=公告/14=精华/16=精选)
    ./run.py --deep                     强制翻完整个列表对账 (补漏)
    ./run.py --max-pages 3              只翻前 3 页列表 (调试)
    ./run.py --limit 5                  只下 5 篇 (调试)
    ./run.py --delay 0.5                加大请求间隔
    ./run.py --store ~/jq-data          换缓存根目录

随时 Ctrl-C / kill 都安全: 每篇文章独立原子提交, 下次运行从队列接着下。
已入库的帖子视为终态, 不会重抓。
"""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from src.sync import run


def main():
    ap = argparse.ArgumentParser(description="同步聚宽社区帖子为 .md")
    ap.add_argument("--store", default=os.path.join(ROOT, "store"))
    ap.add_argument(
        "--cate",
        nargs="+",
        type=int,
        default=[3],
        help="栏目 cate (3=文章/10=问答/13=公告/14=精华/16=精选), 默认 文章",
    )
    ap.add_argument("--delay", type=float, default=0.3, help="请求间隔秒")
    ap.add_argument("--max-pages", type=int, default=None, help="只翻前 N 页 (调试)")
    ap.add_argument("--limit", type=int, default=None, help="只下 N 篇 (调试)")
    ap.add_argument("--deep", action="store_true", help="强制翻完整个列表对账")
    args = ap.parse_args()
    try:
        run(
            args.store,
            cates=args.cate,
            delay=args.delay,
            max_pages=args.max_pages,
            limit=args.limit,
            deep=True if args.deep else None,
        )
    except KeyboardInterrupt:
        print("\n已中断; 下次运行从队列接着下。", flush=True)
        sys.exit(130)


if __name__ == "__main__":
    main()
