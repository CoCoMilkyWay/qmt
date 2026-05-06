"""
Build script for 'main' C++ project (Linux only).
Directly invokes CMake to configure and build the project.
"""

import os
import shutil
import subprocess


def build_main_project():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    cpp_project_dir = os.path.abspath(
        os.path.join(script_dir, "../cpp/projects/main"))
    build_dir = os.path.join(cpp_project_dir, "build")

    print("=" * 40)
    print("  C++ Build Script (Linux)")
    print("=" * 40)

    env = os.environ.copy()
    build_type = "Release"

    cmake_args = [
        "cmake",
        "-S", ".",
        "-B", "build",
        "-G", "Ninja",
        "-DCMAKE_CXX_COMPILER=clang++",
        "-DCMAKE_C_COMPILER=clang",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]

    for flag in ("PROFILE_MODE", "DEBUG_MODE", "ASSERT_MODE"):
        on = env.get(flag) == 'ON'
        if on:
            print(f"{flag}: ENABLED")
        cmake_args.append(f"-D{flag}={'ON' if on else 'OFF'}")

    print("\nConfiguring with CMake...")
    result = subprocess.run(cmake_args, cwd=cpp_project_dir, env=env)
    assert result.returncode == 0, f"CMake configure failed (exit {result.returncode})"

    print("\nBuilding...")
    result = subprocess.run(
        ["cmake", "--build", "build", "--config", build_type, "--parallel"],
        cwd=cpp_project_dir,
        env=env
    )
    assert result.returncode == 0, f"CMake build failed (exit {result.returncode})"

    compile_commands = os.path.join(build_dir, "compile_commands.json")
    if os.path.exists(compile_commands):
        dest = os.path.join(cpp_project_dir, "../../compile_commands.json")
        shutil.copy(compile_commands, dest)
        print("\ncompile_commands.json copied for clangd")

    print("\n" + "=" * 40)
    print("  Build completed successfully!")
    print("=" * 40)


if __name__ == '__main__':
    build_main_project()
