#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace feature {

// fwd decl: 避免互相 #include (feature.hpp 是最底层 spec, 不依赖具体存储类型)
struct Axes;
struct PitPool;
struct StockMeta;
struct Tensor;
struct FeatureSpec;

// ============================================================================
// 特征节点无中心枚举: 每个节点是 cpp/include/feature/def/{basic,factor,filter}/
//   <name>.hpp 下的一个 header-only 文件 (文件名 == 节点名), 声明一个 inline
//   constexpr FeatureSpec (见 graph.hpp) 作为自己的唯一身份 (取地址即 ID, inline
//   保证跨 TU 同址). 依赖 = #include 对方头文件 + 在 deps 数组里放对方 spec 的
//   地址 — 绝大多数节点只出现在 3 处: 自己的定义 / 被其他节点依赖处 / 被策略
//   (weights / filters / pool.rank_key) 引用处. 不存在任何中心化清单需要为新
//   节点手动挂载.
//
//   计算图 (哪些节点真正参与计算) 由 feature/registry.hpp 从"根集合"
//   (框架自身少量固定需求 + 全部 strategy::STRATEGIES[] 引用到的节点) 出发,
//   consteval 沿 deps 做可达性 + 拓扑排序反向推导出来; 不在根可达闭包内的节点
//   文件即使存在也不会进入计算 (不触发计算, 不占 Tensor 存储).
//
//   目录约定 (图构建阶段完全不区分, 统一按 FeatureSpec 处理):
//     basic/  框架结算/白名单计算直接依赖的市场微观结构数据 (成交价/涨跌停/
//             停牌/退市龄/两融/行业等), 与具体策略无关, 是 registry.hpp
//             FRAMEWORK_ROOTS 的来源, 因此例外地会被框架代码 (backtest.cpp /
//             strategy/columns.cpp / feature/cs.cpp) 直接 #include 引用.
//     factor/ 排序因子 (Kind::Factor) + 全部中间变量 (Kind::Inter, 估值/财务 raw).
//     filter/ 状态机最终排除位 (Kind::Filter).
// ============================================================================

enum class Kind : uint8_t { Filter,
                            Factor,
                            Inter };
enum class Axis : uint8_t { TimeSeries,
                            CrossSection };

// per-A TS compute: 写自己 ts_row(self_spec, a). 可读 pool / meta / 已写就的
//   T.ts_row(dep_spec, a) (dep_spec 来自 #include 的依赖头文件).
using TsComputeFn = void (*)(int a, const Axes &, const PitPool &,
                             const StockMeta &, Tensor &);

// per-D CS scratch: thread-local, 长度 = n_a 的 3 个 buffer (复用避免反复分配).
//   factor_pipeline 用 a+b; neutral_pipeline 用满 a(残差)+b(log mcap)+c(industry).
struct CsBufs {
  std::span<float> a;
  std::span<float> b;
  std::span<float> c;
};

using CsComputeFn = void (*)(int d, const Axes &, Tensor &, CsBufs &);

// 节点完整声明 FeatureSpec (name/kind/axis/deps/fn/must_be_finite) 见 graph.hpp;
// 可达性 + 拓扑排序 (TS_ORDER/CS_ORDER/ALL_NODES) 见 registry.hpp.

// ============================================================================
// 公用小工具 (供 def/ 节点文件 / ts.cpp / cs.cpp 共享)
// ============================================================================

// -ffast-math 下 std::isfinite/std::isnan UB; 用 IEEE-754 bit-pattern 判定.
//   (bits & 0x7f80_0000) != 0x7f80_0000 ⇔ exp 非全 1 ⇔ 有限值 (非 inf/NaN).
inline bool is_finite(float x) {
  std::uint32_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  return (bits & 0x7f800000u) != 0x7f800000u;
}

// "YYYYMMDD" → int Y / int M; 长度不足返回 0
inline int year_of(const std::string_view yyyymmdd) {
  if (yyyymmdd.size() < 4)
    return 0;
  return (yyyymmdd[0] - '0') * 1000 + (yyyymmdd[1] - '0') * 100 +
         (yyyymmdd[2] - '0') * 10 + (yyyymmdd[3] - '0');
}

inline int month_of(const std::string_view yyyymmdd) {
  if (yyyymmdd.size() < 6)
    return 0;
  return (yyyymmdd[4] - '0') * 10 + (yyyymmdd[5] - '0');
}

// int32 YYYYMMDD 形式 (PitPool event 内部存储).
//   0 / 负数视为缺失, 返回 0.
inline int year_of(std::int32_t yyyymmdd) {
  return yyyymmdd > 0 ? yyyymmdd / 10000 : 0;
}

inline int month_of(std::int32_t yyyymmdd) {
  return yyyymmdd > 0 ? (yyyymmdd / 100) % 100 : 0;
}

} // namespace feature
