#pragma once

#include <string>

namespace wxmd {

// HTML → Markdown。规则集移植自 turndown 7.x，并采用上游 markdown.ts 的配置：
// headingStyle=atx、bulletListMarker='-'、codeBlockStyle=fenced，
// 且移除 style/script/noscript/link/meta/title 与底部互动栏。
std::string to_markdown(const std::string &html);

} // namespace wxmd
