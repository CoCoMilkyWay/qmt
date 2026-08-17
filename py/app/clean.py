"""Clean tushare 月度 parquet for a single interface.

删除:
  - data/YYYY-MM/<ITF>.parquet   (该 itf 全部月度分片)

适用 ITF ∈ {forecast, express, disclosure} (tushare 3 张事件表).
BigQuant 24 张表同理可清, 但关月冻结后通常无需手动清.

Usage:
    Set ITF below, then: python py/app/clean.py
"""

import glob
import os

# ============================================================================
# Configuration
# ============================================================================
ITF = "disclosure"
DRY_RUN = False  # 先预览，确认无误再实跑


def main():
    repo_root = os.path.abspath(os.path.dirname(__file__) + "/../..")
    os.chdir(repo_root)
    assert os.path.isdir("data"), "data/ not found at repo root"
    assert ITF, "ITF must be set"

    print(f"[clean] itf={ITF} dry_run={DRY_RUN} root={repo_root}")

    files = sorted(glob.glob(f"data/*/{ITF}.parquet"))
    print(f"[clean] month files to delete: {len(files)}")
    for p in files:
        print(f"  rm {p}")
        if not DRY_RUN:
            os.remove(p)

    print("[clean] done" + (" (dry-run, no changes)" if DRY_RUN else ""))


if __name__ == "__main__":
    main()
