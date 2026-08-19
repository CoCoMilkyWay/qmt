#pragma once

#include "misc/mmap.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace feature {

struct Axes;    // fwd decl
struct PitPool; // fwd decl

// ============================================================================
// PIT 中间结构 (Phase 1 写, Phase 2 只读).
//
// 划分:
//   网格 itf (1 record / 交易日 / asset): dense 存. 每字段独立 PoolArr<T>,
//                                          长度 = n_a()*n_d() (a-major, d-minor);
//                                          缺席 = NaN (float) / 0 (uint8/int8);
//                                          与 Tensor::ts_row 同 layout → Phase 2
//                                          零 copy 取 span.
//   事件 itf (per A 时间线):              EventStore<Ev> = (offsets [n_a+1],
//                                          events arena). 同 a 内按 v 升序;
//                                          v = visible_d_idx 经 CUTOFF 调整后的 row D.
//
// 全部字段都是 POD ⇒ cache 文件 (data/pool/<itf>.bin) = 字段 raw blob 的紧凑拼接.
// hit 路径 mmap(MAP_PRIVATE) → PoolArr.map_view 把 data 指针指过去 → 零 copy 零
// 反序列化; 后续 overlay / ffill 写 PoolArr 由 OS 自动 COW (不脏文件).
//
// 数据源 (data/YYYY-MM/<itf>.parquet 月度分片):
//   全部 BigQuant 表入库时间盘后 17:00 后, PIT 严格 = -1. 项目按业务可推出性分两类
//   模式 (详见 README §cutoff):
//     normal (CUTOFF=-1, 承认滞后): row D=T 取 T-1 数据.
//     hybrid (CUTOFF=0,  伪装盘前): 历史按 row=v_idx (假装盘前可见);
//                                   最后一天 (= 实盘当日) 由 apply_meta_overlays
//                                   用 cn_stock_static_data (真盘前 09:00) 填 row=last_d.
//
//   网格:
//     bar1d                 ← cn_stock_real_bar1d    CUTOFF=-1
//     shares                ← cn_stock_shares        CUTOFF=-1
//     limit_price           ← cn_stock_limit_price   CUTOFF=-1
//     status                ← cn_stock_status        CUTOFF=0  (hybrid, overlay)
//     margin_detail         ← cn_stock_margin_trading_detail  CUTOFF=0
//   meta overlay (apply_meta_overlays):
//     cn_stock_static_data (_meta) → status.{suspended, st_status} row=last_d
//   事件:
//     industry_component / industry_change / dividend / financial_ttm /
//     financial_balance / financial_income_annual / forecast (Tushare)
// ============================================================================

// ---------------------------------------------------------------------------
// PoolArr<T>: 双模 POD 视图.
//   - build 路径: allocate(n) → owned vector 持有 n 个 T (= prealloc), 业务可写;
//                 finalize_owned_to_shrink() 可选 (这里不需要).
//   - hit 路径:   map_view(ptr, n) → 指向 mmap 区, owned 释放.
//   接口对外等价于 std::vector<T> 的只读+下标写 (data/size/[]/begin/end).
//
//   注: 对外暴露 data_ / size_ 是为了让 CacheVisitor + cache_layout 通用
//   (template 化, 零开销).
// ---------------------------------------------------------------------------
template <class T>
class PoolArr {
public:
  PoolArr() = default;
  PoolArr(const PoolArr &) = delete;
  PoolArr &operator=(const PoolArr &) = delete;
  PoolArr(PoolArr &&) = default;
  PoolArr &operator=(PoolArr &&) = default;

  T *data() { return data_; }
  const T *data() const { return data_; }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  T &operator[](std::size_t i) { return data_[i]; }
  const T &operator[](std::size_t i) const { return data_[i]; }
  T *begin() { return data_; }
  T *end() { return data_ + size_; }
  const T *begin() const { return data_; }
  const T *end() const { return data_ + size_; }

  // build / miss 路径: 自己分配 n 个 T 并 value-initialize (POD 类型 → 0/NaN
  // 由调用方 fill 决定).
  void allocate(std::size_t n) {
    owned_.assign(n, T{});
    data_ = owned_.data();
    size_ = n;
  }

