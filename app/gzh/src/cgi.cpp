#include "wxmd/cgi.hpp"

#include <quickjs.h>

#include <chrono>

#include "wxmd/assert.hpp"

namespace wxmd {
namespace {

// 防御性上限：避免异常脚本卡死 isolate，与原实现的 EVAL_DEADLINE_MS 一致。
constexpr int kEvalDeadlineMs = 2000;
constexpr size_t kMemoryLimit = 256u * 1024u * 1024u;

struct Deadline {
  std::chrono::steady_clock::time_point until;
};

int interrupt_handler(JSRuntime *, void *opaque) {
  const Deadline *deadline = static_cast<const Deadline *>(opaque);
  return std::chrono::steady_clock::now() > deadline->until ? 1 : 0;
}

// 取回 JS 异常的文本描述，用于断言信息。
std::string exception_text(JSContext *ctx) {
  JSValue exception = JS_GetException(ctx);
  const char *str = JS_ToCString(ctx, exception);
  std::string out = str == nullptr ? "(无法读取异常信息)" : str;
  if (str != nullptr) {
    JS_FreeCString(ctx, str);
  }
  JS_FreeValue(ctx, exception);
  return out;
}

// 以局部 window 作为脚本的 this 与全局对象，执行后在 isolate 内 JSON
// 序列化回传。 在 isolate 内完成 JSON.stringify，可天然处理超大
// content_noencode 字符串并丢弃函数。
std::string make_wrapper(const std::string &script) {
  return "(function () {\n"
         "  var window = {};\n"
         "  var console = { log() {}, info() {}, warn() {}, error() {}, "
         "debug() {} };\n"
         "  var executionError = null;\n"
         "  try {\n"
         "    (function () {\n" +
         script +
         "\n    }).call(window);\n"
         "  } catch (err) {\n"
         "    executionError = (err && err.message) ? err.message : "
         "String(err);\n"
         "  }\n"
         "  var cgiData = (executionError === null && window.cgiDataNew !== "
         "undefined) ? window.cgiDataNew : null;\n"
         "  try { return JSON.stringify({ executionError: executionError, "
         "cgiData: cgiData }); }\n"
         "  catch (e) { return JSON.stringify({ executionError: executionError "
         "|| 'serialize failed', cgiData: null }); }\n"
         "})()";
}

} // namespace

nlohmann::json eval_cgi(const std::string &script) {
  WXMD_ASSERT(!script.empty(), "cgi 脚本为空");

  JSRuntime *runtime = JS_NewRuntime();
  WXMD_ASSERT(runtime != nullptr, "JS_NewRuntime 失败");
  JS_SetMemoryLimit(runtime, kMemoryLimit);

  Deadline deadline{std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(kEvalDeadlineMs)};
  JS_SetInterruptHandler(runtime, interrupt_handler, &deadline);

  JSContext *ctx = JS_NewContext(runtime);
  WXMD_ASSERT(ctx != nullptr, "JS_NewContext 失败");

  const std::string wrapper = make_wrapper(script);
  JSValue result = JS_Eval(ctx, wrapper.data(), wrapper.size(), "<cgi>",
                           JS_EVAL_TYPE_GLOBAL);

  std::string json_text;
  std::string failure;

  if (JS_IsException(result)) {
    failure = "QuickJS 执行 cgi 脚本失败: " + exception_text(ctx);
  } else {
    const char *str = JS_ToCString(ctx, result);
    if (str == nullptr) {
      failure = "cgi 脚本返回值无法转为字符串";
    } else {
      json_text.assign(str);
      JS_FreeCString(ctx, str);
    }
  }

  JS_FreeValue(ctx, result);
  JS_FreeContext(ctx);
  JS_FreeRuntime(runtime);

  WXMD_ASSERT(failure.empty(), failure);

  nlohmann::json parsed = nlohmann::json::parse(json_text);
  WXMD_ASSERT(parsed.contains("executionError") && parsed.contains("cgiData"),
              "沙箱返回结构不符合预期");
  WXMD_ASSERT(parsed["executionError"].is_null(),
              "cgi 脚本执行出错: " + parsed["executionError"].dump());
  WXMD_ASSERT(!parsed["cgiData"].is_null(), "cgi 脚本未产出 window.cgiDataNew");

  return parsed["cgiData"];
}

} // namespace wxmd
