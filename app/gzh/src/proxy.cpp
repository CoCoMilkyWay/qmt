#include "wxmd/proxy.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <thread>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "wxmd/assert.hpp"
#include "wxmd/tlsca.hpp"

#include "fsutil.hpp"
#include "strutil.hpp"

namespace wxmd {
namespace {

constexpr size_t kBufferSize = 32 * 1024;
constexpr size_t kMaxHeaderSize = 256 * 1024;
constexpr int kUpstreamTimeoutSeconds = 30;

// 只宣告 HTTP/1.1：宣告了 h2 客户端就会上 HTTP/2，那套二进制分帧我们不解析。
constexpr unsigned char kAlpnHttp11[] = {8,   'h', 't', 't', 'p',
                                         '/', '1', '.', '1'};

std::string proxy_openssl_error() {
  const unsigned long code = ERR_get_error();
  if (code == 0) {
    return "(无 OpenSSL 错误信息)";
  }
  char buffer[256];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  return buffer;
}

// ------------------------------------------------------------------ 字节流
// 明文 socket 与 TLS 会话共用一个读写接口，转发逻辑就不必关心底下是什么。
struct Stream {
  virtual ~Stream() = default;
  virtual int read(char *buffer, int length) = 0;
  virtual int write(const char *buffer, int length) = 0;
};

struct FdStream final : Stream {
  int fd = -1;

  explicit FdStream(int descriptor) : fd(descriptor) {}

  int read(char *buffer, int length) override {
    return static_cast<int>(::recv(fd, buffer, length, 0));
  }
  int write(const char *buffer, int length) override {
    return static_cast<int>(::send(fd, buffer, length, MSG_NOSIGNAL));
  }
};

struct SslStream final : Stream {
  SSL *ssl = nullptr;

  explicit SslStream(SSL *session) : ssl(session) {}