  // hit 路径: 指向 mmap 区 (data 写入由 OS COW 处理). 不接管所有权.
  void map_view(T *p, std::size_t n) {
    owned_.clear();
    owned_.shrink_to_fit();
    data_ = p;
    size_ = n;
  }

private:
  T *data_ = nullptr;
  std::size_t size_ = 0;
  std::vector<T> owned_; // 仅 build 路径填充; map_view 后空
};

// ---------------------------------------------------------------------------
// EventStore<Ev>: per-A 事件链, 物理布局 (offsets[n_a+1], events arena).
//   build 时: resize_chains(n_a) → 各线程 push_chain(a, ev) (per-a 互斥) →
//             sort_chains() (per-a stable_sort by v) → finalize() 压平.
//   hit 时:   cache_layout 直接 visit offsets / events 两个 PoolArr.
//   读时:     pool.<itf>[a] 返回 std::span<Ev> (range-for / size / [] 全支持).
// ---------------------------------------------------------------------------
template <class Ev>
class EventStore {
public:
  EventStore() = default;
  EventStore(const EventStore &) = delete;
  EventStore &operator=(const EventStore &) = delete;
  EventStore(EventStore &&) = default;
  EventStore &operator=(EventStore &&) = default;

  // ---- build (miss) 路径 ----
  void resize_chains(std::size_t n_a) {
    chains_.assign(n_a, {});
  }
  void push_chain(int a, const Ev &e) {
    chains_[static_cast<std::size_t>(a)].push_back(e);
  }
  void sort_chains() {
    // stable_sort 保 emplace 顺序: 同 v (visible 经 cutoff 落同 row) 时不引入
    // 算法层新不确定 (emplace 顺序本身受 phase 1 调度影响, 彻底 deterministic
    // 需在 Ev 加 tie-breaker, 不做).
    for (auto &c : chains_) {
      std::stable_sort(c.begin(), c.end(),
                       [](const Ev &x, const Ev &y) { return x.v < y.v; });
    }
  }
  // 压平 chains_ 到 owned offsets/events arena. 调用后 chains_ 释放.
  void finalize() {
    std::size_t n_a = chains_.size();
    offsets_.allocate(n_a + 1);
    std::size_t total = 0;
    offsets_[0] = 0;
    for (std::size_t a = 0; a < n_a; ++a) {
      total += chains_[a].size();
      offsets_[a + 1] = static_cast<std::uint32_t>(total);
    }
    events_.allocate(total);
    for (std::size_t a = 0; a < n_a; ++a) {
      std::memcpy(events_.data() + offsets_[a], chains_[a].data(),
                  chains_[a].size() * sizeof(Ev));
    }
    std::vector<std::vector<Ev>>().swap(chains_);
  }

  // ---- 通用 ----
  PoolArr<std::uint32_t> &offsets() { return offsets_; }
  PoolArr<Ev> &events() { return events_; }

  std::size_t n_a() const { return offsets_.size() ? offsets_.size() - 1 : 0; }

  std::span<const Ev> operator[](int a) const {
    std::uint32_t lo = offsets_[a];
    std::uint32_t hi = offsets_[a + 1];
    return std::span<const Ev>(events_.data() + lo, hi - lo);
  }
  std::span<Ev> operator[](int a) {
    std::uint32_t lo = offsets_[a];
    std::uint32_t hi = offsets_[a + 1];
    return std::span<Ev>(events_.data() + lo, hi - lo);
  }

private:
  PoolArr<std::uint32_t> offsets_;
  PoolArr<Ev> events_;
  std::vector<std::vector<Ev>> chains_; // 仅 build 路径用, finalize 后清空
};

// ========== 网格 ==========

// cn_stock_real_bar1d (CUTOFF=-1): 不复权 OHLCV + 后复权乘子.
//   close            不复权 [元/股] (实际市场成交价, 除权日跳跃)
//   adjust_factor    BigQuant 后复权累积乘子 (close_hfq[d] = close[d] × adjust_factor[d])
//   PitPool 暴露 close + adjust_factor; tensor 顶层仅 close_raw (= close 真价).
//   连续性 feature (daily_return) 内部叠 adjust_factor 算 hfq 链式 (= 含分红再投入).
struct GridBar1d {
  PoolArr<float> close;
  PoolArr<float> adjust_factor;
};

