#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wxmd {

// 被劫持域名上的一次 HTTP
// 往返。代理只负责把这些字段抠出来，怎么解释由调用方决定。
struct Exchange {
  std::string url;                      // https://域名 + 请求路径
  std::string cookie;                   // 请求里的 Cookie 头，可能为空
  std::vector<std::string> set_cookies; // 响应里全部 Set-Cookie 的原文
};

// 一个 CONNECT 代理：只对 mitm_hosts 里的域名做中间人，其余域名纯字节盲转发。
// 它不知道微信的存在——抓到的东西怎么解释，完全由 handler 决定。
class MitmProxy {
public:
  // ca_dir 用来存放自签 CA；同一个目录复用同一张 CA，客户端只需信任一次。
  // parent_host 非空时，所有出站连接都经由该父代理（HTTP CONNECT）转发，
  // 这样接管系统代理后，原本走机场的流量仍照原路走。
  // port 传 0 表示监听端口交给内核分配，start() 之后用 port() 取回实际值。
  // 只监听 127.0.0.1，不会暴露到局域网。
  MitmProxy(int port, std::vector<std::string> mitm_hosts,
            const std::string &ca_dir, const std::string &parent_host = "",
            int parent_port = 0);
  ~MitmProxy();

  MitmProxy(const MitmProxy &) = delete;
  MitmProxy &operator=(const MitmProxy &) = delete;

  // 必须在 start 之前设置。handler 在连接线程上被调用，实现需自行保证线程安全。
  void set_handler(std::function<void(const Exchange &)> handler);

  // 必须在 start 之前设置。dump_dir 非空时：对所有域名都做中间人（不再只限
  // mitm_hosts），并把每次往返的请求头/请求体/响应头/响应体落盘到该目录，
  // 一个文件一次往返。用于抓包分析微信客户端实际调用的接口。
  void set_dump_dir(const std::string &dump_dir);

  void start();
  void stop();

  // 要装进客户端信任库的 CA 证书路径。
  const std::string &ca_cert_path() const;
  int port() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace wxmd
