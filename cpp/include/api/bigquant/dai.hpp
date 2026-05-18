#pragma once

#include "package/yyjson/yyjson.h"

#include <arrow/flight/client.h>
#include <arrow/table.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bigquant {

// filters 与 SDK 一致: {column -> [v1, v2, ...]}. server-side 列裁剪 + 谓词下推关键入口.
//   - 用 std::string 而非具体 type: 所有 filter 值最终都序列化为 JSON 字符串数组.
//   - 例: {{"date", {"2024-12-31", "2024-12-31"}}} == python {"date": ["2024-12-31","2024-12-31"]}
using DaiFilters = std::map<std::string, std::vector<std::string>>;

// BigQuant DAI 客户端 — 纯数据面 (Arrow Flight) 等价实现.
//
// 协议:
//   grpc+tcp://bigquant.com:17010 (明文 gRPC, 无 TLS)
//   认证: Basic Token (ak/sk) -> JWT bearer, 每个 DoGet 自动挂 authorization header
//   ticket payload = UTF-8 JSON {sql, full_db_scan, filters, params}
//
// 生命周期:
//   - 构造即可用; Flight 客户端 + JWT 在首次 query() 时 lazy 建立.
//   - JWT TTL 约 12h, 过期由调用方重建 DaiClient.
//   - 所有错误一律 assert; gRPC 自带 backoff, 这里不再做应用层重试.
class DaiClient {
public:
  // 缺省构造: ak/sk 取 config.hpp::BIGQUANT_AK/SK
  DaiClient();
  // 显式指定 (测试 / 多账号切换)
  DaiClient(std::string ak, std::string sk);

  // 执行 SQL, 走 Arrow Flight DoGet.
  //   sql:     标准 DuckDB-flavored SQL (DAI 底层用 DuckDB)
  //   filters: 服务端列过滤 / 分区下推
  // 返回 arrow::Table (整张读完一次性返); server-side 配额耗尽 / 权限缺失 / SQL 错误均 assert.
  std::shared_ptr<arrow::Table>
  query(std::string_view sql, const DaiFilters &filters = {});

private:
  void ensure_flight();

  std::string ak_;
  std::string sk_;
  std::unique_ptr<arrow::flight::FlightClient> flight_;
  // Pair<header_name, header_value> 从 AuthenticateBasicToken 返回, e.g. ("authorization", "Bearer <JWT>").
  std::pair<std::string, std::string> flight_auth_;
};

} // namespace bigquant
