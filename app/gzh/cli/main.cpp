#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <poll.h>

#include <csignal>

#include "wxmd/assert.hpp"
#include "wxmd/capture.hpp"
#include "wxmd/desktop.hpp"
#include "wxmd/proxy.hpp"
#include "wxmd/wxmd.hpp"

namespace {

// ------------------------------------------------------- 编译期参数（调参用）
// 这些没有做成命令行开关：日常用不到，改了要重编译，正好逼着想清楚再改。
// kProxyPort 传 0：监听端口交给内核分配，避开占用与多实例并发冲突；
// 实际端口在 start() 后用 proxy.port() 取回，再写进系统代理设置。
constexpr int kProxyPort = 0;
constexpr const char *kTargetHost = "mp.weixin.qq.com";

constexpr int kPageSize = 20;     // 每页条数，太大容易触发风控
constexpr int kIntervalMs = 1000; // 两次请求间隔
constexpr int kArticleLimit = 0;  // 最多取多少篇，0 为不限
constexpr int kRefreshMs = 1000;  // 捕获列表刷新间隔

void print_usage() {
  std::cout
      << "用法:\n"
         "  wxmd                       启动代理，抓凭证后选公众号拉文章列表\n"
         "  wxmd <文章链接>            抓取单篇并输出 Markdown\n"
         "  wxmd -f <本地 html>        解析本地 HTML（离线回归）\n"
         "\n"
         "可选:\n"
         "  -o <输出文件>              写入文件，默认写到标准输出\n"
         "  --meta                     附加元信息\n"
         "  --html                     输出中间态 HTML，不转 Markdown\n"
         "\n"
         "维护:\n"
         "  --uninstall                还原系统代理并移除本工具的 CA 与 "
         "~/.wxmd\n";
}

std::string read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  WXMD_ASSERT(input.is_open(), "无法打开文件: " + path);

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  WXMD_ASSERT(output.is_open(), "无法写入文件: " + path);
  output << content;
}

// 输出路径为空时走标准输出，否则落盘并在 stderr 报一行。
void emit(const std::string &path, const std::string &content) {
  if (path.empty()) {
    std::cout << content;
    return;
  }
  write_file(path, content);
  std::cerr << "已写入: " << path << " (" << content.size() << " 字节)\n";
}

std::string trim(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string format_time(int64_t unix_seconds) {
  const std::time_t raw = static_cast<std::time_t>(unix_seconds);
  std::tm parts{};
  localtime_r(&raw, &parts);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &parts);
  return buffer;
}

// ------------------------------------------------------------------ 前置检查
// 目标：换一台机器、换一个用户，只跟这个终端交互就能把流程走顺。
// 每一步都先说清楚要做什么、为什么，再等确认。

bool confirm(const std::string &question, bool default_yes) {
  std::cout << question << (default_yes ? " [Y/n] " : " [y/N] ") << std::flush;

  std::string line;
  if (!std::getline(std::cin, line)) {
    return false;
  }
  line = trim(line);
  if (line.empty()) {
    return default_yes;
  }
  return line[0] == 'y' || line[0] == 'Y';
}

// certutil 用来把自签 CA 写进用户证书库，缺了就没法让微信信任我们。
bool ensure_certutil() {
  if (wxmd::has_command("certutil")) {
    return true;
  }

  const std::string package = wxmd::nss_tools_package();
  const std::string command = wxmd::install_command(package);

  std::cout << "\n缺少 certutil。它用来把本工具的自签证书写进你的用户证书库\n"
               "（~/.pki/nssdb），微信的文章页 webview 从那里取信任。\n"
               "只写当前用户，不动系统证书。\n\n";

  if (command.empty()) {
    std::cout << "认不出这台机器的包管理器，请自行安装 certutil"
              << (package.empty() ? "" : "（包名 " + package + "）")
              << " 后重跑。\n";
    return false;
  }

  std::cout << "需要执行（会向你要 sudo 密码）:\n  " << command << "\n\n";
  if (!confirm("现在安装吗?", true)) {
    std::cout << "已跳过。装好后重跑即可。\n";
    return false;
  }

  std::cout << "\n";
  int rc = std::system(command.c_str());
  assert(rc == 0);

  if (!wxmd::has_command("certutil")) {
    std::cout << "\n安装后仍找不到 certutil，请手工确认后重跑。\n";
    return false;
  }
  std::cout << "\ncertutil 就绪。\n";
  return true;
}

// ---------------------------------------------------------------- 交互模式
void print_setup_guide(const wxmd::MitmProxy &proxy) {
  std::cout << "\n就绪。在微信里打开目标公众号的任意一篇文章即可。\n";
  if (wxmd::process_running("wechat")) {
    std::cout << "（抓不到就重启一次微信：它只在启动时读代理和证书）\n";
  }
  std::cout << "回车选最新捕获，输入序号选其它，q 退出。\n"
            << "----\n";
  (void)proxy;
}

