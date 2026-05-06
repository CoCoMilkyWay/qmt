#include "tushare/http.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace tushare {

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

static constexpr const char *HOST = "api.tushare.pro";
static constexpr const char *PORT = "80";
static constexpr int TIMEOUT_SECONDS = 60;

Http::Http(std::string token) : token_(std::move(token)) {
  assert(!token_.empty());
}

yyjson_doc *
Http::call(std::string_view api_name,
           const std::vector<std::pair<std::string, std::string>> &params) {
  // ---- Build request body ----
  yyjson_mut_doc *body_doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(body_doc);
  yyjson_mut_doc_set_root(body_doc, root);

  std::string api_str(api_name);
  yyjson_mut_obj_add_strncpy(body_doc, root, "api_name", api_str.data(),
                             api_str.size());
  yyjson_mut_obj_add_strncpy(body_doc, root, "token", token_.data(),
                             token_.size());
  yyjson_mut_obj_add_strncpy(body_doc, root, "fields", "", 0);

  yyjson_mut_val *params_obj = yyjson_mut_obj(body_doc);
  yyjson_mut_obj_add_val(body_doc, root, "params", params_obj);
  for (auto &[k, v] : params) {
    yyjson_mut_obj_add_strncpy(body_doc, params_obj, k.c_str(), v.data(),
                               v.size());
  }

  size_t body_len = 0;
  char *body_str = yyjson_mut_write(body_doc, 0, &body_len);
  assert(body_str);
  std::string body(body_str, body_len);
  std::free(body_str);
  yyjson_mut_doc_free(body_doc);

  // ---- POST via boost.beast ----
  net::io_context ioc;
  tcp::resolver resolver(ioc);
  beast::tcp_stream stream(ioc);
  stream.expires_after(std::chrono::seconds(TIMEOUT_SECONDS));

  auto results = resolver.resolve(HOST, PORT);
  stream.connect(results);

  http::request<http::string_body> req{http::verb::post, "/", 11};
  req.set(http::field::host, HOST);
  req.set(http::field::user_agent, "qmt-tushare/1.0");
  req.set(http::field::content_type, "application/json");
  req.body() = body;
  req.prepare_payload();

  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);

  // ---- Parse response ----
  const std::string &res_body = res.body();
  yyjson_doc *doc = yyjson_read(res_body.data(), res_body.size(), 0);
  if (!doc) {
    std::cerr << "Tushare API: failed to parse response, body=\n"
              << res_body.substr(0, 1024) << std::endl;
    assert(false);
  }

  yyjson_val *root_val = yyjson_doc_get_root(doc);
  yyjson_val *code_val = yyjson_obj_get(root_val, "code");
  assert(yyjson_is_int(code_val) || yyjson_is_uint(code_val));
  int64_t code = yyjson_get_int(code_val);
  if (code != 0) {
    yyjson_val *msg_val = yyjson_obj_get(root_val, "msg");
    const char *msg =
        (msg_val && yyjson_is_str(msg_val)) ? yyjson_get_str(msg_val) : "";
    std::cerr << "Tushare API error: code=" << code << " msg=" << msg
              << " api=" << api_name << std::endl;
    assert(false);
  }

  return doc;
}

std::string load_token() {
  const char *home = std::getenv("HOME");
  assert(home);
  std::filesystem::path path = std::filesystem::path(home) / ".tushare_token";
  if (!std::filesystem::exists(path)) {
    std::cerr << "tushare token not found at " << path
              << "\n  hint: echo \"<your token>\" > " << path.string()
              << std::endl;
    assert(false);
  }
  std::ifstream f(path);
  std::string token;
  std::getline(f, token);
  while (!token.empty() &&
         std::isspace(static_cast<unsigned char>(token.back()))) {
    token.pop_back();
  }
  assert(!token.empty());
  return token;
}

} // namespace tushare
