#pragma once

#include "feature/axis.hpp"
#include "feature/graph.hpp"
#include "feature/pit.hpp"
#include "feature/tensor.hpp"
#include "misc/date.hpp"

#include <algorithm>
#include <cmath>
#include <string>

// list_age: D - list_date if D ≥ list_date else NaN.
//   PIT: 不写"距事件天数" (未来信息). 下游用 is_finite 判 "已上市".

namespace feature::def {

inline void ts_list_age(int a, const Axes &axes, const PitPool &pool,
                        const StockMeta &meta, Tensor &T);

inline constexpr FeatureSpec list_age_spec{
    "list_age", "上市龄", Kind::Inter, Axis::TimeSeries, {}, &ts_list_age, nullptr,
    /*must_be_finite=*/false,
    /*formula=*/"D − meta.list_date if D ≥ list_date else NaN",
    /*assumption=*/
    "[日历日]; 仅上市当日及之后写值 (上市当日=0), 否则 NaN; 下游 is_finite "
    "判\"已上市\""};

inline void ts_list_age(int a, const Axes &axes, const PitPool &,
                        const StockMeta &meta, Tensor &T) {
  int n_d = axes.n_d();
  auto out = T.ts_row(list_age_spec, a);
  const std::string &anchor = meta.list_date[a];
  if (anchor.size() != 8) {
    std::fill(out.begin(), out.end(), std::nanf(""));
    return;
  }
  auto ad = misc::parse_yyyymmdd(anchor);
  for (int d = 0; d < n_d; ++d) {
    float age = static_cast<float>((axes.date_days[d] - ad).count());
    out[d] = (age >= 0.0f) ? age : std::nanf("");
  }
}

} // namespace feature::def
