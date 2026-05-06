#pragma once

#include "package/yyjson/yyjson.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tushare {

class Http;
struct InterfaceSpec;

struct FetchTask {
  std::string start;
  std::string end;
};

class FetchStrategy {
public:
  virtual ~FetchStrategy() = default;
  virtual std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const = 0;
  virtual yyjson_doc *fetch(Http &http, const FetchTask &task,
                            const InterfaceSpec &spec) const = 0;
};

class DateRangeStrategy : public FetchStrategy {
public:
  explicit DateRangeStrategy(int max_days = 31) : max_days_(max_days) {}
  std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const override;
  yyjson_doc *fetch(Http &http, const FetchTask &task,
                    const InterfaceSpec &spec) const override;

private:
  int max_days_;
};

class SingleDateStrategy : public FetchStrategy {
public:
  explicit SingleDateStrategy(std::string visible_param)
      : visible_param_(std::move(visible_param)) {}
  std::vector<FetchTask>
  plan(const std::vector<std::string> &missing) const override;
  yyjson_doc *fetch(Http &http, const FetchTask &task,
                    const InterfaceSpec &spec) const override;

private:
  std::string visible_param_;
};

struct InterfaceSpec {
  std::string name;
  std::string api;
  std::string visible_date_field;
  std::vector<std::string> pk;
  std::shared_ptr<FetchStrategy> strategy;
};

extern const std::vector<InterfaceSpec> SPECS;

std::string today_yyyymmdd();
std::vector<std::string> iter_days(std::string_view start, std::string_view end);
std::string add_days(std::string_view yyyymmdd, int days);

} // namespace tushare
