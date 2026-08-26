"""快代理隧道 (按量付费, 每请求换出口 IP) 的共享配置与全局流控。

与 gzh/src/config.hpp 是同一个订单 (928771545382112)。果仁与聚宽都走这条隧道:
页面请求经隧道换 IP, 防本机 IP 被封; 图片各站自己直连 (见 site 的 asset)。

订单额度 (控制台): 并发请求量 10 次/s, 带宽峰值 20Mbps (超了是排队不是拒绝),
官方另有一条建议「1 分钟内不超过 600 次」。额度属于订单, 不属于站点 —— 所以限速点
在这里而不在各站的 Fetcher.qps, 见 acquire()。

隧道自己给的错误码 (官方表 kuaidaili.com/doc/dev/tpshttpresponse) 不是目标站的响应。
既然只拿隧道访问具体域名, 这些码全是死结 — 账号密钥、实名、白名单、或域名被禁 —
换 IP 重试也一样, 当场炸掉比默默重试强。剩下的 440/441 (带宽/超频) 与 515-517
(代理抖动) 才是瞬时的, 换个 IP 再来。
"""

import fcntl
import os
import threading
import time

ORDER = "928771545382112"
HOST = "i969.kdltps.com"
PORT = 15818
USER = "t18771545382113"
PASS = "tlh1s2tx"

URL = f"http://{USER}:{PASS}@{HOST}:{PORT}"
PROXIES = {"http": URL, "https": URL}

# 总开关, 所有站点共用一个 (走不走隧道是出口的属性, 不是站点的)。False = 全部直连,
# 只在调试时用: 拿本机 IP 抓几万篇文章迟早被风控盯上, 果仁已经被封过一次。
ENABLED = True

# 订单额度就是 10 次/s。均匀发牌到这个速率时任意 60 秒窗口内最多 600 次, 官方那条
# 「1 分钟不超过 600 次」自动满足, 不必再叠一个分钟窗口计数器。
QPS = 10

# 实际按额度的 95% 发牌, 不贴着 10.0 走: 均匀发牌到正好 10 次/s 时, 起点压在某张牌上
# 的那个 1 秒窗口会数出 11 张 —— 对方怎么切窗口我们不知道, 而超频一次要付约 80 秒
# 全量 RST 的代价, 拿 5% 吞吐换掉这个边界风险很值。
MARGIN = 0.95
_INTERVAL = 1.0 / (QPS * MARGIN)

# 进程级的闸只管得住本进程, 而额度是整台机器共享的一份。所以再加一道跨进程独占:
# 第一次出网时抢这把文件锁, 抢不到就当场炸 —— 两个进程各自守着 10 次/s 就是 20 次/s,
# 而超频的现象 (见 acquire) 极难归因, 早炸远比事后查强。进程死了锁由内核释放, 不会留残。
_LOCK_PATH = f"/tmp/qmt-tunnel-{ORDER}.lock"

# 惩罚窗口的熔断参数, 见 failed()。
TRIP = 5
PAUSE = 90.0
PAUSE_CAP = 900.0

_lock = threading.Lock()
_next = 0.0
_lock_fd = None
_fails = 0
_until = 0.0
_pause = PAUSE


def _grab(fd):
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except BlockingIOError:
        return False


def _claim():
    global _lock_fd
    if _lock_fd is not None:
        return
    fd = os.open(_LOCK_PATH, os.O_CREAT | os.O_RDWR, 0o644)
    holder = os.read(fd, 64).decode(errors="replace").strip() or "?"
    assert _grab(fd), (
        f"隧道订单 {ORDER} 已被 pid {holder} 占着 (锁 {_LOCK_PATH})。额度是整个订单的"
        f" {QPS} 次/s, 两个进程一起跑就是两倍, 超频会招来约 80 秒全量 RST。"
        f" 等它跑完, 或者先停掉它。"
    )
    os.truncate(fd, 0)
    os.lseek(fd, 0, os.SEEK_SET)  # 上面的 read 把偏移推到了 64, 不回零会写出一堆空洞
    os.write(fd, str(os.getpid()).encode())
    _lock_fd = fd


def acquire():
    """出网前排队领一个发牌位。走隧道的每个请求 (含重试) 都必须先过这里。

    这是全进程唯一的限速点: 额度是隧道订单的, 果仁和聚宽同用一个订单, 各站按自己的
    qps 发牌会加起来超频。均匀发牌不攒桶爆发 —— 攒桶意味着允许瞬时爆发, 而爆发正是
    隧道最不能忍的。

    超频的代价远不止被拒这一次: 实测瞬时打出 40 个并发请求后, 隧道会连续约 80 秒对
    所有请求 (哪怕之后是单发) 直接 RST 或截断响应 (IncompleteRead), 期间在抓的文章
    成批变空洞, 而且现象看起来像「目标站封了隧道出口 IP」, 极难归因。宁可发慢点。
    """
    global _next
    with _lock:
        _claim()
        slot = max(time.monotonic(), _next)
        _next = slot + _INTERVAL
    wait = slot - time.monotonic()
    if wait > 0:
        time.sleep(wait)


FATAL = {
    407,
    442,
    443,
    445,
    446,
    447,
    448,
    449,
    450,
    452,
    453,
    454,
    455,
    460,
    466,
}
