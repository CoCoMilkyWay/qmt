#pragma once

#include "api/bigquant/https.hpp"
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

// BigQuant DAI 客户端 — 完全脱离 bigquant_core 的纯 C++ 等价实现.
//
// 协议拆分 (与官方 SDK 二进制完全对账):
//   控制面: HTTPS + HMAC headers (whoami / get schema), 见 bigquant::Https
//   数据面: Arrow Flight, grpc+tcp://bigquant.com:17010 (明文 gRPC, 无 TLS)
//          认证走 Basic Token -> JWT bearer, 每个 DoGet 自动挂 authorization header
//          ticket payload = UTF-8 JSON {sql,full_db_scan,filters,params}
//
// 生命周期:
//   - Https 子客户端构造即可用 (无握手).
//   - Flight 客户端 + JWT 在首次 query() 时 lazy 建立; JWT TTL 约 12h, 过期由调用方重建 DaiClient.
//   - 所有错误一律 assert; 网络瞬抖在 Https 层做有限重试, Flight 层不重试 (gRPC 自带 backoff).
class DaiClient {
public:
  // 缺省构造: ak/sk 取 config.hpp::BIGQUANT_AK/SK
  DaiClient();
  // 显式指定 (测试 / 多账号切换)
  DaiClient(std::string ak, std::string sk);

  // GET /bigapis/auth/v1/users/me — 验证 ak/sk 可用; root = user 对象 (id/username/...).
  yyjson_doc *whoami();

  // GET /bigapis/data/v1/spacedatasources/spaces/{space}/datasources/{ds_id} — 原始 schema.
  // 返回 doc root = {id, user_id, metadata{schema, ...}, docs{schema, ...}, ...}.
  // metadata.schema 是 {col_name -> arrow_type_str} 映射, 是 DAI 给的权威列定义.
  yyjson_doc *
  get_datasource_schema(std::string_view datasource_id,
                        std::string_view space_id = "00000000-0000-0000-0000-000000000000");

  // 执行 SQL, 走 Arrow Flight DoGet.
  //   sql:           标准 DuckDB-flavored SQL (DAI 底层用 DuckDB)
  //   filters:       服务端列过滤 / 分区下推
  //   full_db_scan:  缺省 false; true 时显式跳过 server-side 安全限制 (大查询)
  // 返回 arrow::Table (整张读完一次性返); server-side 配额耗尽 / 权限缺失 / SQL 错误均 assert.
  std::shared_ptr<arrow::Table>
  query(std::string_view sql, const DaiFilters &filters = {}, bool full_db_scan = false);

private:
  void ensure_flight();

  std::string ak_;
  std::string sk_;
  Https https_;
  std::unique_ptr<arrow::flight::FlightClient> flight_;
  // Pair<header_name, header_value> 从 AuthenticateBasicToken 返回, e.g. ("authorization", "Bearer <JWT>").
  std::pair<std::string, std::string> flight_auth_;
};

} // namespace bigquant