// cn_stock_shares (CUTOFF=-1): 各类股本 [股].
struct GridShares {
  PoolArr<float> total_shares;
  PoolArr<float> a_float_shares;
};

// cn_stock_limit_price (CUTOFF=-1, normal): 当日适用涨跌停价 [元/股].
//   实际 BigQuant 入库 17:00 → 承认滞后, row D=T 取 T-1 的 limit.
struct GridLimitPrice {
  PoolArr<float> upper_limit;
  PoolArr<float> lower_limit;
};

// cn_stock_status (CUTOFF=0, hybrid 伪装):
//   st_status         int8 4 态: 0=正常 / 1=ST / 2=*ST / 3=退市整理期
//   suspended         uint8: 0/1 (1=当日停牌)
//   实际盘后入库, 业务上 ST/停牌当日盘前即生效 → 假装盘前; 最后一天由 overlay
//   用 cn_stock_static_data (真盘前 09:00) 填 row=last_d.
struct GridStatus {
  PoolArr<std::int8_t> st_status;
  PoolArr<std::uint8_t> suspended;
};

// cn_stock_margin_trading_detail (CUTOFF=0): 当日两融明细.
//   is_margin                  uint8: 派生 — (D, A) 当日是否存在记录 (1=两融标的)
//   financing_balance          融资余额 [元]
//   securities_lending_balance 融券余额 [元]
struct GridMarginDetail {
  PoolArr<std::uint8_t> is_margin;
  PoolArr<float> financing_balance;
  PoolArr<float> securities_lending_balance;
};

// ========== 事件 (POD only) ==========

// cn_stock_industry_component WHERE industry='sw2021' (CUTOFF=-1, MonthFirst):
//   每月初一份 sw2021 一级行业归属快照. l1_id = sw2021_l1_name_to_id(... ); 不在表 → 0.
struct IndustryComponentEv {
  std::int32_t v; // row D (32-bit 对齐 + 与 axes int 索引一致)
  std::uint8_t l1_id;
  std::uint8_t _pad[3]{}; // POD 对齐
};

// cn_stock_industry_change (CUTOFF=-1, Day): 月内 sw2021 L1 行业切换事件.
//   change_flag=1 进入新行业一侧.
struct IndustryChangeEv {
  std::int32_t v;
  std::uint8_t l1_id;
  std::uint8_t _pad[3]{};
};

// cn_stock_dividend (CUTOFF=-1, v ← publish_date): 分红事件.
//   report_date / ex_date 为 YYYYMMDD int32 (POD; 0 = 缺失).
//   dy_raw 走 v (预案公告日) trailing 12M 窗口, 用 cash_before_tax (税前, 行业口径)
//   × share_raw[ev.v]; dividend_st 用 cash_after_tax × share_raw[ev.v] 推 3y 累计
//   现金分红 (按 report_date.Y 窗口).
struct DividendEv {
  std::int32_t v;
  std::int32_t report_date;
  std::int32_t ex_date;
  float cash_before_tax;
  float cash_after_tax;
};

// Tushare forecast 类型 enum (业务关心的只有 "首亏" / "续亏"; 其他归 Other).
//   aggregate 时一次性 map string → enum, replay/feature 不再碰 string.
enum class ForecastType : std::uint8_t {
  Other = 0,
  FirstLoss = 1,    // 首亏
  ContinueLoss = 2, // 续亏
};

// Tushare forecast (CUTOFF=-1): 业绩预告.
//   profit_st / revenue_st 状态机触发源.
struct ForecastEv {
  std::int32_t v;
  std::int32_t end_date;
  ForecastType type;
  std::uint8_t _pad[3]{};
  float last_parent_net;
};

