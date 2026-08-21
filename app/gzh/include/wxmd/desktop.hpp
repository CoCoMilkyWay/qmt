#pragma once

#include <string>

namespace wxmd {

// 桌面环境集成：系统代理设置与浏览器证书信任库。
// 这是全项目唯一知道「本机是 GNOME + NSS」的地方；换到别的环境只改这一层，
// 代理内核与微信解析都不受影响。

struct ProxyEndpoint {
  std::string host;
  int port = 0;
};

struct SystemProxy {
  std::string mode; // none / manual / auto
  ProxyEndpoint http;
  ProxyEndpoint https;
  ProxyEndpoint socks;
};

// 外部命令是否可用。
bool has_command(const std::string &name);

// 某个进程是否在跑。
bool process_running(const std::string &name);

// certutil 所属的包名，随发行版而异；认不出发行版时返回空。
std::string nss_tools_package();

// 安装某个包的完整命令（含 sudo），认不出包管理器时返回空。
std::string install_command(const std::string &package);

// 读当前系统代理设置。
SystemProxy read_system_proxy();

// 把系统代理指向 127.0.0.1:port，返回改动前的配置，供之后还原。
// socks 会被清空：本代理只讲 HTTP CONNECT，留着 socks 会被客户端优先选中。
SystemProxy take_over_system_proxy(int port);

// 还原系统代理。
void restore_system_proxy(const SystemProxy &saved);

// 接管前把原配置存盘。进程被 kill -9 时没机会还原，
// 靠这份备份让下次启动自己收拾干净，不必让用户手工敲还原命令。
void save_proxy_backup(const std::string &path, const SystemProxy &config);

// 有残留备份就还原并清除；返回是否真的做了还原。
bool restore_proxy_backup(const std::string &path);

void clear_proxy_backup(const std::string &path);

// 把 CA 装进用户 NSS 库（~/.pki/nssdb）。
// Chromium 系的 webview（微信文章页就是）从这里取用户添加的信任，不需要 root。
void install_ca_to_nssdb(const std::string &ca_cert_path);

// 从用户 NSS 库摘掉本工具的 CA 信任。没装过或没有 certutil 时是空操作。
void uninstall_ca_from_nssdb();

} // namespace wxmd
