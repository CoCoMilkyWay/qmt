#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace wxmd {

// 在 QuickJS 沙箱内执行文章的 cgi 脚本，取回 window.cgiDataNew。
// 沙箱内只提供最小的 window / console，不暴露任何宿主能力（无网络、无文件）。
nlohmann::json eval_cgi(const std::string &script);

} // namespace wxmd
