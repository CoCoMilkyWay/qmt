#include "wxmd/tlsca.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "wxmd/assert.hpp"

namespace wxmd {
namespace {

constexpr int kKeyBits = 2048;
constexpr long kCaValiditySeconds = 3650L * 24 * 3600;
// 不少客户端会拒绝有效期超过 398 天的服务端证书。
constexpr long kLeafValiditySeconds = 397L * 24 * 3600;

struct PkeyDeleter {
  void operator()(EVP_PKEY *key) const { EVP_PKEY_free(key); }
};
struct X509Deleter {
  void operator()(X509 *cert) const { X509_free(cert); }
};
struct BioDeleter {
  void operator()(BIO *bio) const { BIO_free(bio); }
};

using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;
using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

std::string openssl_error() {
  const unsigned long code = ERR_get_error();
  if (code == 0) {
    return "(无 OpenSSL 错误信息)";
  }
  char buffer[256];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  return buffer;
}

PkeyPtr generate_key() {
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  WXMD_ASSERT(ctx != nullptr, "创建密钥生成上下文失败: " + openssl_error());
  WXMD_ASSERT(EVP_PKEY_keygen_init(ctx) == 1,
              "密钥生成初始化失败: " + openssl_error());
  WXMD_ASSERT(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, kKeyBits) == 1,
              "设置密钥长度失败: " + openssl_error());

  EVP_PKEY *raw = nullptr;
  WXMD_ASSERT(EVP_PKEY_keygen(ctx, &raw) == 1,
              "生成密钥失败: " + openssl_error());
  EVP_PKEY_CTX_free(ctx);
  return PkeyPtr(raw);
}

std::string bio_to_string(BIO *bio) {
  char *data = nullptr;
  const long length = BIO_get_mem_data(bio, &data);
  WXMD_ASSERT(length > 0, "PEM 序列化结果为空");
  return std::string(data, static_cast<size_t>(length));
}

std::string cert_to_pem(X509 *cert) {
  BioPtr bio(BIO_new(BIO_s_mem()));
  WXMD_ASSERT(PEM_write_bio_X509(bio.get(), cert) == 1,
              "写出证书 PEM 失败: " + openssl_error());
  return bio_to_string(bio.get());
}

std::string key_to_pem(EVP_PKEY *key) {
  BioPtr bio(BIO_new(BIO_s_mem()));
  WXMD_ASSERT(PEM_write_bio_PrivateKey(bio.get(), key, nullptr, nullptr, 0,
                                       nullptr, nullptr) == 1,
              "写出私钥 PEM 失败: " + openssl_error());
  return bio_to_string(bio.get());
}

X509Ptr cert_from_pem(const std::string &pem) {
  BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  X509 *cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
  WXMD_ASSERT(cert != nullptr, "解析 CA 证书失败: " + openssl_error());
  return X509Ptr(cert);
}

PkeyPtr key_from_pem(const std::string &pem) {
  BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  EVP_PKEY *key = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
  WXMD_ASSERT(key != nullptr, "解析 CA 私钥失败: " + openssl_error());
  return PkeyPtr(key);
}

void add_ext(X509 *target, X509 *issuer, int nid, const char *value) {
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer, target, nullptr, nullptr, 0);

  X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
  WXMD_ASSERT(ext != nullptr, std::string("构造证书扩展失败: ") + value + " " +
                                  openssl_error());
  X509_add_ext(target, ext, -1);
  X509_EXTENSION_free(ext);
}

void set_random_serial(X509 *cert) {
  unsigned char bytes[16];
  WXMD_ASSERT(RAND_bytes(bytes, sizeof(bytes)) == 1,
              "生成证书序列号失败: " + openssl_error());
  bytes[0] &= 0x7F; // 序列号必须是正整数

  BIGNUM *number = BN_bin2bn(bytes, sizeof(bytes), nullptr);
  WXMD_ASSERT(number != nullptr, "序列号转换失败");
  BN_to_ASN1_INTEGER(number, X509_get_serialNumber(cert));
  BN_free(number);
}

void set_name(X509_NAME *name, const char *field, const std::string &value) {
  WXMD_ASSERT(X509_NAME_add_entry_by_txt(
                  name, field, MBSTRING_ASC,
                  reinterpret_cast<const unsigned char *>(value.c_str()), -1,
                  -1, 0) == 1,
              std::string("设置证书主体字段失败: ") + field);
}

std::string read_text(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  WXMD_ASSERT(input.is_open(), "无法读取: " + path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void write_text(const std::string &path, const std::string &content,
                bool secret) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  WXMD_ASSERT(output.is_open(), "无法写入: " + path);
  output << content;
  output.close();

  namespace fs = std::filesystem;
  fs::permissions(path,
                  secret ? fs::perms::owner_read | fs::perms::owner_write
                         : fs::perms::owner_read | fs::perms::owner_write |
                               fs::perms::group_read | fs::perms::others_read,
                  fs::perm_options::replace);
}

} // namespace

