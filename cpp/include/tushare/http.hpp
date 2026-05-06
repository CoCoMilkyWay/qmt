#pragma once

#include "package/yyjson/yyjson.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tushare {

class Http {
public:
  explicit Http(std::string token);

  // POST http://api.tushare.pro
  // body = {api_name, token, params, fields:""}
  // 返回的 yyjson_doc* 由 caller 用 yyjson_doc_free 释放
  // code != 0 / 网络异常 直接 assert
  yyjson_doc *
  call(std::string_view api_name,
       const std::vector<std::pair<std::string, std::string>> &params);

private:
  std::string token_;
};

std::string load_token();

} // namespace tushare
