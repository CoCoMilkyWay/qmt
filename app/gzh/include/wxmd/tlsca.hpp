#pragma once

#include <memory>
#include <string>

namespace wxmd {

// 一张现签的服务端证书：叶子证书 + CA 证书拼成的链，以及对应私钥，都是 PEM。
struct CertPair {
  std::string cert_pem;
  std::string key_pem;
};

// 自签 CA，按域名现签叶子证书。
// CA 必须落盘复用：每换一次 CA，客户端就得重装一次信任。
class CertAuthority {
public:
  // dir 下没有 CA 时自动生成一份。
  explicit CertAuthority(const std::string &dir);
  ~CertAuthority();

  CertAuthority(const CertAuthority &) = delete;
  CertAuthority &operator=(const CertAuthority &) = delete;

  // 要装进客户端信任库的就是这个文件。
  const std::string &ca_cert_path() const;

  // 现签一张给定域名的证书；同一域名只签一次，之后走缓存。
  const CertPair &issue(const std::string &host);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace wxmd
