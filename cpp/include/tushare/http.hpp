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
  // body = {api_name, token, params, fields}
  // fields 缺省 ""：tushare 仅返回"默认显示=Y"的列；
  //   传 "f1,f2,..." 显式锁定返回列 (含默认隐藏列)，顺序也与请求一致。
  // 返回的 yyjson_doc* 由 caller 用 yyjson_doc_free 释放
  // code != 0 / 网络异常 直接 assert
  yyjson_doc *
  call(std::string_view api_name,
       const std::vector<std::pair<std::string, std::string>> &params,
       std::string_view fields = "");

private:
  std::string token_;
};

} // namespace tushare
