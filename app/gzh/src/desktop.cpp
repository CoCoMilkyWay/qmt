#include "wxmd/desktop.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "wxmd/assert.hpp"

#include "strutil.hpp"

namespace wxmd {
namespace {

constexpr const char *kCaNickname = "wxmd local CA";

// gsettings 的字符串值带单引号，端口是裸数字。
std::string unquote(const std::string &text) {
  const std::string trimmed = str::trim(text);
  if (trimmed.size() >= 2 && trimmed.front() == '\'' &&
      trimmed.back() == '\'') {
    return trimmed.substr(1, trimmed.size() - 2);
  }
  return trimmed;
}

std::string run_capture(const std::string &command) {
  FILE *pipe = ::popen((command + " 2>/dev/null").c_str(), "r");
  WXMD_ASSERT(pipe != nullptr, "无法执行命令: " + command);

  std::string out;
  std::array<char, 512> buffer{};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    out += buffer.data();
  }
  ::pclose(pipe);
  return str::trim(out);
}

// 返回退出码；调用方决定失败要不要断言（删除不存在的证书就允许失败）。
int run_quiet(const std::string &command) {
  return ::system((command + " >/dev/null 2>&1").c_str());
}

std::string gsettings_get(const std::string &schema, const std::string &key) {
  return run_capture("gsettings get " + schema + " " + key);
}

void gsettings_set(const std::string &schema, const std::string &key,
                   const std::string &value) {
  const std::string command =
      "gsettings set " + schema + " " + key + " " + value;
  WXMD_ASSERT(run_quiet(command) == 0, "设置失败: " + command);
}

ProxyEndpoint read_endpoint(const std::string &kind) {
  const std::string schema = "org.gnome.system.proxy." + kind;
  ProxyEndpoint endpoint;
  endpoint.host = unquote(gsettings_get(schema, "host"));
  endpoint.port = static_cast<int>(
      std::strtol(gsettings_get(schema, "port").c_str(), nullptr, 10));
  return endpoint;
}

void write_endpoint(const std::string &kind, const ProxyEndpoint &endpoint) {
  const std::string schema = "org.gnome.system.proxy." + kind;
  gsettings_set(schema, "host", "\"" + endpoint.host + "\"");
  gsettings_set(schema, "port", std::to_string(endpoint.port));
}

std::string nssdb_dir() {
  const char *home = std::getenv("HOME");
  WXMD_ASSERT(home != nullptr, "读不到 HOME 环境变量");
  return std::string(home) + "/.pki/nssdb";
}

std::string nssdb_path() { return "sql:" + nssdb_dir(); }

// /etc/os-release 的 ID 与 ID_LIKE 合起来判发行版族，比只看 ID 宽容。
std::string distro_family() {
  const std::string text =
      run_capture("cat /etc/os-release") + " " + run_capture("uname -s");
  const std::string lowered = str::lowered(text);

  for (const char *name :
       {"debian", "ubuntu", "fedora", "rhel", "centos", "arch", "suse"}) {
    if (lowered.find(name) != std::string::npos) {
      return name;
    }
  }
  return {};
}

} // namespace

bool has_command(const std::string &name) {
  return run_quiet("command -v " + name) == 0;
}

bool process_running(const std::string &name) {
  return run_quiet("pgrep -x " + name) == 0;
}

std::string nss_tools_package() {
  const std::string family = distro_family();
  if (family == "debian" || family == "ubuntu") {
    return "libnss3-tools";
  }
  if (family == "fedora" || family == "rhel" || family == "centos") {
    return "nss-tools";
  }
  if (family == "arch") {
    return "nss";
  }
  if (family == "suse") {
    return "mozilla-nss-tools";
  }
  return {};
}

std::string install_command(const std::string &package) {
  if (package.empty()) {
    return {};
  }
  if (has_command("apt-get")) {
    return "sudo apt-get install -y " + package;
  }
  if (has_command("dnf")) {
    return "sudo dnf install -y " + package;
  }
  if (has_command("pacman")) {
    return "sudo pacman -S --noconfirm " + package;
  }
  if (has_command("zypper")) {
    return "sudo zypper install -y " + package;
  }
  return {};
}

