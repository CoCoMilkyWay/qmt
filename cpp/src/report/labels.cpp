#include "report/labels.hpp"

#include "misc/parquet.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace report {

std::vector<std::string> load_industry_labels(const feature::Axes &axes) {
  std::vector<std::string> out(static_cast<std::size_t>(axes.n_a()), "未知");

  auto files = misc::pq::list_month_files("cn_stock_industry_component");
  assert(!files.empty() &&
         "data/YYYY-MM/cn_stock_industry_component.parquet missing — "
         "先跑 bigquant::update");

  // ym 升序 ⇒ 从最后一个月往前找第一个有行的文件 (当月可能还是 0 行).
  for (auto it = files.rbegin(); it != files.rend(); ++it) {
    misc::pq::TableView v(misc::pq::read_table(it->second));
    if (v.rows() == 0)
      continue;
    misc::pq::Col ins = v.col("instrument");
    misc::pq::Col src = v.col("industry");
    misc::pq::Col l1 = v.col("industry_level1_name");
    misc::pq::Col l2 = v.col("industry_level2_name");
    for (std::int64_t i = 0, nr = v.rows(); i < nr; ++i) {
      if (src.str(i) != "sw2021")
        continue;
      auto ci = axes.code_idx.find(std::string(ins.str(i)));
      if (ci == axes.code_idx.end())
        continue;
      std::string_view a1 = l1.str(i);
      std::string_view a2 = l2.str(i);
      if (a1.empty())
        continue;
      std::string label(a1);
      if (!a2.empty()) {
        label += " -- ";
        label += a2;
      }
      out[static_cast<std::size_t>(ci->second)] = std::move(label);
    }
    return out;
  }

  assert(false && "cn_stock_industry_component 全部月份 0 行");
  return out;
}

} // namespace report