  int read(char *buffer, int length) override {
    return SSL_read(ssl, buffer, length);
  }
  int write(const char *buffer, int length) override {
    return SSL_write(ssl, buffer, length);
  }
};

bool write_all(Stream &stream, const char *buffer, size_t length) {
  size_t written = 0;
  while (written < length) {
    const int chunk =
        stream.write(buffer + written, static_cast<int>(length - written));
    if (chunk <= 0) {
      return false;
    }
    written += static_cast<size_t>(chunk);
  }
  return true;
}

bool write_all(Stream &stream, const std::string &text) {
  return write_all(stream, text.data(), text.size());
}

// 读到空行为止，返回整个头部（含结尾的 \r\n\r\n）。
// 多读到的字节是紧跟其后的报文体，通过 overflow 带出去。
bool read_headers(Stream &stream, std::string &headers, std::string &overflow) {
  headers.clear();
  overflow.clear();

  char buffer[kBufferSize];
  while (headers.size() < kMaxHeaderSize) {
    const size_t scan_from = headers.size() >= 3 ? headers.size() - 3 : 0;
    const int count = stream.read(buffer, sizeof(buffer));
    if (count <= 0) {
      return false;
    }
    headers.append(buffer, static_cast<size_t>(count));

    const size_t end = headers.find("\r\n\r\n", scan_from);
    if (end != std::string::npos) {
      overflow = headers.substr(end + 4);
      headers.resize(end + 4);
      return true;
    }
  }
  return false;
}

// CONNECT 必须精确停在头部结尾：多读出来的字节是 TLS ClientHello，
// 丢了握手就会静默失败。头部本身很短，逐字节读可以接受。
bool read_headers_exact(Stream &stream, std::string &headers) {
  headers.clear();
  char byte = 0;
  while (headers.size() < kMaxHeaderSize) {
    if (stream.read(&byte, 1) <= 0) {
      return false;
    }
    headers += byte;
    if (headers.size() >= 4 &&
        headers.compare(headers.size() - 4, 4, "\r\n\r\n") == 0) {
      return true;
    }
  }
  return false;
}

// --------------------------------------------------------------- HTTP 片段
std::string header_value(const std::string &headers, const std::string &name) {
  const std::string lowered = str::lowered(headers);
  const std::string needle = "\r\n" + str::lowered(name) + ":";

  const size_t at = lowered.find(needle);
  if (at == std::string::npos) {
    return {};
  }
  const size_t begin = at + needle.size();
  const size_t end = headers.find("\r\n", begin);
  return str::trim(headers.substr(begin, end - begin));
}

std::vector<std::string> collect_set_cookies(const std::string &headers) {
  const std::string lowered = str::lowered(headers);
  static const std::string kNeedle = "\r\nset-cookie:";

  std::vector<std::string> values;
  size_t at = lowered.find(kNeedle);
  while (at != std::string::npos) {
    const size_t begin = at + kNeedle.size();
    const size_t end = headers.find("\r\n", begin);
    values.push_back(str::trim(headers.substr(begin, end - begin)));
    at = lowered.find(kNeedle, end);
  }
  return values;
}

// 请求行形如 "GET /s?__biz=... HTTP/1.1"。
std::string request_path(const std::string &headers) {
  const size_t first = headers.find(' ');
  if (first == std::string::npos) {
    return {};
  }
  const size_t second = headers.find(' ', first + 1);
  if (second == std::string::npos) {
    return {};
  }
  return headers.substr(first + 1, second - first - 1);
}

// 把 keep-alive 改成 close：一个连接只跑一个来回，
// 响应体就能一路读到对端关闭为止，全部 HTTP 分帧逻辑都省了。
std::string force_connection_close(const std::string &headers) {
  std::string out;
  out.reserve(headers.size() + 32);

  size_t line_begin = 0;
  while (line_begin < headers.size()) {
    size_t line_end = headers.find("\r\n", line_begin);
    if (line_end == std::string::npos) {
      line_end = headers.size();
    }
    const std::string line = headers.substr(line_begin, line_end - line_begin);
    const std::string lowered = str::lowered(line);

    const bool drop = lowered.rfind("connection:", 0) == 0 ||
                      lowered.rfind("proxy-connection:", 0) == 0 ||
                      lowered.rfind("keep-alive:", 0) == 0;
    if (!drop) {
      out += line;
      out += "\r\n";
    }
    line_begin = line_end + 2;
  }

  // 此时 out 末尾是头部原有的空行，插在它前面。
  WXMD_ASSERT(out.size() >= 2, "请求头结构异常");
  out.insert(out.size() - 2, "Connection: close\r\n");
  return out;
}

// -------------------------------------------------------------------- 网络
int tcp_connect(const std::string &host, int port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *results = nullptr;
  const std::string service = std::to_string(port);
  if (::getaddrinfo(host.c_str(), service.c_str(), &hints, &results) != 0) {
    return -1;
  }

  int fd = -1;
  for (addrinfo *item = results; item != nullptr; item = item->ai_next) {
    fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (fd < 0) {
      continue;
    }
    if (::connect(fd, item->ai_addr, item->ai_addrlen) == 0) {
      break;
    }
    ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(results);

  if (fd >= 0) {
    timeval timeout{kUpstreamTimeoutSeconds, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  }
  return fd;
}

// 经父代理打隧道：先连父代理，再用 CONNECT 让它接到目标。
int tunnel_via_parent(const std::string &parent_host, int parent_port,
                      const std::string &host, int port) {
  const int fd = tcp_connect(parent_host, parent_port);
  if (fd < 0) {
    return -1;
  }

  const std::string authority = host + ":" + std::to_string(port);
  const std::string request =
      "CONNECT " + authority + " HTTP/1.1\r\nHost: " + authority + "\r\n\r\n";

  FdStream stream(fd);
  std::string reply;
  if (!write_all(stream, request) || !read_headers_exact(stream, reply) ||
      reply.find(" 200 ") == std::string::npos) {
    ::close(fd);
    return -1;
  }
  return fd;
}

// 出站统一走这里：配了父代理就打隧道，否则直连。
int open_upstream(const std::string &parent_host, int parent_port,
                  const std::string &host, int port) {
  if (parent_host.empty()) {
    return tcp_connect(host, port);
  }
  return tunnel_via_parent(parent_host, parent_port, host, port);
}

// 不劫持的域名：两个明文 fd 之间对拷，直到任一端关闭。
void splice_both_ways(int left, int right) {
  char buffer[kBufferSize];
  pollfd watch[2];
  watch[0] = {left, POLLIN, 0};
  watch[1] = {right, POLLIN, 0};

  for (;;) {
    watch[0].revents = 0;
    watch[1].revents = 0;
    if (::poll(watch, 2, kUpstreamTimeoutSeconds * 1000) <= 0) {
      return;
    }

    for (int side = 0; side < 2; ++side) {
      if ((watch[side].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
        continue;
      }
      const int from = side == 0 ? left : right;
      const int to = side == 0 ? right : left;

      const ssize_t count = ::recv(from, buffer, sizeof(buffer), 0);
      if (count <= 0) {
        return;
      }
      ssize_t written = 0;
      while (written < count) {
        const ssize_t chunk =
            ::send(to, buffer + written, count - written, MSG_NOSIGNAL);
        if (chunk <= 0) {
          return;
        }
        written += chunk;
      }
    }
  }
}

// 把一次往返的请求/响应原文落盘。dump_dir 非空时由 run_mitm 调用。
// 文件名 = 毫秒时间戳_序号.http，序号用原子计数器保证多线程不撞。
void write_dump(const std::string &dump_dir, const std::string &req,
                const std::string &resp) {
  if (dump_dir.empty() || (req.empty() && resp.empty())) {
    return;
  }
  static std::atomic<long long> counter{0};
  const long long seq = counter.fetch_add(1);
  const auto now = std::chrono::system_clock::now();
  const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();

  std::string dir = dump_dir;
  if (dir.empty() || dir.back() != '/') {
    dir += '/';
  }
  fsu::mkdirs(dir);
  const std::string path =
      dir + std::to_string(ms) + "_" + std::to_string(seq) + ".http";

  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    warn("dump 写不进去: " + path);
    return;
  }
  out << "==== REQUEST ====\r\n" << req << "\r\n==== RESPONSE ====\r\n" << resp;
}

// ---------------------------------------------------------------------- TLS
int alpn_select(SSL *, const unsigned char **out, unsigned char *out_length,
                const unsigned char *in, unsigned int in_length, void *) {
  if (SSL_select_next_proto(const_cast<unsigned char **>(out), out_length,
                            kAlpnHttp11, sizeof(kAlpnHttp11), in,
                            in_length) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  }
  return SSL_TLSEXT_ERR_OK;
}

SSL_CTX *make_server_context(const CertPair &pair) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  WXMD_ASSERT(ctx != nullptr, "创建 TLS 服务端上下文失败");

  BIO *cert_bio = BIO_new_mem_buf(pair.cert_pem.data(),
                                  static_cast<int>(pair.cert_pem.size()));
  X509 *leaf = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
  WXMD_ASSERT(leaf != nullptr, "读取叶子证书失败");
  WXMD_ASSERT(SSL_CTX_use_certificate(ctx, leaf) == 1, "装载叶子证书失败");

  // 证书链里剩下的是 CA，补进去客户端才能验到根。
  for (;;) {
    X509 *chained = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
    if (chained == nullptr) {
      break;
    }
    SSL_CTX_add_extra_chain_cert(ctx, chained);
  }
  ERR_clear_error();
  X509_free(leaf);
  BIO_free(cert_bio);

  BIO *key_bio = BIO_new_mem_buf(pair.key_pem.data(),
                                 static_cast<int>(pair.key_pem.size()));
  EVP_PKEY *key = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
  WXMD_ASSERT(key != nullptr, "读取叶子私钥失败");
  WXMD_ASSERT(SSL_CTX_use_PrivateKey(ctx, key) == 1, "装载叶子私钥失败");
  EVP_PKEY_free(key);
  BIO_free(key_bio);

  SSL_CTX_set_alpn_select_cb(ctx, alpn_select, nullptr);
  return ctx;
}

SSL_CTX *make_client_context() {
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  WXMD_ASSERT(ctx != nullptr, "创建 TLS 客户端上下文失败");
  WXMD_ASSERT(SSL_CTX_set_default_verify_paths(ctx) == 1, "加载系统根证书失败");
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
  SSL_CTX_set_alpn_protos(ctx, kAlpnHttp11, sizeof(kAlpnHttp11));
  return ctx;
}

} // namespace

// ======================================================================= Impl
struct MitmProxy::Impl {
  int port = 0;
  std::set<std::string> mitm_hosts;
  std::string parent_host;
  int parent_port = 0;
  CertAuthority ca;
  std::function<void(const Exchange &)> handler;
  std::string dump_dir; // 非空则对所有域名 MITM 并落盘每次往返