struct CertAuthority::Impl {
  std::string ca_cert_path;
  std::string ca_key_path;
  std::string ca_cert_pem;
  X509Ptr ca_cert;
  PkeyPtr ca_key;
  // 所有叶子共用一把私钥，省掉每次握手前的 2048 位密钥生成。
  PkeyPtr leaf_key;

  std::mutex mutex;
  std::map<std::string, CertPair> cache;
};

CertAuthority::CertAuthority(const std::string &dir)
    : impl_(std::make_unique<Impl>()) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(dir, ec);
  WXMD_ASSERT(fs::is_directory(dir), "无法创建证书目录: " + dir);

  impl_->ca_cert_path = (fs::path(dir) / "wxmd-ca-cert.pem").string();
  impl_->ca_key_path = (fs::path(dir) / "wxmd-ca-key.pem").string();

  const bool has_ca =
      fs::exists(impl_->ca_cert_path) && fs::exists(impl_->ca_key_path);

  if (has_ca) {
    impl_->ca_cert_pem = read_text(impl_->ca_cert_path);
    impl_->ca_cert = cert_from_pem(impl_->ca_cert_pem);
    impl_->ca_key = key_from_pem(read_text(impl_->ca_key_path));
  } else {
    impl_->ca_key = generate_key();

    X509Ptr cert(X509_new());
    WXMD_ASSERT(cert != nullptr, "创建 CA 证书对象失败");
    X509_set_version(cert.get(), 2); // v3
    set_random_serial(cert.get());
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), kCaValiditySeconds);
    X509_set_pubkey(cert.get(), impl_->ca_key.get());

    X509_NAME *subject = X509_get_subject_name(cert.get());
    set_name(subject, "CN", "wxmd local CA");
    set_name(subject, "O", "wxmd");
    X509_set_issuer_name(cert.get(), subject); // 自签

    add_ext(cert.get(), cert.get(), NID_basic_constraints, "critical,CA:TRUE");
    add_ext(cert.get(), cert.get(), NID_key_usage,
            "critical,keyCertSign,cRLSign");
    add_ext(cert.get(), cert.get(), NID_subject_key_identifier, "hash");

    WXMD_ASSERT(X509_sign(cert.get(), impl_->ca_key.get(), EVP_sha256()) > 0,
                "CA 证书自签名失败: " + openssl_error());

    impl_->ca_cert = std::move(cert);
    impl_->ca_cert_pem = cert_to_pem(impl_->ca_cert.get());

    write_text(impl_->ca_cert_path, impl_->ca_cert_pem, false);
    write_text(impl_->ca_key_path, key_to_pem(impl_->ca_key.get()), true);
  }

  impl_->leaf_key = generate_key();
}

CertAuthority::~CertAuthority() = default;

const std::string &CertAuthority::ca_cert_path() const {
  return impl_->ca_cert_path;
}

const CertPair &CertAuthority::issue(const std::string &host) {
  const std::lock_guard<std::mutex> guard(impl_->mutex);

  const auto cached = impl_->cache.find(host);
  if (cached != impl_->cache.end()) {
    return cached->second;
  }

  X509Ptr leaf(X509_new());
  WXMD_ASSERT(leaf != nullptr, "创建叶子证书对象失败");
  X509_set_version(leaf.get(), 2);
  set_random_serial(leaf.get());
  X509_gmtime_adj(X509_getm_notBefore(leaf.get()), -3600);
  X509_gmtime_adj(X509_getm_notAfter(leaf.get()), kLeafValiditySeconds);
  X509_set_pubkey(leaf.get(), impl_->leaf_key.get());

  set_name(X509_get_subject_name(leaf.get()), "CN", host);
  X509_set_issuer_name(leaf.get(), X509_get_subject_name(impl_->ca_cert.get()));

  // 现代客户端只认 subjectAltName，CN 仅作展示。
  const std::string san = "DNS:" + host;
  add_ext(leaf.get(), impl_->ca_cert.get(), NID_subject_alt_name, san.c_str());
  add_ext(leaf.get(), impl_->ca_cert.get(), NID_basic_constraints,
          "critical,CA:FALSE");
  add_ext(leaf.get(), impl_->ca_cert.get(), NID_key_usage,
          "critical,digitalSignature,keyEncipherment");
  add_ext(leaf.get(), impl_->ca_cert.get(), NID_ext_key_usage, "serverAuth");
  add_ext(leaf.get(), impl_->ca_cert.get(), NID_authority_key_identifier,
          "keyid:always");

  WXMD_ASSERT(X509_sign(leaf.get(), impl_->ca_key.get(), EVP_sha256()) > 0,
              "叶子证书签名失败: " + openssl_error());

  CertPair pair;
  pair.cert_pem = cert_to_pem(leaf.get()) + impl_->ca_cert_pem;
  pair.key_pem = key_to_pem(impl_->leaf_key.get());

  return impl_->cache.emplace(host, std::move(pair)).first->second;
}

} // namespace wxmd
