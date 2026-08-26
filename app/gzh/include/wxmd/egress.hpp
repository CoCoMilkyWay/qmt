#pragma once

#include <string>

namespace wxmd {

// 请求怎么离开这台机器。全项目只在这一层描述，其余各层只读。
//
//   隧道 —— 快代理隧道，每个请求换一个出口 IP，可以并发抓。
//   直连 —— 本机出口 IP，串行。
//
// 走哪条路由 config::kUseTunnel 在编译期定死，全程不变；要换路就改 config
// 重编译。
//
// 两种方式的频率都只用 qps 描述、都由 throttle() 逐请求发牌：一处限速点比
// 「隧道按请求、直连按篇」两套机制好推理，抓一篇的图与正文也不再有突发。
//
// 不叫 proxy 是因为这名字已被 proxy.hpp 的 MitmProxy（抓凭证的中间人）与
// ~/.wxmd/proxy-backup.json（系统代理备份）占了两层含义，再撞只会误导，
// 所以叫 egress。
struct EgressPolicy {
  // 隧道入口。host 为空即直连，此时下面的账号密码不生效。
  std::string host;
  int port = 0;
  std::string user;
  std::string pass;

  // 每秒发多少个请求。隧道卖的就是这个数（超了返回 441），直连按「别被风控
  // 盯上」定。两种方式都非 0。
  int qps = 0;
  // drain 并发抓取的线程数。提交始终是串行的，见 sync.cpp。
  int workers = 1;

  bool tunneled() const { return !host.empty(); }
};

// 本次运行的出网策略。由 config::kUseTunnel 在开局定死，之后不再变。
const EgressPolicy &egress();

// 领一个发请求的时间片，到点才返回。fetch.cpp 每发一个请求前调一次，
// 是全项目唯一的限速点。
void throttle();

} // namespace wxmd
