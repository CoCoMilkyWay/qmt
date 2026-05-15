#pragma once

#include <string>
#include <string_view>

namespace bigquant {

// BigQuant 控制面 HMAC 签名公式 (已与 SDK 二进制 build_auth_headers 对账):
//   sig = HMAC_SHA256(sk, path || body || timestamp) 转 lowercase hex
//   headers:
//     X-BigQuant-Access-Key: <ak>
//     X-BigQuant-Timestamp:  <ts_ms>      // unix epoch in milliseconds, decimal string
//     X-BigQuant-Signature:  <sig>
//
// 仅返回 64 字符 hex; 上层负责拼 header.
std::string hmac_sha256_hex(std::string_view sk, std::string_view msg);

// 当前 unix 毫秒时间戳的十进制字符串 (server 强制 ms 精度, s 精度会被拒).
std::string now_unix_ms();

// 一次性产出三元组 (ak, ts_ms, sig); body 允许空.
struct SignedHeaders {
  std::string access_key; // X-BigQuant-Access-Key
  std::string timestamp;  // X-BigQuant-Timestamp (ms)
  std::string signature;  // X-BigQuant-Signature (hex)
};

SignedHeaders sign_request(std::string_view ak, std::string_view sk,
                           std::string_view path, std::string_view body = {});

} // namespace bigquant