SystemProxy read_system_proxy() {
  WXMD_ASSERT(run_quiet("command -v gsettings") == 0,
              "找不到 gsettings，无法接管系统代理");

  SystemProxy config;
  config.mode = unquote(gsettings_get("org.gnome.system.proxy", "mode"));
  config.http = read_endpoint("http");
  config.https = read_endpoint("https");
  config.socks = read_endpoint("socks");
  return config;
}

SystemProxy take_over_system_proxy(int port) {
  const SystemProxy saved = read_system_proxy();

  const ProxyEndpoint ours{"127.0.0.1", port};
  write_endpoint("http", ours);
  write_endpoint("https", ours);
  write_endpoint("socks", {"", 0});
  gsettings_set("org.gnome.system.proxy", "mode", "'manual'");

  return saved;
}

void restore_system_proxy(const SystemProxy &saved) {
  write_endpoint("http", saved.http);
  write_endpoint("https", saved.https);
  write_endpoint("socks", saved.socks);
  gsettings_set("org.gnome.system.proxy", "mode", "'" + saved.mode + "'");
}

void save_proxy_backup(const std::string &path, const SystemProxy &config) {
  const auto dump = [](const ProxyEndpoint &endpoint) {
    return nlohmann::json{{"host", endpoint.host}, {"port", endpoint.port}};
  };
  const nlohmann::json root{{"mode", config.mode},
                            {"http", dump(config.http)},
                            {"https", dump(config.https)},
                            {"socks", dump(config.socks)}};

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  WXMD_ASSERT(output.is_open(), "无法写入代理备份: " + path);
  output << root.dump(2);
}

bool restore_proxy_backup(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  input.close();

  const nlohmann::json root =
      nlohmann::json::parse(buffer.str(), nullptr, false);
  WXMD_ASSERT(!root.is_discarded(), "代理备份文件损坏，请手工删除: " + path);

  const auto load = [&root](const char *key) {
    ProxyEndpoint endpoint;
    endpoint.host = root[key].value("host", std::string());
    endpoint.port = root[key].value("port", 0);
    return endpoint;
  };

  SystemProxy saved;
  saved.mode = root.value("mode", std::string("none"));
  saved.http = load("http");
  saved.https = load("https");
  saved.socks = load("socks");

  restore_system_proxy(saved);
  clear_proxy_backup(path);
  return true;
}

void clear_proxy_backup(const std::string &path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void install_ca_to_nssdb(const std::string &ca_cert_path) {
  WXMD_ASSERT(has_command("certutil"), "找不到 certutil");

  const std::string db = nssdb_path();

  // 全新用户没有这个库，certutil 不会替我们建目录，也不会凭空建库。
  WXMD_ASSERT(run_quiet("mkdir -p '" + nssdb_dir() + "'") == 0,
              "无法创建 NSS 目录: " + nssdb_dir());
  if (run_quiet("certutil -d " + db + " -L") != 0) {
    WXMD_ASSERT(run_quiet("certutil -d " + db + " -N --empty-password") == 0,
                "初始化 NSS 库失败: " + nssdb_dir());
  }

  // 先删后加：CA 换了之后不能留着旧的，否则信任的是一张已经用不上的证书。
  run_quiet("certutil -d " + db + " -D -n '" + kCaNickname + "'");

  const std::string add = "certutil -d " + db + " -A -t 'C,,' -n '" +
                          kCaNickname + "' -i '" + ca_cert_path + "'";
  WXMD_ASSERT(run_quiet(add) == 0, "把 CA 装进 NSS 库失败: " + add);

  WXMD_ASSERT(run_quiet("certutil -d " + db + " -L -n '" + kCaNickname + "'") ==
                  0,
              "CA 装完之后在 NSS 库里查不到，信任不会生效");
}

void uninstall_ca_from_nssdb() {
  if (!has_command("certutil")) {
    return; // 没有 certutil 说明从未装过，无需清理。
  }
  // 删不存在的条目 certutil 会返回非 0；这里本就是清理，忽略结果即可。
  run_quiet("certutil -d " + nssdb_path() + " -D -n '" + kCaNickname + "'");
}

} // namespace wxmd
