#pragma once

// 全项目的可调参数集中在这里，与 strutil / fsutil 一样是内部头（不进 include/：
// 它不是这个库对外的形状，只是这次构建选的一组数）。
// 不需要操心、不需要改的常量（协议常量、实现细节）仍留在各处原位。

namespace wxmd::config {

// 微信目标主机。抓包白名单与 fetch_raw 的 host 校验共用同一个。
constexpr const char *kTargetHost = "mp.weixin.qq.com";

// 缓存根目录，相对当前工作目录（run.py always cd 到项目根）。换目录是
// 搬家而不是日常操作，真要换直接改这里重编译。
constexpr const char *kStoreDir = "store";

// dump 开关：true 则对所有域名 MITM 并把每次往返落盘到 store/dump。
// 改了要重编译——全量 MITM 风险大，正好逼着想清楚再开。
constexpr bool kDump = true;

// 出网走不走隧道。true = 快代理隧道（并发、每请求换 IP）；false = 本机直连
// （串行）。改成 false 重编译即退回直连，不再开局探测——出网方式由这一个开关
// 决定，全程不变，比运行时探测再降级好推理得多。
constexpr bool kUseTunnel = false;

// 微信内置浏览器的 User-Agent。微信版本变化时可能需要跟着改。
constexpr const char *kUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) "
    "Chrome/107.0.0.0 Safari/537.36 MicroMessenger/6.8.0(0x16080000) "
    "NetType/WIFI "
    "MiniProgramEnv/Mac MacWechat/WECHAT/WeChatBrowser XWEB/1191";

// 快代理隧道（按量付费，每次请求换 IP）。账号密码就写在这里：项目惯例是
// 「参数写死在文件顶部，改了要重编译，正好逼着想清楚再改」，而这几个号敞口
// 有限。主入口不通就换备用 i970.kdltps.com，端口相同。
constexpr const char *kTunnelHost = "i969.kdltps.com";
constexpr int kTunnelPort = 15818;
constexpr const char *kTunnelUser = "t18771545382113";
constexpr const char *kTunnelPass = "tlh1s2tx";

// 两种出网方式都只用「每秒多少个请求」描述频率，由 throttle() 统一发牌。
//
// 隧道：订单买的就是 10 次/s（后台另一处写成「1 分钟内不超过 600 次」，同一个
// 意思），超了返回 441。
// 直连：本机出口 IP，天花板是「别被风控盯上」而不是吞吐。2 次/s ⇒ 一篇
// （实测 4.85 请求）至少 2.4s，与从前「抓取时间 + 篇间 800ms」同一量级。
constexpr int kTunnelQps = 10;
constexpr int kDirectQps = 2;

// workers 远大于 qps 是刻意的：qps 卡发牌速率，workers 卡能藏住多少单请求延迟。
// 实测单流过隧道只 ~330KB/s，订单额度 2.5MB/s（20Mbps）且是订单总额非单 IP 独享
// （6 路并发聚合仍卡 ~600KB/s）——不并发就是浪费额度。32 个 worker 能在单请求
// 几百毫秒延迟下跑满 10 次/s，代价只是内存：一篇连图压缩后约 1.2MB，32
// 篇同时在手约 40MB。
//
// 两个天花板刚好咬合：qps 10 ÷ 4.85 请求/篇 ≈ 2.06 篇/s；带宽 2.5MB/s ÷ 1.18MB
// ≈ 2.12 篇/s。 超带宽在后台是「请求排队」，满了只慢不错。
//
// 直连没有这一项：本机就一个出口 IP，并发只会让同一个 IP 更显眼，永远是 1。
constexpr int kTunnelWorkers = 32;

// profile_ext 翻页：每页条数与翻页间隔。没做成命令行开关：日常用不到，
// 太大容易撞风控，改了要重编译，正好逼着想清楚再改。2s 是实测能稳定过风控的
// 翻页间隔。
constexpr int kPageSize = 20;
constexpr int kPageIntervalMs = 2000;

} // namespace wxmd::config
