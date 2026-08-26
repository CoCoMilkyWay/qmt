#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <poll.h>
#include <unistd.h>

#include "wxmd/assert.hpp"
#include "wxmd/capture.hpp"
#include "wxmd/desktop.hpp"
#include "wxmd/egress.hpp"
#include "wxmd/profile.hpp"
#include "wxmd/proxy.hpp"
#include "wxmd/store.hpp"
#include "wxmd/sync.hpp"

#include "config.hpp"
#include "strutil.hpp"

namespace {

// kProxyPort 传 0：监听端口交给内核分配，避开占用与多实例并发冲突；
// 实际端口在 start() 后用 proxy.port() 取回，再写进系统代理设置。
constexpr int kProxyPort = 0;
constexpr int kRefreshMs = 1000; // 捕获列表的刷新间隔

// 只有一条主路：增量同步。以前那些单篇 / 离线解析 / 只出 HTML 的独立入口都被
// 这条路覆盖了，留着只是多几种自己会腐烂的用法。
void print_usage() {
  std::cout
      << "用法:\n"
         "  wxmd                       增量同步 ./store：先补齐已有号的正文，\n"
         "                             再问要不要起代理加新号 / 刷新凭证\n"
         "  wxmd --uninstall           还原系统代理并移除本工具的 CA 与 "
         "~/.wxmd\n";
}

std::string wxmd_dir() {
  const char *home = std::getenv("HOME");
  WXMD_ASSERT(home != nullptr, "读不到 HOME 环境变量");
  return std::string(home) + "/.wxmd";
}

// ------------------------------------------------------------------ 前置检查
// 目标：换一台机器、换一个用户，只跟这个终端交互就能把流程走顺。

bool confirm(const std::string &question, bool default_yes) {
  std::cout << question << (default_yes ? " [Y/n] " : " [y/N] ") << std::flush;

  std::string line;
  if (!std::getline(std::cin, line)) {
    return false;
  }
  line = wxmd::str::trim(line);
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
  const int rc = std::system(command.c_str());
  WXMD_ASSERT(rc == 0, "安装命令失败: " + command);

  if (!wxmd::has_command("certutil")) {
    std::cout << "\n安装后仍找不到 certutil，请手工确认后重跑。\n";
    return false;
  }
  std::cout << "\ncertutil 就绪。\n";
  return true;
}

// ------------------------------------------------------------ 系统代理的还原
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

// 断言失败走 abort()→SIGABRT，段错误是 SIGSEGV，关终端是 SIGHUP——本项目
// 「尽早失败」下断言遍地，光挂 SIGINT/SIGTERM 兜不住。这里把致命信号全接住：
// 喊清是哪个信号、还原环境，再恢复默认处理并重抛，保留正确退出码与 core。
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

// -------------------------------------------------------------- 抓包收凭证
// 一趟收多个号是刻意的：凭证按号绑定，但 25 分钟的窗口是共享的——在微信里连
// 点几下，之后所有号的增量都在同一个窗口里做完，不必反复接管系统代理。

// 这一趟新抓到的号。按捕获时刻筛：表里那些上次留下的、还没过期的凭证不算，
// 它们在这次启动时就已经同步过一轮了。
std::vector<wxmd::Account> captured_since(const wxmd::Credentials &creds,
                                          int64_t since_ms) {
  std::vector<wxmd::Account> out;
  for (wxmd::Account &account : creds.snapshot()) {
    if (account.captured_ms >= since_ms) {
      out.push_back(std::move(account));
    }
  }
  return out;
}

void print_accounts(const std::vector<wxmd::Account> &accounts) {
  if (accounts.empty()) {
    std::cout << "[等待捕获] 还没抓到凭证\r" << std::flush;
    return;
  }

  std::cout << "\n已捕获 " << accounts.size() << " 个公众号:\n";
  for (size_t i = 0; i < accounts.size(); ++i) {
    const int left = accounts[i].credential_seconds();
    std::cout << "  [" << (i + 1) << "] " << accounts[i].label() << "  剩余 "
              << (left / 60) << "分" << (left % 60) << "秒"
              << (left == 0 ? "  (已过期，重新打开一篇文章)" : "") << "\n";
  }
  std::cout << "> " << std::flush;
}

// 名字要联网解析，放在主循环里做，别拖住代理线程。
void resolve_names(wxmd::Credentials &creds,
                   const std::vector<wxmd::Account> &accounts) {
  for (const wxmd::Account &account : accounts) {
    if (!account.nickname.empty()) {
      continue;
    }
    // 名字写在文章页的 var nickname 里；历史消息页没有这个字段，跳过。
    if (account.source_url.find("/s?") == std::string::npos &&
        account.source_url.find("/s/") == std::string::npos) {
      wxmd::warn("公众号 " + account.biz + " 的凭证来自非文章页（" +
                 account.source_url +
                 "），页内没有名字，列表只能先显示 __biz；"
                 "在微信里打开该号任意一篇文章即可解析出名字。");
      continue;
    }
    const std::string name = wxmd::fetch_account_name(account.source_url);
    if (name.empty()) {
      wxmd::warn("从文章页解析公众号名字失败（" + account.source_url +
                 "），列表暂显示 __biz " + account.biz);
    }
    creds.set_nickname(account.biz, name);
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

// 边刷列表边等回车；用户放弃则返回空。
std::vector<wxmd::Account> select_accounts(wxmd::Credentials &creds,
                                           int64_t since_ms) {
  size_t known = 0;

  for (;;) {
    std::vector<wxmd::Account> accounts = captured_since(creds, since_ms);
    if (accounts.size() != known) {
      resolve_names(creds, accounts);
      accounts = captured_since(creds, since_ms);
      known = accounts.size();
      print_accounts(accounts);
    }

    std::string line;
    if (!read_line_timeout(line, kRefreshMs)) {
      continue;
    }

    line = wxmd::str::trim(line);
    if (line == "q") {
      return {};
    }
    if (accounts.empty()) {
      std::cout << "还没抓到凭证，先在微信里打开一篇文章\n> " << std::flush;
      continue;
    }

    std::vector<wxmd::Account> valid;
    for (wxmd::Account &account : accounts) {
      if (account.credential_seconds() > 0) {
        valid.push_back(std::move(account));
        continue;
      }
      wxmd::warn("凭证已过期，跳过: " + account.label());
    }
    WXMD_ASSERT(!valid.empty(),
                "捕获到的凭证全部已过期，请在微信里重新打开文章");
    return valid;
  }
}

// 起代理收一轮凭证，收完立刻把系统代理还回去。收下的凭证一并进 creds。
// 之后的翻页与抓正文都是我们自己直连，不再需要接管环境。
std::vector<wxmd::Account> capture_session(wxmd::Credentials &creds) {
  WXMD_ASSERT(wxmd::has_command("gsettings"),
              "找不到 gsettings，本工具靠它临时接管系统代理；"
              "非 GNOME 桌面暂不支持");
  if (!ensure_certutil()) {
    return {};
  }

  const std::string base = wxmd_dir();
  const std::string backup = base + "/proxy-backup.json";

  // 上次异常退出留下的代理残留，已在 main() 入口无条件自愈过，这里直接读当前
  // 值。原有的系统代理成为我们的上游，用户其它流量的走向保持不变。
  const wxmd::SystemProxy saved = wxmd::read_system_proxy();
  const bool chain = saved.mode == "manual" && !saved.https.host.empty();

  wxmd::MitmProxy proxy(kProxyPort, {wxmd::config::kTargetHost}, base,
                        chain ? saved.https.host : std::string(),
                        chain ? saved.https.port : 0);
  proxy.set_handler([&creds](const wxmd::Exchange &exchange) {
    if (const std::optional<wxmd::Account> account =
            wxmd::parse_exchange(exchange)) {
      creds.offer(*account);
    }
  });

  if (wxmd::config::kDump) {
    const std::string dir = "store/dump";
    proxy.set_dump_dir(dir);
    std::cerr << "[wxmd] dump 已开启，落盘到 " << dir
              << "（对所有域名 MITM）\n";
  }

  proxy.start();

  wxmd::install_ca_to_nssdb(proxy.ca_cert_path());

  wxmd::save_proxy_backup(backup, saved);
  g_backup_path = backup;
  g_saved_proxy = saved;
  g_restore_armed = 1;
  std::atexit(restore_now); // 任何 return/exit 路径的兜底还原。
  arm_restore_signals();

  const int64_t session_start = wxmd::now_ms();
  wxmd::take_over_system_proxy(proxy.port());

  std::cout
      << "\n就绪。在微信里打开想同步的公众号的任意一篇文章即可。\n"
         "想同步几个号就逐个点开，凭证会一起收下（每个号 25 分钟有效）。\n";
  if (wxmd::process_running("wechat")) {
    std::cout << "（抓不到就重启一次微信：它只在启动时读代理和证书）\n";
  }
  std::cout << "回车收下已捕获的全部，q 放弃。\n"
            << "----\n";

  std::vector<wxmd::Account> picked = select_accounts(creds, session_start);
  if (!picked.empty()) {
    std::cerr << "\n已收下 " << picked.size() << " 个号的凭证，正在断开代理…\n";
  }

  restore_now();
  // 先落盘再停代理：proxy.stop() 偶尔会卡在 inflight 不归零，若被 Ctrl-C 打断，
  // 凭证至少已经写进 credentials.json，不会白抓一趟。
  creds.save();
  proxy.stop();
  return picked;
}

// ------------------------------------------------------------------ 两种模式

// 主 flow：先维护已有的号，再问要不要加新的。
void run_sync(const std::string &root) {
  wxmd::Credentials creds(wxmd_dir() + "/credentials.json");

  std::vector<wxmd::Account> known = wxmd::list_accounts(root);
  std::cout << "缓存目录: " << root << "（已有 " << known.size()
            << " 个公众号）\n";
  for (wxmd::Account &account : known) {
    creds.fill(account);
    wxmd::sync_account(root, account);
  }

  std::cout << "\n";

  // 收下的凭证已经在 capture_session 里落盘了（那一步必须发生在停代理之前）。
  for (const wxmd::Account &account : capture_session(creds)) {
    wxmd::sync_account(root, account);
  }
}

// 一键卸载：还原可能残留的系统代理，摘掉 NSS 里的 CA 信任，删掉 ~/.wxmd。
// 走完之后机器回到用本工具之前的干净状态（不动 store/）。
void run_uninstall() {
  const std::string base = wxmd_dir();

  if (wxmd::restore_proxy_backup(base + "/proxy-backup.json")) {
    std::cout << "已还原残留的系统代理设置。\n";
  }
  wxmd::uninstall_ca_from_nssdb();
  std::cout << "已从证书库移除本工具的 CA 信任。\n";

  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::cout << "已删除 " << base << "（CA 证书 / 私钥 / 凭证 / 备份）。\n";
}

} // namespace

int main(int argc, char **argv) {
  // 忽略 SIGPIPE：往对端已关闭的 socket 写会触发它，默认动作是静默杀掉整个
  // 进程。我们的明文转发用了 MSG_NOSIGNAL，但 OpenSSL 的 SSL_write 底层 write
  // 不带它，微信随时会掐断连接、stop() 也会主动 shutdown，必须全局忽略，让写
  // 操作只是返回错误由各自的循环处理，而不是把进程带走。
  std::signal(SIGPIPE, SIG_IGN);

  // 无论这次跑哪种模式：上次若被强杀/崩溃，系统代理可能还指着我们的死端口。
  // 先无条件自愈——没有残留备份时这步只是打开一个不存在的文件后立即返回，
  // 代价可忽略，却保证了「不管上次怎么挂的，这次一开机就是干净环境」。
  if (wxmd::restore_proxy_backup(wxmd_dir() + "/proxy-backup.json")) {
    std::cerr << "检测到上次异常退出，已自动还原系统代理。\n";
  }

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "--uninstall") {
      run_uninstall();
      return 0;
    }
    WXMD_ASSERT(false, "认不出的参数: " + arg + "（-h 看用法）");
  }

  // 走隧道还是直连由 config::kUseTunnel 在编译期定死，开局打印一声让人有数。
  const wxmd::EgressPolicy &policy = wxmd::egress();
  std::cerr << "出网: ";
  if (policy.tunneled()) {
    std::cerr << "隧道 " << policy.host << ":" << policy.port;
  } else {
    std::cerr << "直连";
  }
  std::cerr << "（" << policy.qps << " 次/s，" << policy.workers
            << " 并发抓取）\n";

  run_sync(wxmd::config::kStoreDir);
  return 0;
}
