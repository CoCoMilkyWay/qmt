#include "api/tushare/spec.hpp"
#include "config.hpp"
#include "misc/date.hpp"
#include "api/tushare/http.hpp"

#include <algorithm>
#include <chrono>

namespace tushare {

using std::chrono::days;
using std::chrono::sys_days;

// ============================================================================
// 通用 fetch：task.params 已是完整 query
// ============================================================================

yyjson_doc *FetchStrategy::fetch(Http &http, const FetchTask &task,
                                 const InterfaceSpec &spec) const {
  return http.call(spec.api, task.params);
}

// ============================================================================
// RangeStrategy
// ============================================================================

namespace {

// 把 missing (按字典序升序) 切成连续段，每段长度 ≤ max_days
std::vector<std::pair<std::string, std::string>>
split_segments(const std::vector<std::string> &missing, int max_days) {
  std::vector<std::pair<std::string, std::string>> segments;
  if (missing.empty())
    return segments;

  sys_days seg_start = misc::parse_yyyymmdd(missing[0]);
  sys_days seg_prev = seg_start;
  auto flush = [&](sys_days end) {
    sys_days cur = seg_start;
    while (cur <= end) {
      sys_days block_end = std::min(cur + days{max_days - 1}, end);
      segments.emplace_back(misc::fmt_yyyymmdd(cur),
                            misc::fmt_yyyymmdd(block_end));
      cur = block_end + days{1};
    }
  };
  for (size_t i = 1; i < missing.size(); i++) {
    sys_days cur = misc::parse_yyyymmdd(missing[i]);
    if (cur == seg_prev + days{1}) {
      seg_prev = cur;
    } else {
      flush(seg_prev);
      seg_start = cur;
      seg_prev = cur;
    }
  }
  flush(seg_prev);
  return segments;
}

} // namespace

std::vector<FetchTask>
RangeStrategy::plan(const std::vector<std::string> &missing) const {
  auto segments = split_segments(missing, max_days_);

  std::vector<FetchTask> tasks;
  size_t n_variants = variant_values_.empty() ? 1 : variant_values_.size();
  tasks.reserve(segments.size() * n_variants);

  for (auto &[s, e] : segments) {
    if (variant_values_.empty()) {
      tasks.push_back({s, e, {{"start_date", s}, {"end_date", e}}});
    } else {
      for (auto &v : variant_values_) {
        tasks.push_back(
            {s, e, {{"start_date", s}, {"end_date", e}, {variant_key_, v}}});
      }
    }
  }
  return tasks;
}

// ============================================================================
// PerDayStrategy
// ============================================================================

std::vector<FetchTask>
PerDayStrategy::plan(const std::vector<std::string> &missing) const {
  std::vector<FetchTask> tasks;
  tasks.reserve(missing.size() * day_params_.size());
  for (auto &d : missing) {
    for (auto &p : day_params_) {
      tasks.push_back({d, d, {{p, d}}});
    }
  }
  return tasks;
}

// ============================================================================
// SPECS
// ============================================================================

const std::vector<InterfaceSpec> SPECS = {
    {"forecast",
     "forecast_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    {"express",
     "express_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    // 财报披露 (api=disclosure_date) 拆成两个 itf, 各自时序自洽 (无未来信息):
    //
    //   disclosure: 披露计划公告 (visible=ann_date)
    //     - query=ann_date=Y, 抓当天发布或修订的披露计划
    //     - drop actual_date/modify_date (后续回填 = 未来信息)
    //     - 留 pre_date (ann_date 当天声明的计划日, 非回填)
    //     - 用途: 提前感知"哪些股票计划在某日披露" (不用于状态机终止)
    //     - 历史 null ann_date 记录 (~1.2%, 2015-2020) 自动落入 report
    //
    //   report: 实际披露事件 (visible=actual_date)
    //     - query=actual_date=Y, 仅返回 actual_date 已填的记录 (= 实际已披露)
    //     - 所有字段 (ann_date/pre_date) 都是 actual_date 当下及之前的历史信息
    //     - 用途: 状态机终止信号 (forecast 系列 ST 终止于 report.actual_date)
    {"disclosure",
     "disclosure_date",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date"}),
     {"actual_date", "modify_date"}},
    {"report",
     "disclosure_date",
     {"actual_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"actual_date"})},
    // ST 风险警示 (旧, 事件型): visible_date=imp_date (状态生效日, 盘前已知)
    // PK=(ts_code, pub_date, imp_date, st_tpye): 同 imp_date 可能多种类型 or 多次修正
    // 注: 已不再入张量 (pit.cpp 无 itf_st 块); 保留 spec 以维持 data/**/st.json 落地连续
    //     (历史归档用; risk_warn 改读 stock_st 每日快照, 数据起点不漏标)
    {"st",
     "st",
     {"imp_date"},
     {"ts_code", "pub_date", "imp_date", "st_tpye"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"imp_date"})},
    // ST 股票列表 (新, 每日快照): visible_date=trade_date (盘前 9:20 入库)
    // - 每个交易日返回当日全部 ST 名单 (~150 行, 含 *ST), 数据起点 20160101
    // - 相比旧 st 事件流, 优点是「开始不漏」: 首日即获完整存量
    // - PK=(ts_code, trade_date): 单日单股唯一
    // - 用途: 入张量喂 risk_warn (name 是否含 '*' 区分 ST=1 / *ST=2)
    {"stock_st",
     "stock_st",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 交易日历：每天每交易所仅 1 行，按 10 年/段切；变体笛卡尔积全部 7 个交易所
    // 文档支持枚举: SSE 上交所 / SZSE 深交所 / CFFEX 中金所 / SHFE 上期所 /
    //              CZCE 郑商所 / DCE 大商所 / INE 上能源
    // 北交所不在 trade_cal 输出枚举内, 实测 BSE/BJSE/BJEX/BJ/NEEQ 全部返回 items=[];
    // A 股节假日由证监会统一安排, 北交所与 SSE/SZSE 一致, 无需单拉.
    {"calendar",
     "trade_cal",
     {"cal_date"},
     {"exchange", "cal_date"},
     std::make_shared<RangeStrategy>(3650, "exchange", std::vector<std::string>{"SSE", "SZSE", "CFFEX", "SHFE", "CZCE", "DCE", "INE"})},
    // 分红送股：每个 day=Y 双查询 (ann_date=Y) + (imp_ann_date=Y)
    // - ann_date=Y 抓当天预案/决议公告 (visible_date=ann_date=Y)
    // - imp_ann_date=Y 抓当天实施公告 (visible_date=imp_ann_date=Y)
    // visible_date 优先 imp_ann_date 否则 ann_date：实施阶段归实施公告日
    // PK 含 div_proc：同一笔分红的"预案/股东大会通过/实施"为多条记录
    {"dividend",
     "dividend",
     {"imp_ann_date", "ann_date"},
     {"ts_code", "end_date", "div_proc"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date", "imp_ann_date"})},
    // 每日指标：换手率/量比/PE_TTM/PS_TTM/自由流通市值等核心因子
    // 一天全市场 ~5000 行，限额 6000；盘后 15-17 点入库
    {"daily_basic",
     "daily_basic",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 复权因子：分红/送股事件后才变化，但每个交易日每只股都有一条
    // 一天全市场 ~5000 行；盘前 9:15-9:20 入库
    {"adj_factor",
     "adj_factor",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 每日涨跌停价：盘前 8:40 入库；一天全市场 ~5000 行，限额 5800
    {"stk_limit",
     "stk_limit",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 每日停复牌：稀疏数据 (一天数十条)，按月切段批量拉
    // PK 含 suspend_type：同一只股一天内可能同时有 S(停)+R(复) 两条
    {"suspend_d",
     "suspend_d",
     {"trade_date"},
     {"ts_code", "trade_date", "suspend_type"},
     std::make_shared<RangeStrategy>(::config::FETCH_MAX_DAYS_PER_CALL)},
    // 财务指标 (vip)：按 ann_date 单日拉，不指定 period
    // - 单日返回当天所有公告记录 (跨多个 end_date 自动覆盖)
    // - visible_date=ann_date：财报对外可见即落库
    // - PK=(ts_code, end_date)：同期修正版本以最后一条为准 (响应顺序内)
    // - 跨天修正 (>lookback) 留旧版本在历史 day file，行为与 forecast/express 一致
    {"fina_indicator",
     "fina_indicator_vip",
     {"ann_date"},
     {"ts_code", "end_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"ann_date"})},
    // 利润表 (vip)：start_date/end_date 文档语义=公告日范围 (= 我们的 visible_date)，可用 Range
    // - 普通 income 接口必传 ts_code 不能扫全市场, 只能用 income_vip
    // - PK 含 report_type：同 (ts_code, end_date) 可能并存合并/单季/调整等多版本
    //   (1合并/2单季/3调整单季/4调整合并/5调整前/6母公司/7母单季/...)
    // - fields=""：默认列已覆盖 revenue / n_income_attr_p (rev_raw / ni_raw 用)
    // - max_days=7：4月底/8月底/10月底披露高峰单日 ~1000-2000 行 × 多 report_type，
    //   按周窗口估算峰值 < 8000 行；比 forecast/express 的 31 天更保守 (财报记录密度更高)
    {"income",
     "income_vip",
     {"ann_date"},
     {"ts_code", "end_date", "report_type"},
     std::make_shared<RangeStrategy>(7)},
    // 现金流量表 (vip)：同 income (start_date/end_date = 公告日范围)
    // - PK 含 report_type 同理
    // - 默认列已覆盖 n_cashflow_act (pcf_raw 用)
    {"cashflow",
     "cashflow_vip",
     {"ann_date"},
     {"ts_code", "end_date", "report_type"},
     std::make_shared<RangeStrategy>(7)},
    // 融资融券标的: 当日两融名单 (含 ETF). visible_date=trade_date, 盘前更新.
    // - 一天全市场 ~3000-5000 行, 限额 6000 行/单次, PerDay 单 query 即可.
    // - PK=(ts_code, trade_date): 同 trade_date 同 ts_code 唯一.
    // - 用途: 后续策略可加"仅两融"过滤 (当前默认不过滤, 保持与 py 一致).
    {"margin_secs",
     "margin_secs",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
    // 融资融券明细: 每日两融余额/买卖量. visible_date=trade_date, "每日9点更新" (盘前).
    // - tushare 9点入库的是 T-1 日明细 (trade_date=T-1), T 日盘中可见 (cutoff=0 自洽).
    // - 一天全市场 ~1000-3000 行, 限额 6000 行/单次, PerDay 单 query 即可.
    // - PK=(ts_code, trade_date).
    // - 用途: 后续可派生融资余额/融券余额因子 (当前不入张量, 仅落地).
    {"margin_detail",
     "margin_detail",
     {"trade_date"},
     {"ts_code", "trade_date"},
     std::make_shared<PerDayStrategy>(std::vector<std::string>{"trade_date"})},
};

} // namespace tushare
