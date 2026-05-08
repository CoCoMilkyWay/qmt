import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FONTS = sorted(HERE.glob("*.ttf")) + sorted(HERE.glob("*.otf"))
assert FONTS, f"未在 {HERE} 找到任何 .ttf/.otf 字体"


def install_linux():
    dst_dir = Path.home() / ".local/share/fonts"
    dst_dir.mkdir(parents=True, exist_ok=True)
    for src in FONTS:
        dst = dst_dir / src.name
        shutil.copy2(src, dst)
        print(f"copied: {dst}")
    fc_cache = shutil.which("fc-cache")
    assert fc_cache, "未找到 fc-cache，请先安装 fontconfig"
    subprocess.run([fc_cache, "-fv", str(dst_dir)], check=True)


def install_macos():
    dst_dir = Path.home() / "Library/Fonts"
    dst_dir.mkdir(parents=True, exist_ok=True)
    for src in FONTS:
        dst = dst_dir / src.name
        shutil.copy2(src, dst)
        print(f"copied: {dst}")


def install_windows():
    import winreg  # noqa: PLC0415

    local_appdata = os.environ.get("LOCALAPPDATA")
    assert local_appdata, "缺少环境变量 LOCALAPPDATA"
    dst_dir = Path(local_appdata) / "Microsoft/Windows/Fonts"
    dst_dir.mkdir(parents=True, exist_ok=True)

    reg_key = r"Software\Microsoft\Windows NT\CurrentVersion\Fonts"
    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER, reg_key, 0, winreg.KEY_SET_VALUE
    ) as key:
        for src in FONTS:
            dst = dst_dir / src.name
            shutil.copy2(src, dst)
            suffix = " (OpenType)" if src.suffix.lower() == ".otf" else " (TrueType)"
            value_name = f"{src.stem}{suffix}"
            winreg.SetValueEx(key, value_name, 0, winreg.REG_SZ, str(dst))
            print(f"copied: {dst}")
            print(f"  reg : HKCU\\{reg_key}\\{value_name}")

    HWND_BROADCAST = 0xFFFF
    WM_FONTCHANGE = 0x001D
    import ctypes  # noqa: PLC0415

    ctypes.windll.user32.SendMessageW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0)


PLATFORMS = {
    "linux": install_linux,
    "darwin": install_macos,
    "win32": install_windows,
}

assert sys.platform in PLATFORMS, f"不支持的平台: {sys.platform}"
PLATFORMS[sys.platform]()
print("done. 重启编辑器后字体生效")