// cn_stock_financial_ttm_shift: 财务 TTM (shift=0 为主记录;
//   同 (date, instrument) 的 shift=4 记录 [4 个季度 = 12 个月前的同口径 TTM]
//   在 build 时配对写入 net_cffoa_ttm_shift4, 供 cffoa_raw 算同比增量用).
//   per-A 沿 v 升序取 latest event 即可 (max v 自然取新).
struct FinancialTtmEv {
  std::int32_t v;
  std::int32_t report_date;
  float total_operating_revenue_ttm;
  float net_profit_to_parent_shareholders_ttm;
  float net_profit_ttm; // 含少数股东损益; roa_raw 分子 (配含少数的 total_assets)
  float net_cffoa_ttm;
  float net_cffoa_ttm_shift4; // shift=4 (4 季度前) 同口径 net_cffoa_ttm; 缺失 → NaN
};

// cn_stock_financial_balance_general_pit: 资产负债表 PIT (MRQ snapshot).
//   同 visible_date 多 report_date (历史 + 修正), feature 层走 max report_date.
struct FinancialBalanceEv {
  std::int32_t v;
  std::int32_t report_date;
  float total_owner_equity;
  float total_equity_to_parent_shareholders;
  float total_assets;
};

// cn_stock_financial_income_general_pit: 利润表 PIT.
//   aggregate 过滤 fs_quarter_index!=4 (只入年报); 给 ni_raw 用 (dividend_st 阈值).
struct FinancialIncomeAnnualEv {
  std::int32_t v;
  std::int32_t report_date;
  float net_profit_to_parent_shareholders;
};

// ============================================================================
// PitPool — 一份 Phase 1 → 2 的桥梁结构. 所有字段都是 PoolArr<POD> 或
// EventStore<POD Ev> ⇒ 可整 dump / 整 mmap.
// ============================================================================
struct PitPool {
  GridBar1d bar1d;
  GridShares shares;
  GridLimitPrice limit_price;
  GridStatus status;
  GridMarginDetail margin_detail;

  EventStore<IndustryComponentEv> industry_component;
  EventStore<IndustryChangeEv> industry_change;
  EventStore<DividendEv> dividend;
  EventStore<FinancialTtmEv> financial_ttm;
  EventStore<FinancialBalanceEv> financial_balance;
  EventStore<FinancialIncomeAnnualEv> financial_income_annual;
  EventStore<ForecastEv> forecast;

  // load.cpp internal: 每个 itf 一个 mmap 句柄 (hit 路径下), 与 ITFS 同序.
  // 声明在最后 ⇒ 析构在所有 PoolArr 之后, 保证 view 指针在 munmap 前仍合法.
  // miss 路径下对应槽为空 (PoolArr 走 owned vector). phase 2 不应访问.
  std::vector<misc::MmapFile> _cache_mmaps;
};

// ============================================================================
// MonthFile: (ym, path) 二元组. itf.build 的输入. load.cpp 枚举给定 itf 的
//   全部月度 parquet (data/YYYY-MM/<itf>.parquet) 升序排列后传入.
// ============================================================================
struct MonthFile {
  std::string ym; // "YYYY-MM"
  std::filesystem::path path;
};

// ============================================================================
// CacheVisitor — 通用 cache 序列化 visitor.
//
// 每个 itf 的 cache_layout(PitPool&, CacheVisitor&) 按固定顺序对每个 PoolArr<T>
// 调 v.section(arr). 同一份 cache_layout 服务 3 个用途 (kind 切换):
//   Size  — 累计字段总字节 (dump 前算 file 总长)
//   Write — 把每段 raw bytes 顺序 append 到 out, 同时记 (offset, bytes) 入 table
//   Map   — 把 PoolArr.data 指向 mmap 区基址 + section offset (零 copy)
//
// section table = [(offset, bytes)] × n_sections. 写在 cache 文件头里, hit 时
// 直接消费. cache_layout 在三种模式下访问顺序必须一致 (由代码即文档保证).
// ============================================================================
struct CacheVisitor {
  enum Kind : std::uint8_t { Size,
                             Write,
                             Map };
  Kind kind;

  // Size 模式: 累计 (含 8 字节 align padding).
  std::size_t total_bytes = 0;

