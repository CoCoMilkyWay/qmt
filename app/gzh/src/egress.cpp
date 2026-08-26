#include "wxmd/egress.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

#include "config.hpp"

namespace wxmd {
namespace {

// 参数不合法就编译不过，这是能失败的最早时刻。
static_assert(config::kTunnelQps > 0 && config::kDirectQps > 0);
static_assert(config::kTunnelWorkers >= 1);

EgressPolicy make_tunnel() {
  EgressPolicy policy;
  policy.host = config::kTunnelHost;
  policy.port = config::kTunnelPort;
  policy.user = config::kTunnelUser;
  policy.pass = config::kTunnelPass;
  policy.qps = config::kTunnelQps;
  policy.workers = config::kTunnelWorkers;
  return policy;
}

// 直连只改频率：本机就一个出口 IP，并发只会让它更显眼，workers 保持默认的 1。
EgressPolicy make_direct() {
  EgressPolicy policy;
  policy.qps = config::kDirectQps;
  return policy;
}

EgressPolicy g_policy = config::kUseTunnel ? make_tunnel() : make_direct();

// 下一个可以发请求的时刻。均匀发牌而不是攒桶再爆发：隧道对持续超频会直接拒，
// 直连突发只会更显眼，而我们没有任何场景需要突发。
std::mutex g_mutex;
std::chrono::steady_clock::time_point g_next_slot;

} // namespace

const EgressPolicy &egress() { return g_policy; }

void throttle() {
  const std::chrono::microseconds spacing(1000000 / g_policy.qps);

  std::chrono::steady_clock::time_point slot;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_next_slot = std::max(std::chrono::steady_clock::now(), g_next_slot);
    slot = g_next_slot;
    g_next_slot += spacing;
  }
  // 等在锁外：每个线程拿到的是各自不同的时间片，没必要互相堵着。
  std::this_thread::sleep_until(slot);
}

} // namespace wxmd
