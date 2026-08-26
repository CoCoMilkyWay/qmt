#!/usr/bin/env python3
"""编译并运行 wxmd。

用法:
    ./run.py                            增量同步 store/，结束后可选加新公众号
    ./run.py --uninstall                还原系统代理、移除 CA 与 ~/.wxmd
    ./run.py --build                    只编译，不运行
    ./run.py --clean                    清空构建目录后重新编译

--build / --clean 之外的参数原样透传给 wxmd。
"""

import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT, "build")
BINARY = os.path.join(BUILD_DIR, "wxmd")


def run(cmd, quiet=False):
    if not quiet:
        print("$ " + " ".join(cmd), flush=True)

    if quiet:
        completed = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if completed.returncode != 0:
            print("$ " + " ".join(cmd), flush=True)
            print(completed.stdout + completed.stderr, flush=True)
    else:
        completed = subprocess.run(cmd, cwd=ROOT)

    if completed.returncode != 0:
        sys.exit(completed.returncode)


def build(jobs):
    # cmake 重复 configure 是幂等且很快的，总是执行可避免上次失败留下的半成品缓存。
    # configure 的日志只在出错时才有价值，平时静默。
    run(
        ["cmake", "-S", ROOT, "-B", BUILD_DIR, "-DCMAKE_BUILD_TYPE=Release"], quiet=True
    )
    run(["cmake", "--build", BUILD_DIR, "--target", "wxmd_cli", "-j", str(jobs)])


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--build", action="store_true", help="只编译")
    parser.add_argument("--clean", action="store_true", help="清空构建目录")
    parser.add_argument(
        "-j", type=int, default=os.cpu_count() or 4, help="并行编译任务数"
    )
    parser.add_argument("-h", "--help", action="store_true")
    known, passthrough = parser.parse_known_args()

    if known.help:
        print(__doc__)
        return

    if known.clean and os.path.isdir(BUILD_DIR):
        print(f"清理 {BUILD_DIR}", flush=True)
        shutil.rmtree(BUILD_DIR)

    build(known.j)

    if known.build:
        print(f"\n构建完成: {BINARY}", flush=True)
        return

    print("", flush=True)
    completed = subprocess.run([BINARY] + passthrough, cwd=ROOT)
    sys.exit(completed.returncode)


if __name__ == "__main__":
    main()