  int listen_fd = -1;
  std::atomic<bool> running{false};
  std::atomic<int> inflight{0};
  std::thread accept_thread;

  // 在飞的连接。stop() 靠它主动切断：微信会保持大量长连接经我们盲转发或 MITM，
  // 光等它们自己结束会无限期挂住，必须能把卡在读/转发里的线程踹醒。
  // client_fds 覆盖盲转发与 MITM 两条路（serve 入口就登记）；
  // upstream_fds 只在 MITM 路里登记——run_mitm 末尾会阻塞在 upstream.read
  // 上读完整响应体，客户端侧 shutdown 踹不醒它，stop() 必须连上游一起切。
  std::mutex conn_mutex;
  std::set<int> client_fds;
  std::set<int> upstream_fds;

  Impl(int listen_port, const std::vector<std::string> &hosts,
       const std::string &ca_dir, const std::string &up_host, int up_port)
      : port(listen_port), mitm_hosts(hosts.begin(), hosts.end()),
        parent_host(up_host), parent_port(up_port), ca(ca_dir) {}

  void serve(int client_fd);
  void run_mitm(int client_fd, const std::string &host, int upstream_port);
};

// 劫持路径：对客户端当服务端，对真实站点当客户端，中间抠出我们要的东西。
void MitmProxy::Impl::run_mitm(int client_fd, const std::string &host,
                               int upstream_port) {
  SSL_CTX *server_ctx = make_server_context(ca.issue(host));
  SSL *client_ssl = SSL_new(server_ctx);
  SSL_set_fd(client_ssl, client_fd);

  // 客户端不信任我们的 CA 时，握手就断在这里。
  if (SSL_accept(client_ssl) <= 0) {
    std::fprintf(stderr,
                 "[wxmd] 与客户端 TLS 握手失败，多半还没把 CA 装进信任库: %s\n",
                 proxy_openssl_error().c_str());
    SSL_free(client_ssl);
    SSL_CTX_free(server_ctx);
    return;
  }

  const int upstream_fd =
      open_upstream(parent_host, parent_port, host, upstream_port);
  if (upstream_fd < 0) {
    warn("连不上上游 " + host + ":" + std::to_string(upstream_port) +
         (parent_host.empty() ? "（直连失败）"
                              : "（经父代理 " + parent_host + " 失败）"));
    SSL_free(client_ssl);
    SSL_CTX_free(server_ctx);
    return;
  }
  {
    const std::lock_guard<std::mutex> guard(conn_mutex);
    upstream_fds.insert(upstream_fd);
  }

  SSL_CTX *client_ctx = make_client_context();
  SSL *server_ssl = SSL_new(client_ctx);
  SSL_set_fd(server_ssl, upstream_fd);
  SSL_set_tlsext_host_name(server_ssl, host.c_str());
  SSL_set1_host(server_ssl, host.c_str());

  if (SSL_connect(server_ssl) <= 0) {
    warn("与上游 " + host + " 的 TLS 握手失败: " + proxy_openssl_error());
  } else {
    SslStream downstream(client_ssl);
    SslStream upstream(server_ssl);

    std::string req_dump;
    std::string resp_dump;
    std::string headers;
    std::string overflow;
    if (read_headers(downstream, headers, overflow)) {
      req_dump = headers;
      req_dump += overflow;
      const std::string url = "https://" + host + request_path(headers);

      if (write_all(upstream, force_connection_close(headers)) &&
          write_all(upstream, overflow)) {
        // 请求体：目标流量都是 GET，有 body 时按 Content-Length 补齐即可。
        const std::string length_text = header_value(headers, "Content-Length");
        const long long declared =
            length_text.empty()
                ? 0
                : std::strtoll(length_text.c_str(), nullptr, 10);

        long long remaining =
            declared - static_cast<long long>(overflow.size());
        char buffer[kBufferSize];
        while (remaining > 0) {
          const int wanted = static_cast<int>(std::min<long long>(
              remaining, static_cast<long long>(sizeof(buffer))));
          const int count = downstream.read(buffer, wanted);
          if (count <= 0 ||
              !write_all(upstream, buffer, static_cast<size_t>(count))) {
            break;
          }
          req_dump.append(buffer, static_cast<size_t>(count));
          remaining -= count;
        }

        std::string response;
        std::string response_overflow;
        if (read_headers(upstream, response, response_overflow)) {
          resp_dump = response;
          resp_dump += response_overflow;
          if (handler) {
            Exchange exchange;
            exchange.url = url;
            exchange.cookie = header_value(headers, "Cookie");
            exchange.set_cookies = collect_set_cookies(response);
            handler(exchange);
          }

          if (write_all(downstream, response) &&
              write_all(downstream, response_overflow)) {
            // 上游已被要求 close，一路读到它关闭就是完整响应体。
            for (;;) {
              const int count = upstream.read(buffer, sizeof(buffer));
              if (count <= 0 ||
                  !write_all(downstream, buffer, static_cast<size_t>(count))) {
                break;
              }
              resp_dump.append(buffer, static_cast<size_t>(count));
            }
          }
        }
      }
    }
    write_dump(dump_dir, req_dump, resp_dump);
  }

  SSL_shutdown(server_ssl);
  SSL_free(server_ssl);
  SSL_CTX_free(client_ctx);
  {
    const std::lock_guard<std::mutex> guard(conn_mutex);
    upstream_fds.erase(upstream_fd);
  }
  ::close(upstream_fd);

  SSL_shutdown(client_ssl);
  SSL_free(client_ssl);
  SSL_CTX_free(server_ctx);
}

void MitmProxy::Impl::serve(int client_fd) {
  FdStream client(client_fd);

  std::string headers;
  if (!read_headers_exact(client, headers)) {
    return;
  }

  // 明文 GET 只用来下发 CA，方便任意环境把证书拷走。
  if (headers.rfind("GET /wxmd-ca.crt", 0) == 0 ||
      headers.rfind("GET /wxmd-ca.pem", 0) == 0) {
    const std::string body = [&] {
      std::ifstream input(ca.ca_cert_path(), std::ios::binary);
      WXMD_ASSERT(input.is_open(), "读不到 CA 证书: " + ca.ca_cert_path());
      return std::string(std::istreambuf_iterator<char>(input),
                         std::istreambuf_iterator<char>());
    }();
    const std::string reply = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/x-x509-ca-cert\r\n"
                              "Content-Length: " +
                              std::to_string(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body;
    write_all(client, reply);
    return;
  }

  // 只处理 CONNECT；其它明文 HTTP 代理请求不是目标流量。
  if (headers.rfind("CONNECT ", 0) != 0) {
    const std::string refuse =
        "HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n\r\n";
    write_all(client, refuse);
    return;
  }

  const std::string authority = request_path(headers);
  const size_t colon = authority.rfind(':');
  const std::string host =
      colon == std::string::npos ? authority : authority.substr(0, colon);
  const int port = colon == std::string::npos
                       ? 443
                       : static_cast<int>(std::strtol(
                             authority.c_str() + colon + 1, nullptr, 10));

  const std::string established = "HTTP/1.1 200 Connection Established\r\n\r\n";
  if (!write_all(client, established)) {
    return;
  }

  if (!dump_dir.empty() || mitm_hosts.count(host) > 0) {
    run_mitm(client_fd, host, port);
  } else {
    const int upstream_fd = open_upstream(parent_host, parent_port, host, port);
    if (upstream_fd < 0) {
      warn("盲转发连不上 " + host + ":" + std::to_string(port) +
           (parent_host.empty() ? "（直连失败）"
                                : "（经父代理 " + parent_host + " 失败）"));
    } else {
      splice_both_ways(client_fd, upstream_fd);
      ::close(upstream_fd);
    }
  }
}

// ======================================================================= 接口
MitmProxy::MitmProxy(int port, std::vector<std::string> mitm_hosts,
                     const std::string &ca_dir, const std::string &parent_host,
                     int parent_port)
    : impl_(std::make_unique<Impl>(port, mitm_hosts, ca_dir, parent_host,
                                   parent_port)) {}

MitmProxy::~MitmProxy() { stop(); }

void MitmProxy::set_handler(std::function<void(const Exchange &)> handler) {
  impl_->handler = std::move(handler);
}

void MitmProxy::set_dump_dir(const std::string &dump_dir) {
  impl_->dump_dir = dump_dir;
}

const std::string &MitmProxy::ca_cert_path() const {
  return impl_->ca.ca_cert_path();
}

int MitmProxy::port() const { return impl_->port; }

void MitmProxy::start() {
  impl_->listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  WXMD_ASSERT(impl_->listen_fd >= 0, "创建监听 socket 失败");

  const int reuse = 1;
  ::setsockopt(impl_->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
               sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  // 只监听回环：这是个会用自签 CA 现签证书的中间人，绝不能暴露到局域网。
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  // port 传 0 时交给内核分配，绑定后回读真实端口，天然避开占用与多实例冲突。
  address.sin_port = htons(static_cast<uint16_t>(impl_->port));

  WXMD_ASSERT(::bind(impl_->listen_fd, reinterpret_cast<sockaddr *>(&address),
                     sizeof(address)) == 0,
              "端口 " + std::to_string(impl_->port) +
                  " 绑定失败，可能已被占用");
  WXMD_ASSERT(::listen(impl_->listen_fd, 64) == 0, "监听失败");

  // 回读内核实际分配的端口：port 传 0 时这是唯一拿到真实端口的途径，
  // 固定端口时值不变。之后 take_over_system_proxy 要用它。
  sockaddr_in bound{};
  socklen_t bound_len = sizeof(bound);
  WXMD_ASSERT(::getsockname(impl_->listen_fd,
                            reinterpret_cast<sockaddr *>(&bound),
                            &bound_len) == 0,
              "读取监听端口失败");
  impl_->port = ntohs(bound.sin_port);

  impl_->running = true;
  impl_->accept_thread = std::thread([this] {
    while (impl_->running) {
      const int client_fd = ::accept(impl_->listen_fd, nullptr, nullptr);
      if (client_fd < 0) {
        if (impl_->running) {
          warn(std::string("accept 失败，继续等待: ") + std::strerror(errno));
          continue;
        }
        break;
      }
      impl_->inflight.fetch_add(1);
      std::thread([this, client_fd] {
        {
          const std::lock_guard<std::mutex> guard(impl_->conn_mutex);
          impl_->client_fds.insert(client_fd);
        }
        impl_->serve(client_fd);
        {
          const std::lock_guard<std::mutex> guard(impl_->conn_mutex);
          impl_->client_fds.erase(client_fd);
        }
        ::close(client_fd);
        impl_->inflight.fetch_sub(1);
      }).detach();
    }
  });
}

void MitmProxy::stop() {
  if (!impl_->running.exchange(false)) {
    return;
  }
  if (impl_->listen_fd >= 0) {
    ::shutdown(impl_->listen_fd, SHUT_RDWR);
    ::close(impl_->listen_fd);
    impl_->listen_fd = -1;
  }
  if (impl_->accept_thread.joinable()) {
    impl_->accept_thread.join();
  }

  // 主动切断所有在飞连接：微信保持的长连接不会自己结束，
  // shutdown 会把卡在 splice/读里的线程立刻踹醒，inflight 才能很快归零。
  // 上游 fd 也要一起切：MITM 路末尾阻塞在 upstream.read 读完整响应体，
  // 只切客户端侧踹不醒它（dump 末尾那些 video 长连接就是这么把 stop()
  // 挂死的）。
  {
    const std::lock_guard<std::mutex> guard(impl_->conn_mutex);
    for (const int fd : impl_->client_fds) {
      ::shutdown(fd, SHUT_RDWR);
    }
    for (const int fd : impl_->upstream_fds) {
      ::shutdown(fd, SHUT_RDWR);
    }
  }

  while (impl_->inflight.load() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

} // namespace wxmd
