#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "wxmd/model.hpp"

namespace wxmd {

// 历史消息列表（mp/profile_ext）是整个流程里唯一需要凭证的一段，也是唯一
// 「不能中断续跑」的一段——所以这一层只对外提供一个「整趟翻完」的入口。

// 凭证还能不能用。只探一条，不解析列表。过期在增量流程里是常态而不是 bug
// （分钟级失效且无法续签），所以这里返回状态而不是断言。
bool probe_credential(const Account &account);

// 增量发现：从最新一页往回翻，撞上水位线就停（since 为 0 则拉全量）。
// 历史消息严格按群发时间倒序，所以本页一出现早于 since 的文章就可以收工。
// 但一次群发的多篇共享同一个发布时间，单看时间会把同批里还没入库的几篇一起
// 挡掉，所以再用 known 逐条问一句「这篇是不是已经入库了」。
// 翻页进度直接打在 stderr：全量第一趟可能几十页、每页间隔一秒，不能闷着不响。
std::vector<Entry>
fetch_new_entries(const Account &account, int64_t since,
                  const std::function<bool(const std::string &id)> &known);

// 打开一篇文章，从页内的 `var nickname` / `nick_name` 取出公众号名称。
// 名称只是展示用的可选信息：解析不到就返回空串，由调用方退回显示 __biz，
// 不因页面结构变动而中断捕获。
std::string fetch_account_name(const std::string &article_url);

} // namespace wxmd
