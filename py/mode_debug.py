"""Debug mode: Linux core-dump capture with auto bt analysis (gdb)"""
import glob
import os
import resource
import subprocess
import time


def _find_latest_core(debug_dir):
    cores = glob.glob(f"{debug_dir}/core*")
    return max(cores, key=os.path.getmtime) if cores else None


def _analyze_core(core_path, binary_path):
    output_dir = os.path.dirname(core_path)
    stack_file = os.path.join(output_dir, "crash_stacks.txt")

    gdb_script = (
        "set pagination off\n"
        "set print pretty on\n"
        "set print frame-arguments all\n"
        "echo ======================================\\n\n"
        "echo CRASH ANALYSIS\\n\n"
        "echo ======================================\\n\n"
        "info registers\n"
        "echo \\n======================================\\n\n"
        "echo CURRENT THREAD BACKTRACE (full)\\n\n"
        "echo ======================================\\n\n"
        "bt full\n"
        "echo \\n======================================\\n\n"
        "echo ALL THREAD BACKTRACES\\n\n"
        "echo ======================================\\n\n"
        "thread apply all bt full\n"
        "echo \\n======================================\\n\n"
        "echo ANALYSIS COMPLETE\\n\n"
        "echo ======================================\\n\n"
        "quit\n"
    )

    script_path = os.path.join(output_dir, "analyze.gdb")
    with open(script_path, 'w') as f:
        f.write(gdb_script)

    print(f"Analyzing core: {core_path}")
    with open(stack_file, 'w') as out:
        subprocess.run(
            ["gdb", "-batch", "-x", script_path, binary_path, core_path],
            stdout=out, stderr=subprocess.STDOUT,
        )

    if os.path.exists(stack_file):
        size_kb = os.path.getsize(stack_file) / 1024.0
        print(f"Stacks saved to: {stack_file} ({size_kb:.1f} KB)")


def run(binary_path, working_dir):
    debug_dir = os.path.abspath("output/debug")
    os.makedirs(debug_dir, exist_ok=True)

    resource.setrlimit(resource.RLIMIT_CORE,
                       (resource.RLIM_INFINITY, resource.RLIM_INFINITY))

    env = os.environ.copy()

    print("Running with core-dump capture (RLIMIT_CORE=infinity)...")
    print(f"Crash cores will be written to: {debug_dir}")
    print("(NOTE: kernel core_pattern must allow writing here; e.g. 'core' in cwd)")
    print("Press Ctrl+C to exit\n")
    start_time = time.time()

    proc = subprocess.Popen([binary_path], cwd=debug_dir, env=env)
    proc.wait()
    elapsed_time = time.time() - start_time

    print(f"\n{'='*80}")
    if proc.returncode == 0:
        print(f"Debug Complete! ({elapsed_time:.2f}s)")
    else:
        print(f"Crashed with code {proc.returncode} ({elapsed_time:.2f}s)")
        core = _find_latest_core(debug_dir)
        if core:
            print(f"Core dump: {core}")
            _analyze_core(core, binary_path)
        else:
            print("No core file found in output/debug.")
            print("Check kernel.core_pattern: `sysctl kernel.core_pattern`")
            print("(if it pipes to systemd-coredump, use `coredumpctl debug` instead)")
    print(f"{'='*80}\n")
