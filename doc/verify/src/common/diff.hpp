#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace verify::diff {

// 数值字段的比对参数. 对 (date, ts_code) 同时在 ts/bq 出现的记录,
// 任意 FieldSpec 不通过 → 该条记 diff.
struct FieldSpec {
    std::string name;     // 在 fields map 里的 key (两边映射后已统一)
    bool numeric = true;  // false: 字符串严格 ==
    double scale = 1.0;   // ts 字段值 * scale 后与 bq 比
    double eps = 1e-6;    // 数值容差: |ts*scale - bq| < eps * max(1, |bq|)
};

using FieldMap = std::unordered_map<std::string, std::string>;

// 按年聚合, 一个 YearReport 对应 out/<api>/<year>.json.
class YearReport {
public:
    YearReport(int year, std::vector<FieldSpec> field_specs);

    // 同 (instrument, date) 重复 add 会覆盖 (按需). 通常不应重复.
    void add_ts(std::string instrument, std::string date, FieldMap fields);
    void add_bq(std::string instrument, std::string date, FieldMap fields);

    // 统计 + 写 out_dir / "<year>.json".
    void write(const std::filesystem::path &out_dir);

    int year() const { return year_; }
    std::size_t ts_count() const { return ts_.size(); }
    std::size_t bq_count() const { return bq_.size(); }

private:
    int year_;
    std::vector<FieldSpec> specs_;
    std::unordered_map<std::string, FieldMap> ts_;  // key = "inst|date"
    std::unordered_map<std::string, FieldMap> bq_;
};

// 多年报表容器: verifier 用这个一次性管理整段年份范围.
class MultiYearReport {
public:
    MultiYearReport(std::string api,
                    int year_from, int year_to,
                    std::vector<FieldSpec> field_specs);

    // 加入一条记录到对应年份 (按 date 的前 4 位选 year).
    void add_ts(const std::string &instrument, const std::string &date, FieldMap fields);
    void add_bq(const std::string &instrument, const std::string &date, FieldMap fields);

    // 写 out_dir/<api>/<year>.json (每年一份, 包括 0 条差异的年份).
    void write_all(const std::filesystem::path &out_root);

    const std::string &api() const { return api_; }

private:
    std::string api_;
    int year_from_, year_to_;
    std::vector<FieldSpec> specs_;
    std::unordered_map<int, YearReport> years_;

    YearReport &get_year(const std::string &date);
};

} // namespace verify::diff
