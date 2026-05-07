sudo apt install wine64 wine32

bash ./install_gecko.sh

sudo apt install mesa-utils
glxinfo | grep OpenGL
glxgears

winetricks fakechinese corefonts allfonts
winetricks mfc42 gdiplus riched20 msxml3 vcrun6