  // Write 模式: out 是文件主体 buffer, sections 累 (offset, bytes).
  std::string *write_out = nullptr;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> *sections = nullptr;
  std::size_t write_align_base = 0; // 文件内 section 区起始 offset (= header end)

  // Map 模式: base = mmap 起点, sections 是 cache 头读到的 table, cursor 累自增.
  const std::uint8_t *map_base = nullptr;
  const std::pair<std::uint64_t, std::uint64_t> *map_sections = nullptr;
  std::size_t cursor = 0;

  template <class T>
  void section(PoolArr<T> &arr) {
    if (kind == Size) {
      // section 起点 8 字节对齐 (T align ≤ 8; mmap 后直接 reinterpret_cast).
      while (total_bytes % 8 != 0)
        ++total_bytes;
      total_bytes += arr.size() * sizeof(T);
    } else if (kind == Write) {
      std::size_t bytes = arr.size() * sizeof(T);
      // 对齐到 8: 相对文件起点的 offset 必须 8 对齐, 而 section 区基址
      // (write_align_base) 本身已 8 对齐 → 只需保证 write_out 内 offset 8 对齐.
      while ((write_out->size() + write_align_base) % 8 != 0)
        write_out->push_back('\0');
      std::uint64_t off = static_cast<std::uint64_t>(write_out->size()) +
                          static_cast<std::uint64_t>(write_align_base);
      write_out->append(reinterpret_cast<const char *>(arr.data()), bytes);
      sections->emplace_back(off, static_cast<std::uint64_t>(bytes));
    } else { // Map
      auto [off, bytes] = map_sections[cursor++];
      assert(bytes % sizeof(T) == 0);
      T *p = reinterpret_cast<T *>(
          const_cast<std::uint8_t *>(map_base) + off);
      arr.map_view(p, bytes / sizeof(T));
    }
  }

  // EventStore: 两个 section (offsets, events).
  template <class Ev>
  void section(EventStore<Ev> &store) {
    section(store.offsets());
    section(store.events());
  }
};

// ============================================================================
// ItfDesc — 单 itf 的描述. pit.cpp 内每 itf 一组 (build + cache_layout
//   + post_ffill?) 集中定义, 末尾挂进 ITFS[]. load.cpp 仅迭代该表.
//
// 端到端单点:
//   build(axes, files, pool):
//     从月度 parquet (arrow 列) 直接写入 pool 字段 (并行 / per-a mutex 由 itf
//     内部决定). 全程串通 prealloc → 读列 → emplace → sort → finalize, 不经
//     任何中间 row 表示. miss 路径调一次. 调完 pool 字段就是"row D 已 cutoff
//     的合法数据".
//   cache_layout(pool, visitor):
//     列出此 itf 在 cache 内的 PoolArr blob (固定顺序). dump / map / size 走同一表.
//   post_ffill(axes, pool):
//     网格 itf per-A forward fill. 不入 cache (每次都跑, 改 ffill 逻辑不用 bump
//     POOL_VERSION). 事件 itf 留 nullptr.
// ============================================================================
struct ItfDesc {
  const char *file_name; // .parquet basename, 也是 log / cache file 用名
  void (*build)(const Axes &, const std::vector<MonthFile> &, PitPool &);
  void (*cache_layout)(PitPool &, CacheVisitor &);
  void (*post_ffill)(const Axes &, PitPool &); // 网格 itf 才有
};

extern const ItfDesc ITFS[];
extern const int ITFS_COUNT;

// ============================================================================
// apply_meta_overlays — Phase 1 末段: 真盘前 _meta 快照填充 row=last_d.
//   唯一 overlay: cn_stock_static_data → status 2 字段 (suspended, st_status).
//
//   语义"填充": status CUTOFF=0 假装盘前, 实盘当日 (last_d) 数据还未入库时
//   row=last_d 是默认 0; static_data 真盘前 09:00 值写进去. 历史天 (T<last_d) 不动.
//
//   _meta 不存在 ⇒ silent noop; axes.n_d()==0 ⇒ noop; instrument 不在 codes ⇒ skip.
// ============================================================================
void apply_meta_overlays(const Axes &axes, PitPool &pool);

} // namespace feature
