#!/usr/bin/env python3
"""果仁论坛爬虫入口。

用法:
    ./run.py                            增量更新 store/ (首次自动全量)
    ./run.py --deep                     强制翻完整个列表对账 (补漏)
    ./run.py --tag usage                只爬某个标签 (all/share/usage/other/...)
    ./run.py --max-pages 3              只翻前 3 页列表 (调试)
    ./run.py --limit 5                  只下 5 篇 (调试)
    ./run.py --delay 0.5                加大请求间隔
    ./run.py --store ~/guoren-data      换缓存根目录

随时 Ctrl-C / kill 都安全: 每篇文章独立原子提交, 下次运行从队列接着下。
已入库的帖子视为终态, 不会重抓。
"""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from guoren.sync import run


def main():
    ap = argparse.ArgumentParser(description="爬取果仁论坛全部帖子为 .md")
    ap.add_argument("--store", default=os.path.join(ROOT, "store"))
    ap.add_argument("--tag", default="all", help="all/share/usage/other/...")
    ap.add_argument("--delay", type=float, default=0.3, help="请求间隔秒")
    ap.add_argument("--max-pages", type=int, default=None, help="只翻前 N 页 (调试)")
    ap.add_argument("--limit", type=int, default=None, help="只下 N 篇 (调试)")
    ap.add_argument("--deep", action="store_true", help="强制翻完整个列表对账")
    args = ap.parse_args()
    try:
        run(
            args.store,
            tag=args.tag,
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
