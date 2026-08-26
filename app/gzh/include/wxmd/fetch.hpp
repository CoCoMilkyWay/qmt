#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "wxmd/model.hpp"

namespace wxmd {

// 这个请求走哪条路出去（策略本身见 egress.hpp）。
//
// 绝大多数请求要的是「量大、能换 IP」，所以默认走隧道 —— 没开隧道时它自动
// 退回直连，调用方不必区分。唯一必须显式 Direct 的是带凭证的那一路：key 与
// pass_ticket 是微信客户端在某个 IP 上拿到的，同一个 key 从一堆轮换 IP 发请求
// 比固定 IP 更像异常；那一路本来也串行、量小，换 IP 一点好处都没有。
enum class Route { Tunneled, Direct };

// 以微信内置浏览器的身份 GET 一个 mp.weixin.qq.com 链接，返回响应体。
// cookie 为空时不发送该头：正文页是公开 URL，不需要 cookie；带 cookie 的那一路
// 是历史消息列表在用。重试耗尽仍拿不到 → 返回空 optional（见 get 的 warn），
// 由调用方决定是 fatal 还是降级成空洞。
std::optional<std::string> fetch_raw(const std::string &url,
                                     const std::string &cookie = {},
                                     Route route = Route::Tunneled);

// 抓正文里的第 seq 张图，顺带把落盘用的文件名定下来（扩展名先看 Content-Type，
// 再退回 URL 上的 wx_fmt，都认不出就当成失败）。图片不在 mp.weixin.qq.com 而在
// 腾讯的图片 CDN 上，所以另开一个入口，而不是把 fetch_raw 的白名单撕开。
// 图片是整个流程里请求量最大的一块（实测 3.85 张/篇），永远走隧道。
// 抓不到返回空 optional；入库要求「连图完整」，所以调用方（sync.cpp）会把整篇
// 作空洞留在队列里下轮重来，而不是入库一篇缺图的。
std::optional<AssetFile> fetch_asset(const std::string &url, size_t seq);

} // namespace wxmd
