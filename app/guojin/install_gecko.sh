set -e

INSTALLER_DIR="installer"

mkdir -p "$INSTALLER_DIR"

echo "==> 查询最新 Gecko 版本"

LATEST_GECKO=$(curl -s https://dl.winehq.org/wine/wine-gecko/ \
    | grep -oP 'href="\K[0-9.]+(?=/")' \
    | sort -V \
    | tail -1)

GECKO_FILE="wine-gecko-${LATEST_GECKO}-x86_64.msi"
GECKO_URL="https://dl.winehq.org/wine/wine-gecko/${LATEST_GECKO}/${GECKO_FILE}"
GECKO_INSTALL_DIR="$HOME/.wine/drive_c/windows/system32/gecko/$LATEST_GECKO"

echo "==> 下载 Gecko Installer"

if [ ! -f "$INSTALLER_DIR/$GECKO_FILE" ]; then
    wget -O "$INSTALLER_DIR/$GECKO_FILE" "$GECKO_URL"
fi

echo "==> 运行 Gecko Installer（Wine）"

wine msiexec /i "$INSTALLER_DIR/$GECKO_FILE"

echo "==> 验证安装"

[ -d "$GECKO_INSTALL_DIR" ] || { echo "Gecko 未安装：$GECKO_INSTALL_DIR 不存在" >&2; exit 1; }
echo "Gecko 已安装：$GECKO_INSTALL_DIR"
