"""Build & Run Pipeline (Linux only)

Usage:
    Set ONE mode flag to True, then: python run.py

Modes:
    - DEBUG:      Core-dump capture + auto bt analysis
    - PROFILE:    CPU profiling with Tracy
    - ASSERT:     Optimized build with assertions
    - PRODUCTION: Maximum performance (-O3, fastest)
"""
import os
import subprocess
import sys
import time

from py import mode_debug, mode_profile, mode_assert, mode_production

# ============================================================================
# Configuration
# ============================================================================
APP_NAME = "main"

# Build & Run modes (set ONLY ONE to True)
ENABLE_DEBUG = False
ENABLE_PROFILE = False
ENABLE_ASSERT = True
ENABLE_PRODUCTION = False  # Auto-enabled if all others are False


def _cleanup_processes():
    subprocess.run(["pkill", "-f", f"app_{APP_NAME}"],
                   capture_output=True, check=False)
    time.sleep(0.3)


def _build(app_name, enable_debug, enable_profile, enable_assert):
    py_script = f"py/{app_name}.py"
    assert os.path.exists(py_script), f"Build script not found: {py_script}"

    env = os.environ.copy()
    env['DEBUG_MODE'] = 'ON' if enable_debug else 'OFF'
    env['PROFILE_MODE'] = 'ON' if enable_profile else 'OFF'
    env['ASSERT_MODE'] = 'ON' if enable_assert else 'OFF'

    result = subprocess.run(["python", py_script], env=env)
    assert result.returncode == 0, f"Build failed (exit {result.returncode})"


def _run(binary_path, working_dir, enable_debug, enable_profile, enable_assert):
    assert os.path.exists(binary_path), f"Binary not found: {binary_path}"

    if enable_debug:
        mode_debug.run(binary_path, working_dir)
    elif enable_profile:
        mode_profile.run(binary_path, working_dir)
    elif enable_assert:
        mode_assert.run(binary_path, working_dir)
    else:
        mode_production.run(binary_path, working_dir)


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    print("Cleanup...")
    _cleanup_processes()

    print("Building...")
    _build(APP_NAME, ENABLE_DEBUG, ENABLE_PROFILE, ENABLE_ASSERT)

    print("Running...")
    build_dir = os.path.abspath(f"cpp/projects/{APP_NAME}/build")
    binary_path = os.path.join(build_dir, f"bin/app_{APP_NAME}")
    _run(binary_path, build_dir,
         ENABLE_DEBUG, ENABLE_PROFILE, ENABLE_ASSERT)


if __name__ == "__main__":
    main()