// 接管系统代理后，任何退出路径都必须把它还回去，否则整台机器的流量就断在
// 我们的死端口上。restore_now 是唯一的还原入口：正常收尾、信号、断言 abort
// 都汇到这里；靠 g_restore_armed 保证幂等，重复调用是安全的。
wxmd::SystemProxy g_saved_proxy;
std::string g_backup_path;
volatile std::sig_atomic_t g_restore_armed = 0;

void restore_now() {
  if (!g_restore_armed) {
    return;
  }
  g_restore_armed = 0; // 先清标志：防止信号重入与正常收尾的重复还原。
  wxmd::restore_system_proxy(g_saved_proxy);
  wxmd::clear_proxy_backup(g_backup_path);
}

// 断言失败走的是 abort()→SIGABRT，段错误是 SIGSEGV，关终端是 SIGHUP——
// 本项目「尽早失败」下断言遍地，光挂 SIGINT/SIGTERM 兜不住。这里把这些致命
// 信号全接住：先喊清楚是哪个信号（别再靠猜），还原环境，再恢复默认处理并重抛，
// 保留正确的退出码与
// core。（即便还原因状态损坏没跑成，下次启动的无条件自愈也兜底。）
extern "C" void on_signal(int sig) {
  // 信号处理器里只能用 async-signal-safe 调用，故用裸 write 直接喊。
  const char *note = nullptr;
  switch (sig) {
  case SIGINT:
    note = "\n[wxmd] 收到 SIGINT（Ctrl-C），还原系统代理后退出。\n";
    break;
  case SIGTERM:
    note = "\n[wxmd] 收到 SIGTERM，还原系统代理后退出。\n";
    break;
  case SIGHUP:
    note = "\n[wxmd] 收到 SIGHUP（终端关闭），还原系统代理后退出。\n";
    break;
  case SIGQUIT:
    note = "\n[wxmd] 收到 SIGQUIT，还原系统代理后退出。\n";
    break;
  case SIGABRT:
    note = "\n[wxmd] 收到 SIGABRT（断言失败/abort），还原系统代理后退出。\n";
    break;
  case SIGSEGV:
    note = "\n[wxmd] 收到 SIGSEGV（段错误！这是 bug），还原系统代理后退出。\n";
    break;
  default:
    note = "\n[wxmd] 收到致命信号，还原系统代理后退出。\n";
    break;
  }
  const ssize_t ignored = ::write(STDERR_FILENO, note, std::strlen(note));
  (void)ignored;

  restore_now();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

void arm_restore_signals() {
  for (int sig : {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGABRT, SIGSEGV}) {
    std::signal(sig, on_signal);
  }
}

void print_accounts(const std::vector<wxmd::CapturedAccount> &accounts) {
  if (accounts.empty()) {
    std::cout << "[等待捕获] 还没抓到凭证\r" << std::flush;
    return;
  }

  const int64_t now = wxmd::now_ms();
  std::cout << "\n已捕获 " << accounts.size() << " 个公众号:\n";
  for (size_t i = 0; i < accounts.size(); ++i) {
    const wxmd::CapturedAccount &item = accounts[i];
    const int left = item.remaining_seconds(now);
    std::cout << "  [" << (i + 1) << "] "
              << (item.nickname.empty() ? item.cred.biz : item.nickname)
              << "  剩余 " << (left / 60) << "分" << (left % 60) << "秒"
              << (left == 0 ? "  (已过期，重新打开一篇文章)" : "") << "\n";
  }
  std::cout << "> " << std::flush;
}

// 名字要联网解析，放在主循环里做，别拖住代理线程。
void resolve_pending_names(wxmd::CredentialStore &store,
                           const std::vector<wxmd::CapturedAccount> &accounts) {
  for (const wxmd::CapturedAccount &item : accounts) {
    if (!item.nickname.empty()) {
      continue;
    }
    // 名字写在文章页的 var nickname 里；历史消息页没有这个字段，跳过。
    if (item.url.find("/s?") == std::string::npos &&
        item.url.find("/s/") == std::string::npos) {
      wxmd::warn("公众号 " + item.cred.biz + " 的凭证来自非文章页（" +
                 item.url +
                 "），页内没有名字，列表只能先显示 __biz；"
                 "在微信里打开该号任意一篇文章即可解析出名字。");
      continue;
    }
    const std::string name = wxmd::fetch_account_name(item.url);
    if (name.empty()) {
      wxmd::warn("从文章页解析公众号名字失败（" + item.url +
                 "），列表暂显示 __biz " + item.cred.biz);
    }
    store.set_nickname(item.cred.biz, name);
  }
}

// 阻塞等一行输入；超时返回 false，好让列表继续刷新。
bool read_line_timeout(std::string &line, int timeout_ms) {
  pollfd watch{STDIN_FILENO, POLLIN, 0};
  const int ready = ::poll(&watch, 1, timeout_ms);
  if (ready < 0) {
    wxmd::warn("poll(stdin) 出错，稍后重试");
    return false;
  }
  if (ready == 0) {
    return false;
  }
  if (!std::getline(std::cin, line)) {
    // 标准输入被关闭/EOF：当成主动退出，但要说清楚，别让人以为是自己没输入。
    wxmd::warn("标准输入已结束（EOF），按退出处理");
    line = "q";
  }
  return true;
}

// 返回选中的凭证；用户主动退出则返回 false。
bool select_account(wxmd::CredentialStore &store, wxmd::CapturedAccount &out) {
  size_t known = 0;

  for (;;) {
    std::vector<wxmd::CapturedAccount> accounts = store.snapshot();
    if (accounts.size() != known) {
      resolve_pending_names(store, accounts);
      accounts = store.snapshot();
      known = accounts.size();
      print_accounts(accounts);
    }

    std::string line;
    if (!read_line_timeout(line, kRefreshMs)) {
      continue;
    }

    line = trim(line);
    if (line == "q") {
      return false;
    }
    if (accounts.empty()) {
      std::cout << "还没抓到凭证，先在微信里打开一篇文章\n> " << std::flush;
      continue;
    }

    const size_t choice =
        line.empty()
            ? accounts.size()
            : static_cast<size_t>(std::strtoul(line.c_str(), nullptr, 10));
    if (choice < 1 || choice > accounts.size()) {
      std::cout << "序号超出范围\n> " << std::flush;
      continue;
    }

    out = accounts[choice - 1];
    WXMD_ASSERT(out.valid_at(wxmd::now_ms()),
                "该凭证已过期，请在微信里重新打开一篇文章");
    return true;
  }
}

int run_interactive(const std::string &output_path, bool with_meta) {
  const char *home = std::getenv("HOME");
  WXMD_ASSERT(home != nullptr, "读不到 HOME 环境变量，无法决定 CA 存放位置");

  WXMD_ASSERT(wxmd::has_command("gsettings"),
              "找不到 gsettings，本工具靠它临时接管系统代理；"
              "非 GNOME 桌面暂不支持");
  if (!ensure_certutil()) {
    return 1;
  }

  const std::string base = std::string(home) + "/.wxmd";
  const std::string backup = base + "/proxy-backup.json";

  // 上次异常退出留下的代理残留，已在 main()
  // 入口无条件自愈过，这里直接读当前值。
  // 原有的系统代理成为我们的上游，用户其它流量的走向保持不变。
  const wxmd::SystemProxy saved = wxmd::read_system_proxy();
  const bool chain = saved.mode == "manual" && !saved.https.host.empty();

  wxmd::CredentialStore store;
  wxmd::MitmProxy proxy(kProxyPort, {kTargetHost}, base,
                        chain ? saved.https.host : std::string(),
                        chain ? saved.https.port : 0);

  proxy.set_handler([&store](const wxmd::Exchange &exchange) {
    wxmd::CapturedAccount account;
    if (wxmd::parse_exchange(exchange, account)) {
      store.offer(account);
    }
  });
  proxy.start();

  wxmd::install_ca_to_nssdb(proxy.ca_cert_path());

  wxmd::save_proxy_backup(backup, saved);
  g_backup_path = backup;
  g_saved_proxy = saved;
  g_restore_armed = 1;
  std::atexit(restore_now); // 任何 return/exit 路径的兜底还原。
  arm_restore_signals();
  wxmd::take_over_system_proxy(proxy.port());

  print_setup_guide(proxy);

  wxmd::CapturedAccount chosen;
  const bool picked = select_account(store, chosen);

  if (picked) {
    std::cerr << "\n已选择，正在断开代理…\n";
  }

  // 抓完就把系统代理还回去：后面拉列表是我们自己直连，不再需要接管。
  restore_now();
  proxy.stop();

  if (!picked) {
    return 0;
  }

  const std::string name =
      chosen.nickname.empty() ? chosen.cred.biz : chosen.nickname;
  std::cout << "\n开始拉取: " << name << "\n";

  std::string out;
  size_t total = 0;
  const auto on_page = [&](const wxmd::ProfilePage &page, int offset) {
    for (const wxmd::ProfileEntry &entry : page.entries) {
      if (with_meta) {
        out += format_time(entry.datetime) + "\t" + entry.title + "\t";
      }
      out += entry.link + "\n";
    }
    total += page.entries.size();
    std::cerr << "\r已抓取 " << total << " 篇（offset " << offset << "）      "
              << std::flush;
    // 每页落盘：key 可能中途过期触发断言，已拿到的部分不至于丢。
    if (!output_path.empty() && !out.empty()) {
      write_file(output_path, out);
    }
  };

  const wxmd::ProfileList list = wxmd::fetch_profile_list(
      chosen.cred, 0, kPageSize, kArticleLimit, kIntervalMs, on_page);
  std::cerr << "\n"; // 收尾：让刷新行定格，后续输出另起一行。

  // 屏幕上给人看的是发布时间 + 文章标题；链接是给后台用的，只写文件、不刷屏。
  for (size_t i = 0; i < list.entries.size(); ++i) {
    const wxmd::ProfileEntry &entry = list.entries[i];
    const std::string when =
        entry.datetime > 0 ? format_time(entry.datetime) : "??";
    std::cout << "  " << (i + 1) << ". [" << when << "] "
              << (entry.title.empty() ? "(无标题)" : entry.title) << "\n";
  }
  std::cerr << "共 " << list.entries.size() << " 篇";
  if (output_path.empty()) {
    std::cerr << "（链接未保存，需要就加 -o 文件名）\n";
  } else {
    std::cerr << "，链接已写入 " << output_path << "\n";
  }
  return 0;
}

// 一键卸载：还原可能残留的系统代理，摘掉 NSS 里的 CA 信任，删掉 ~/.wxmd。
// 走完之后机器回到用本工具之前的干净状态。
int run_uninstall() {
  const char *home = std::getenv("HOME");
  WXMD_ASSERT(home != nullptr, "读不到 HOME 环境变量");

  const std::string base = std::string(home) + "/.wxmd";
  const std::string backup = base + "/proxy-backup.json";

  if (wxmd::restore_proxy_backup(backup)) {
    std::cout << "已还原残留的系统代理设置。\n";
  }
  wxmd::uninstall_ca_from_nssdb();
  std::cout << "已从证书库移除本工具的 CA 信任。\n";

  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::cout << "已删除 " << base << "（CA 证书 / 私钥 / 备份）。清理完成。\n";
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  // 忽略 SIGPIPE：往对端已关闭的 socket
  // 写会触发它，默认动作是静默杀掉整个进程。 我们的明文转发用了
  // MSG_NOSIGNAL，但 OpenSSL 的 SSL_write 底层 write 不带它，
  // 微信随时会掐断连接、stop() 也会主动
  // shutdown，必须全局忽略，让写操作只是返回
  // 错误由各自的循环处理，而不是把进程带走。
  std::signal(SIGPIPE, SIG_IGN);

  // 无论这次跑哪种模式：上次若被强杀/崩溃，系统代理可能还指着我们的死端口。
  // 先无条件自愈——没有残留备份时这步只是打开一个不存在的文件后立即返回，
  // 代价可忽略，却保证了「不管上次怎么挂的，这次一开机就是干净环境」。
  if (const char *home = std::getenv("HOME")) {
    const std::string backup = std::string(home) + "/.wxmd/proxy-backup.json";
    if (wxmd::restore_proxy_backup(backup)) {
      std::cerr << "检测到上次异常退出，已自动还原系统代理。\n";
    }
  }

  std::string source;
  bool from_file = false;
  bool with_meta = false;
  bool html_only = false;
  std::string output_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "--uninstall") {
      return run_uninstall();
    }
    if (arg == "-f") {
      WXMD_ASSERT(i + 1 < argc, "-f 后缺少文件路径");
      from_file = true;
      source = argv[++i];
      continue;
    }
    if (arg == "-o") {
      WXMD_ASSERT(i + 1 < argc, "-o 后缺少输出路径");
      output_path = argv[++i];
      continue;
    }
    if (arg == "--meta") {
      with_meta = true;
      continue;
    }
    if (arg == "--html") {
      html_only = true;
      continue;
    }
    source = arg;
  }

  if (source.empty()) {
    return run_interactive(output_path, with_meta);
  }

  const std::string raw_html =
      from_file ? read_file(source) : wxmd::fetch_article(source);

  if (html_only) {
    emit(output_path, wxmd::render_article_html(raw_html));
    return 0;
  }

  const wxmd::Article article = wxmd::parse_article(raw_html);

  std::string out;
  if (with_meta) {
    out += "# " + article.title + "\n\n";
    out += "- 公众号: " + article.account + "\n";
    out += "- 作者: " + article.author + "\n";
    out += "- 发布时间: " + article.publish_time + "\n";
    out += "- 原文: " + article.link + "\n\n---\n\n";
  }
  out += article.markdown;
  out += "\n";

  emit(output_path, out);
  return 0;
}
