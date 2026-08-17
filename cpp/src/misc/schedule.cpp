#include "misc/schedule.hpp"

#include "misc/date.hpp"
#include "misc/parquet.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace misc {

namespace fs = std::filesystem;

namespace {

// "YYYYMMDD" → "YYYY-MM"
std::string ym_of(std::string_view yyyymmdd) {
  assert(yyyymmdd.size() == 8);
  std::string ym(yyyymmdd.substr(0, 4));
  ym += '-';
  ym += yyyymmdd.substr(4, 2);
  return ym;
}

// "YYYY-MM" → 下月 "YYYY-MM"
std::string next_ym(std::string_view ym) {
  int y = std::stoi(std::string(ym.substr(0, 4)));
  int m = std::stoi(std::string(ym.substr(5, 2)));
  if (++m == 13) { m = 1; ++y; }
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%04d-%02d", y, m);
  return buf;
}

// 文件 mtime 距今秒数; 不存在返回 -1.
long file_age_seconds(const fs::path &p) {
  if (!fs::exists(p)) return -1;
  auto mt = fs::last_write_time(p);
  auto now = fs::file_time_type::clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now - mt).count();
}

// 文件 mtime → 写盘日 "YYYYMMDD"; 不存在返回空串.
std::string file_write_date(const fs::path &p) {
  if (!fs::exists(p)) return {};
  auto sys = std::chrono::clock_cast<std::chrono::system_clock>(
      fs::last_write_time(p));
  return fmt_yyyymmdd(std::chrono::floor<std::chrono::days>(sys));
}

} // namespace

std::vector<FetchMonth> plan_months(std::string_view name,
                                    std::string_view start_date,
                                    std::string_view today, int lookback_days,
                                    int dedup_seconds) {
  assert(start_date.size() == 8 && today.size() == 8 && start_date <= today);
  std::vector<FetchMonth> out;

  // 关月判定线: 月末 < frozen_before 的月不再变动.
  std::string frozen_before = add_days(today, -lookback_days);

  std::string end_ym = ym_of(today);
  for (std::string ym = ym_of(start_date);; ym = next_ym(ym)) {
    std::string yyyymm = ym.substr(0, 4) + ym.substr(5, 2);
    std::string m_first = yyyymm + "01";
    std::string m_last = yyyymm + month_last_dd(yyyymm);

    fs::path p = pq::month_path(ym, name);
    bool closed = m_last < frozen_before;

    if (closed) {
      // 冻结条件 = 写盘日 ≥ 月末 + lookback: 开放月 fetch 只覆盖到写盘日
      // (end = min(月末, today)), 写盘时该月已出 lookback 窗口才能保证
      // "整月行 + 全部回填" 都已入盘. 月中写的半月文件在跑批空窗期后关月,
      // 此处整月重拉一次 (写盘日=today ⇒ 之后永久冻结).
      std::string wd = file_write_date(p);
      if (wd.empty() || wd < add_days(m_last, lookback_days)) {
        out.push_back({ym, std::max(m_first, std::string(start_date)), m_last});
      }
    } else {
      long age = file_age_seconds(p);
      if (age < 0 || age >= dedup_seconds) {
        out.push_back({ym, std::max(m_first, std::string(start_date)),
                       std::min(m_last, std::string(today))});
      }
    }

    if (ym == end_ym) break;
  }
  return out;
}

bool file_fresh(const fs::path &p, int seconds) {
  long age = file_age_seconds(p);
  return age >= 0 && age < seconds;
}

} // namespace misc
