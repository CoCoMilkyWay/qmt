#include "api/bigquant/signer.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace bigquant {

std::string hmac_sha256_hex(std::string_view sk, std::string_view msg) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> raw{};
  unsigned int raw_len = 0;
  unsigned char *out = HMAC(EVP_sha256(),
                            sk.data(), static_cast<int>(sk.size()),
                            reinterpret_cast<const unsigned char *>(msg.data()),
                            msg.size(),
                            raw.data(), &raw_len);
  assert(out != nullptr && "OpenSSL HMAC failed");
  assert(raw_len == 32 && "SHA256 must produce 32 bytes");

  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex(64, '0');
  for (unsigned i = 0; i < 32; ++i) {
    hex[i * 2 + 0] = kHex[(raw[i] >> 4) & 0xF];
    hex[i * 2 + 1] = kHex[raw[i] & 0xF];
  }
  return hex;
}

std::string now_unix_ms() {
  using namespace std::chrono;
  auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  return std::to_string(static_cast<int64_t>(ms));
}

SignedHeaders sign_request(std::string_view ak, std::string_view sk,
                           std::string_view path, std::string_view body) {
  SignedHeaders h;
  h.access_key.assign(ak);
  h.timestamp = now_unix_ms();

  std::string msg;
  msg.reserve(path.size() + body.size() + h.timestamp.size());
  msg.append(path.data(), path.size());
  msg.append(body.data(), body.size());
  msg.append(h.timestamp);

  h.signature = hmac_sha256_hex(sk, msg);
  return h;
}

} // namespace bigquant
