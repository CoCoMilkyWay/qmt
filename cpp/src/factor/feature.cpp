#include "factor/feature.hpp"

namespace factor {

const std::array<FeatureMeta, static_cast<std::size_t>(F::COUNT)> FEATURES = {{
    // ---- filter (时序) ----
    {"profit_st",   Kind::Filter, Axis::TimeSeries},
    {"revenue_st",  Kind::Filter, Axis::TimeSeries},
    {"dividend_st", Kind::Filter, Axis::TimeSeries},
    {"trading_st",  Kind::Filter, Axis::TimeSeries},
    {"risk_warn",   Kind::Filter, Axis::TimeSeries},
    {"new_list",    Kind::Filter, Axis::TimeSeries},
    // ---- factor (截面) ----
    {"close",       Kind::Factor, Axis::CrossSection},
    {"mcap",        Kind::Factor, Axis::CrossSection},
    {"fmcap",       Kind::Factor, Axis::CrossSection},
    {"pe_ttm4",     Kind::Factor, Axis::CrossSection},
    {"pb_ttm1",     Kind::Factor, Axis::CrossSection},
    {"ps_ttm4",     Kind::Factor, Axis::CrossSection},
    {"pcf_ttm4",    Kind::Factor, Axis::CrossSection},
    {"roe_ttm4",    Kind::Factor, Axis::CrossSection},
    {"roa_ttm4",    Kind::Factor, Axis::CrossSection},
    {"dy_ttm4",     Kind::Factor, Axis::CrossSection},
    // ---- inter: raw 时序 ----
    {"close_raw",   Kind::Inter,  Axis::TimeSeries},
    {"up_lim",      Kind::Inter,  Axis::TimeSeries},
    {"dn_lim",      Kind::Inter,  Axis::TimeSeries},
    {"susp",        Kind::Inter,  Axis::TimeSeries},
    {"mcap_raw",    Kind::Inter,  Axis::TimeSeries},
    {"fmcap_raw",   Kind::Inter,  Axis::TimeSeries},
    {"share_raw",   Kind::Inter,  Axis::TimeSeries},
    {"pe_raw",      Kind::Inter,  Axis::TimeSeries},
    {"pb_raw",      Kind::Inter,  Axis::TimeSeries},
    {"ps_raw",      Kind::Inter,  Axis::TimeSeries},
    {"dy_raw",      Kind::Inter,  Axis::TimeSeries},
    {"pcf_raw",     Kind::Inter,  Axis::TimeSeries},
    {"roe_raw",     Kind::Inter,  Axis::TimeSeries},
    {"roa_raw",     Kind::Inter,  Axis::TimeSeries},
    {"rev_raw",     Kind::Inter,  Axis::TimeSeries},
    {"ni_raw",      Kind::Inter,  Axis::TimeSeries},
    // ---- inter: asset 静态 ----
    {"mb",          Kind::Inter,  Axis::TimeSeries},
    {"list_age",    Kind::Inter,  Axis::TimeSeries},
    // ---- inter: 时序 衍生 ----
    {"low_p",       Kind::Inter,  Axis::TimeSeries},
    {"low_mc",      Kind::Inter,  Axis::TimeSeries},
    {"limit_up",    Kind::Inter,  Axis::TimeSeries},
    {"limit_dn",    Kind::Inter,  Axis::TimeSeries},
    // ---- inter: pool ----
    {"pool_b",      Kind::Inter,  Axis::TimeSeries},
    {"pool",        Kind::Inter,  Axis::CrossSection},
}};

} // namespace factor
