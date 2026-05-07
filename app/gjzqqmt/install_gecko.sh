wine_arch=$(wine cmd /c echo %PROCESSOR_ARCHITECTURE% 2>/dev/null | tr -d '\r')

if [[ "$wine_arch" == "AMD64" ]]; then
    gecko_platform="x86_64"
else
    gecko_platform="x86"
fi

latest_gecko=$(curl -s https://dl.winehq.org/wine/wine-gecko/ \
| grep -oP 'href="\K[0-9.]+(?=/")' \
| sort -V \
| tail -1)

gecko_file="wine-gecko-${latest_gecko}-${gecko_platform}.msi"

gecko_url="https://dl.winehq.org/wine/wine-gecko/${latest_gecko}/${gecko_file}"

cd /tmp || exit 1

wget -O "$gecko_file" "$gecko_url"

wine msiexec /i "$gecko_file"