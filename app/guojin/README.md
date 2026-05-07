===========================================================
~/.bashrc 加入:
# Wine 中文环境
export LANG=zh_CN.UTF-8
export LANGUAGE=zh_CN:zh
export LC_ALL=zh_CN.UTF-8

export WINEARCH=win64
export WINEPREFIX=$HOME/.wine

===========================================================
sudo apt install wine64 wine32

bash ~/work/qmt/app/guojin/install_gecko.sh

sudo apt install mesa-utils
glxinfo | grep OpenGL
glxgears

winetricks corefonts allfonts
winetricks mfc42 gdiplus riched20 msxml3 msxml6 vcrun6 vcrun2015

===========================================================
cd ~/work/qmt/app/guojin/gjqmt/bin.x64 && wine XtItClient.exe
cd ~/work/qmt/app/guojin/gjzq && wine TdxW.exe

===========================================================

