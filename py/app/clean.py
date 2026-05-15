"""Clean tushare local data for a single interface.

Removes:
  - data/YYYY/MM/DD/<ITF>.json        (all per-day data files)
  - <ITF> key from every data/YYYY/MM/_empty.json
    (deletes the manifest entirely if it becomes empty)

Usage:
    Set ITF below, then: python py/app/clean.py
"""

import glob
import json
import os

# ============================================================================
# Configuration
# ============================================================================
ITF = "disclosure"
DRY_RUN = False  # 先预览，确认无误再实跑


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    os.chdir(repo_root)
    assert os.path.isdir("data"), "data/ not found at repo root"
    assert ITF, "ITF must be set"

    print(f"[clean] itf={ITF} dry_run={DRY_RUN} root={repo_root}")

    day_files = sorted(glob.glob(f"data/*/*/*/{ITF}.json"))
    print(f"[clean] day files to delete: {len(day_files)}")
    for p in day_files:
        print(f"  rm {p}")
        if not DRY_RUN:
            os.remove(p)

    manifests = sorted(glob.glob("data/*/*/_empty.json"))
    n_keys_removed = 0
    n_manifests_removed = 0
    n_manifests_rewritten = 0
    for p in manifests:
        with open(p, "r", encoding="utf-8") as f:
            obj = json.load(f)
        assert isinstance(obj, dict), f"{p}: expected object"
        if ITF not in obj:
            continue
        del obj[ITF]
        n_keys_removed += 1
        if not obj:
            print(f"  rm {p} (now empty)")
            n_manifests_removed += 1
            if not DRY_RUN:
                os.remove(p)
        else:
            print(f"  upd {p} (drop key)")
            n_manifests_rewritten += 1
            if not DRY_RUN:
                with open(p, "w", encoding="utf-8") as f:
                    json.dump(obj, f, indent=2, sort_keys=True, ensure_ascii=False)
                    f.write("\n")

    print(
        f"[clean] _empty keys removed: {n_keys_removed} "
        f"(manifests rewritten={n_manifests_rewritten}, "
        f"manifests deleted={n_manifests_removed})"
    )
    print("[clean] done" + (" (dry-run, no changes)" if DRY_RUN else ""))


if __name__ == "__main__":
    main()
