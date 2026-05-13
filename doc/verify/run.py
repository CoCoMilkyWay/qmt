#!/usr/bin/env python3
"""build & run doc/verify 下的 C++ verify_cpp.

用法:
  python run.py                                # 按 main.cpp 顶部 kApis 表里的开关跑
  python run.py --year-from 2020 --year-to 2024
  python run.py --clean                        # 删 build/ 重建
  python run.py --debug                        # -O0 -g

api 开关写在 src/main.cpp 顶部的 kApis 数组里, 编辑 true/false 后重跑.
"""

import argparse
import pathlib
import shutil
import subprocess


HERE = pathlib.Path(__file__).resolve().parent
BUILD = HERE / "build"
BIN = BUILD / "bin" / "verify_cpp"
OUT = HERE / "out"


def configure(debug):
    BUILD.mkdir(exist_ok=True)
    cmd = [
        "cmake",
        "-S", str(HERE),
        "-B", str(BUILD),
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
    ]
    if debug:
        cmd.append("-DDEBUG_MODE=ON")
    print("[configure]", " ".join(cmd))
    subprocess.run(cmd, check=True)


def build():
    cmd = ["cmake", "--build", str(BUILD), "-j"]
    print("[build]", " ".join(cmd))
    subprocess.run(cmd, check=True)


def run(year_from, year_to, sample_dates):
    assert BIN.exists(), f"build 失败: {BIN} 不存在"
    OUT.mkdir(exist_ok=True)
    cmd = [
        str(BIN),
        "--year-from", str(year_from),
        "--year-to", str(year_to),
        "--out-dir", str(OUT),
        "--data-dir", str(HERE.parents[1] / "data"),
    ]
    if sample_dates:
        cmd += ["--sample-dates", sample_dates]
    print("[run]", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--year-from", type=int, default=2015)
    ap.add_argument("--year-to", type=int, default=2025)
    ap.add_argument("--sample-dates", default="",
                    help="可选 YYYYMMDD 逗号列表; 给定则只比这些日期 (调试用)")
    ap.add_argument("--clean", action="store_true", help="先删 build/")
    ap.add_argument("--debug", action="store_true", help="DEBUG_MODE=ON")
    ap.add_argument("--no-build", action="store_true", help="跳过 build, 直接 run")
    args = ap.parse_args()

    if args.clean and BUILD.exists():
        print(f"[clean] rm -rf {BUILD}")
        shutil.rmtree(BUILD)

    if not args.no_build:
        configure(args.debug)
        build()

    run(args.year_from, args.year_to, args.sample_dates)


if __name__ == "__main__":
    main()
