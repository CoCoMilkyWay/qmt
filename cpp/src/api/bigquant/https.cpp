#include "api/bigquant/https.hpp"

#include "api/bigquant/signer.hpp"
#include "config.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace bigquant {

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace {

// 单次请求 (一次性 TLS 握手 + read/write). 不带重试 / 校验, 仅作为 retry 上层的最小单元.
std::pair<int, std::string> request_once(http::verb verb, std::string_view path,
                                         std::string_view body,
                                         const SignedHeaders &h) {
  net::io_context ioc;
  // tls_client = 让 OpenSSL 与 server 协商 TLS 1.2/1.3, bigquant.com 默认 1.3.
  ssl::context ctx(ssl::context::tls_client);
  ctx.set_default_verify_paths();
  ctx.set_verify_mode(ssl::verify_peer);

  tcp::resolver resolver(ioc);
  beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

  // SNI — 缺则握手报 SSL: alert handshake failure
  bool sni_ok = ::SSL_set_tlsext_host_name(stream.native_handle(),
                                           ::config::BIGQUANT_HTTPS_HOST);
  assert(sni_ok && "SSL_set_tlsext_host_name failed");

  auto results = resolver.resolve(::config::BIGQUANT_HTTPS_HOST,
                                  ::config::BIGQUANT_HTTPS_PORT);
  beast::get_lowest_layer(stream).expires_after(
      std::chrono::seconds(::config::BIGQUANT_HTTPS_TIMEOUT_SECONDS));
  beast::get_lowest_layer(stream).connect(results);
  stream.handshake(ssl::stream_base::client);

  std::string target(path);
  http::request<http::string_body> req{verb, target, 11};
  req.set(http::field::host, ::config::BIGQUANT_HTTPS_HOST);
  req.set(http::field::user_agent, "qmt-bq/1.0");
  req.set("X-BigQuant-Access-Key", h.access_key);
  req.set("X-BigQuant-Timestamp", h.timestamp);
  req.set("X-BigQuant-Signature", h.signature);
  if (verb == http::verb::post) {
    req.set(http::field::content_type, "application/json");
    req.body().assign(body.data(), body.size());
    req.prepare_payload();
  }

  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  beast::error_code ec;
  [[maybe_unused]] auto shutdown_ec = stream.shutdown(ec);
  // truncation / stream_truncated 是 TLS 关闭时常见 benign 错误, 忽略.

  int status = static_cast<int>(res.result_int());
  return {status, std::move(res.body())};
}

// 解析 {code, reason, message, data}; code != 0 直接 assert.
// 返回 yyjson_doc*, 其 root 已指向原 `data` 字段拷贝 (caller free).
yyjson_doc *parse_envelope(std::string_view path, const std::string &body) {
  yyjson_doc *doc = yyjson_read(body.data(), body.size(), 0);
  if (!doc) {
    std::cerr << "[bigquant.https] parse error path=" << path << " body=\n"
              << body.substr(0, 1024) << std::endl;
    assert(false && "response is not valid JSON");
  }
  yyjson_val *root = yyjson_doc_get_root(doc);
  yyjson_val *code_v = yyjson_obj_get(root, "code");
  assert(code_v && (yyjson_is_int(code_v) || yyjson_is_uint(code_v)) &&
         "response missing integer 'code'");
  int64_t code = yyjson_get_int(code_v);
  if (code != 0) {
    yyjson_val *reason_v = yyjson_obj_get(root, "reason");
    yyjson_val *msg_v = yyjson_obj_get(root, "message");
    const char *reason = (reason_v && yyjson_is_str(reason_v)) ? yyjson_get_str(reason_v) : "";
    const char *msg = (msg_v && yyjson_is_str(msg_v)) ? yyjson_get_str(msg_v) : "";
    std::cerr << "[bigquant.https] api error path=" << path << " code=" << code
              << " reason=" << reason << " message=" << msg << std::endl;
    yyjson_doc_free(doc);
    assert(false && "BigQuant API returned non-zero code");
  }

  yyjson_val *data = yyjson_obj_get(root, "data");
  assert(data && "response missing 'data' field");

  // 把 data 重新序列化 + 解析, 让 caller 拿到的 doc root 就是 data 本身.
  // 简化下游 yyjson_obj_get 用法, 也避免 caller 误持有外层 doc 的子节点.
  size_t out_len = 0;
  char *out = yyjson_val_write(data, 0, &out_len);
  assert(out && "yyjson_val_write(data) failed");
  yyjson_doc *data_doc = yyjson_read(out, out_len, 0);
  std::free(out);
  yyjson_doc_free(doc);
  assert(data_doc);
  return data_doc;
}

} // namespace

Https::Https(std::string ak, std::string sk) : ak_(std::move(ak)), sk_(std::move(sk)) {
  assert(!ak_.empty() && !sk_.empty());
}

yyjson_doc *Https::get(std::string_view path) {
  std::string last_err;
  for (int attempt = 0;; ++attempt) {
    SignedHeaders h = sign_request(ak_, sk_, path, /*body*/ {});
    try {
      auto [status, body] = request_once(http::verb::get, path, {}, h);
      assert(status == 200 && "HTTP status != 200");
      return parse_envelope(path, body);
    } catch (const boost::system::system_error &e) {
      last_err = e.what();
      std::cerr << "[bigquant.https] transient (attempt " << (attempt + 1) << "/"
                << (::config::BIGQUANT_HTTPS_RETRY_MAX + 1) << ") GET " << path
                << ": " << last_err << std::endl;
      assert(attempt < ::config::BIGQUANT_HTTPS_RETRY_MAX &&
             "exhausted HTTP retries");
      std::this_thread::sleep_for(
          std::chrono::seconds(::config::BIGQUANT_HTTPS_RETRY_INTERVAL_SECONDS));
    }
  }
}

yyjson_doc *Https::post(std::string_view path, std::string_view body) {
  std::string last_err;
  for (int attempt = 0;; ++attempt) {
    SignedHeaders h = sign_request(ak_, sk_, path, body);
    try {
      auto [status, resp] = request_once(http::verb::post, path, body, h);
      assert(status == 200 && "HTTP status != 200");
      return parse_envelope(path, resp);
    } catch (const boost::system::system_error &e) {
      last_err = e.what();
      std::cerr << "[bigquant.https] transient (attempt " << (attempt + 1) << "/"
                << (::config::BIGQUANT_HTTPS_RETRY_MAX + 1) << ") POST " << path
                << ": " << last_err << std::endl;
      assert(attempt < ::config::BIGQUANT_HTTPS_RETRY_MAX &&
             "exhausted HTTP retries");
      std::this_thread::sleep_for(
          std::chrono::seconds(::config::BIGQUANT_HTTPS_RETRY_INTERVAL_SECONDS));
    }
  }
}

} // namespace bigquant
