#pragma once

#include "package/yyjson/yyjson.h"

#include <string>
#include <string_view>

namespace bigquant {

// BigQuant 控制面 HTTPS 客户端 (boost.beast::ssl + OpenSSL).
//   - 基址 https://bigquant.com:443
//   - 自动 HMAC 签名 (sign_request -> X-BigQuant-{Access-Key,Timestamp,Signature})
//   - 复用 tushare/http.cpp 的网络瞬抖 retry pattern, 业务错误一律 assert
//
// 返回 yyjson_doc*: 已剥掉 {code,reason,data} 信封, root 即响应中的 `data` 字段
//   (caller 用 yyjson_doc_free 释放).
//   服务端 code != 0 直接 assert (stderr 打印 reason / message).
class Https {
public:
  explicit Https(std::string ak, std::string sk);

  // GET <path> 不带 query (path 已含 /bigapis/.../...).
  // 签名 message = path || "" || ts_ms.
  yyjson_doc *get(std::string_view path);

  // POST <path> 携带 JSON body. content_type 默认 application/json.
  // 签名 message = path || body || ts_ms.
  yyjson_doc *post(std::string_view path, std::string_view body);

private:
  std::string ak_;
  std::string sk_;
};

} // namespace bigquant
