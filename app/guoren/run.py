#!/usr/bin/env python3
"""果仁论坛爬虫入口。

用法:
    ./run.py                            增量更新 store/ (默认爬 知识共享 + 精华)
    ./run.py --tag share elite          指定栏目 (share/usage/other/elite/all/...)
    ./run.py --deep                     强制翻完整个列表对账 (补漏)
    ./run.py --max-pages 3              只翻前 3 页列表 (调试)
    ./run.py --limit 5                  只下 5 篇 (调试)
    ./run.py --store ~/guoren-data      换缓存根目录

随时 Ctrl-C / kill 都安全: 每篇文章独立原子提交, 下次运行从队列接着下。
已入库的帖子视为终态, 不会重抓; 这一轮没抓到的留作空洞, 下一轮接着重试。

出网方式 (隧道/直连)、频率与并发写死在 src/fetch.py 顶部, 不做成命令行开关。
"""

import argparse
import os
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)

from src.sync import run


def main():
    ap = argparse.ArgumentParser(description="爬取果仁论坛全部帖子为 .md")
    ap.add_argument("--store", default=os.path.join(ROOT, "store"))
    ap.add_argument(
        "--tag",
        nargs="+",
        default=["share", "elite"],
        help="栏目 (share/usage/other/elite/all/...), 默认 知识共享+精华",
    )
    ap.add_argument("--max-pages", type=int, default=None, help="只翻前 N 页 (调试)")
    ap.add_argument("--limit", type=int, default=None, help="只下 N 篇 (调试)")
    ap.add_argument("--deep", action="store_true", help="强制翻完整个列表对账")
    args = ap.parse_args()
    try:
        run(
            args.store,
            tags=args.tag,
            max_pages=args.max_pages,
            limit=args.limit,
            deep=True if args.deep else None,
        )
    except KeyboardInterrupt:
        print("\n已中断; 下次运行从队列接着下。", flush=True)
        sys.exit(130)


if __name__ == "__main__":
    main()
