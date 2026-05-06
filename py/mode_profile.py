"""Profile Mode - CPU profiling with Tracy (Linux)"""
import os
import subprocess
import time
from typing import Optional


def _repo_root() -> str:
    # py/ is directly under repo root
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _find_tracy_profiler_exe() -> Optional[str]:
    override = os.environ.get("TRACY_PROFILER_EXE") or os.environ.get("TRACY_PROFILER_PATH")
    if override:
        override_path = os.path.abspath(override)
        if os.path.isfile(override_path) and os.access(override_path, os.X_OK):
            return override_path

    candidate = os.path.join(_repo_root(), "cpp", "package", "tracy", "tracy-profiler")
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate

    from shutil import which
    for name in ("tracy-profiler", "tracy"):
        found = which(name)
        if found:
            return found

    return None


def _launch_tracy_ui() -> Optional[subprocess.Popen]:
    tracy_exe = _find_tracy_profiler_exe()
    if not tracy_exe:
        print("[Tracy] tracy-profiler not found (skipping UI auto-launch).")
        print("        Install Tracy UI or drop binary at cpp/package/tracy/tracy-profiler,")
        print("        or set TRACY_PROFILER_EXE.")
        return None

    proc = subprocess.Popen(
        [tracy_exe, "-a", "localhost"],
        cwd=os.path.dirname(tracy_exe),
        start_new_session=True,
    )
    print(f"[Tracy] UI launched with auto-connect: {tracy_exe}")
    return proc


def run(binary_path, working_dir):
    """Run with Tracy profiler."""
    print(f"\n{'='*80}")
    print("Tracy Profiler Mode")
    print(f"{'='*80}\n")
    
    # Launch Tracy UI with auto-connect
    _launch_tracy_ui()
    time.sleep(1.0)

    print("Starting application with tracy profiling...")
    print("Tracy UI will auto-connect to localhost")
    print("Press Ctrl+C to stop\n")
    
    # Run binary
    start_time = time.time()
    proc = subprocess.Popen([binary_path], cwd=working_dir)
    try:
        return_code = proc.wait()
    except KeyboardInterrupt:
        print("\n[Interrupted] Stopping application...")
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
        return_code = proc.returncode if proc.returncode is not None else 1
    elapsed_time = time.time() - start_time
    
    print(f"\n{'='*80}")
    if return_code == 0:
        print(f"[OK] Profile Complete! ({elapsed_time:.2f}s)")
    else:
        print(f"[X] Profile exited with code {return_code} ({elapsed_time:.2f}s)")
    print(f"{'='*80}")

