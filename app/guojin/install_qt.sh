set -e

SRC_DLL="installer/Qt5Svg.dll"
TARGET_DIR="gjqmt/bin.x64"
TARGET_DLL="$TARGET_DIR/Qt5Svg.dll"

echo "==> 拷贝 Qt5Svg.dll 到 $TARGET_DIR"

[ -f "$SRC_DLL" ] || { echo "源 DLL 不存在：$SRC_DLL" >&2; exit 1; }
[ -d "$TARGET_DIR" ] || { echo "目标目录不存在：$TARGET_DIR" >&2; exit 1; }
cp "$SRC_DLL" "$TARGET_DLL"

echo "==> 验证"

[ -f "$TARGET_DLL" ] || { echo "Qt5Svg.dll 不存在：$TARGET_DLL" >&2; exit 1; }
echo "Qt5Svg.dll 已就位：$TARGET_DLL"
