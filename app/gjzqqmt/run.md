# Linux Wine 跑国金 QMT
https://download.gjzq.com.cn/gjty/organ/gjzqqmt.rar
安装器 32 位, 客户端 64 位 (`bin.x64`). 必须 wine64+wine32 都装, 用默认 wow64 prefix, 启动带中文 locale.

```bash
# 1. wine + i386
sudo apt install -y wine wine64 winetricks cabextract fonts-noto-cjk
sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install -y wine32:i386

# 2. 中文 locale
sudo locale-gen zh_CN.UTF-8 && sudo update-locale

# 3. 重建 64 位 wow64 prefix (如有旧的 win32 prefix 必须删)
rm -rf ~/.wine && wineboot

# 4. 中文字体替换表
WINEPREFIX=~/.wine winetricks -q cjkfonts

# 5. 装 QMT
cd ~/work/qmt/gjzqqmt
LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 wine gjqmt_setup.exe

# 6. 启动客户端 (XtItClient=主端, XtMiniQmt=API端)
echo "alias winecn='LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 WINEPREFIX=$HOME/.wine wine'" >> ~/.bashrc
source ~/.bashrc
cd ~/work/qmt/gjzqqmt/国金证券QMT交易端/bin.x64
winecn XtItClient.exe
```
