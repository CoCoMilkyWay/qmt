#pragma once

#include <string>

#include "wxmd/model.hpp"

namespace wxmd {

// 主 flow：同步一个号。
//
// 先把队列里剩下的文章补齐，再去发现新的——两段的约束完全不同：
//   补齐（抓正文与图片）走公开 URL，不需要凭证，逐篇原子提交，随时可中断续跑；
//   发现（翻历史消息）要凭证，且只有整趟翻完才能写队列，中断会留空洞。
// 补齐排在发现之前不是随意排的：补齐会把水位线往前推，紧接着的发现就能更早停。
//
// account 没有凭证（或凭证已失效）时只做补齐——这一步永远能做。
// 进度直接打给人看：这是一条一次可能跑上几十分钟的流程。
void sync_account(const std::string &root, const Account &account);

} // namespace wxmd
