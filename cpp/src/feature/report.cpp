#include "feature/report.hpp"

#include "feature/registry.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace feature {

namespace {

// 运行期 DFS 后序 (= 拓扑序); 只用于本文件的分组展示, 不参与实际计算图构建
// (计算图已在 feature/registry.hpp 编译期 consteval 构建完毕).
std::vector<const FeatureSpec *>
reachable_order(const std::vector<const FeatureSpec *> &roots) {
  std::vector<const FeatureSpec *> order;
  std::unordered_set<const FeatureSpec *> seen;
  std::function<void(const FeatureSpec *)> visit = [&](const FeatureSpec *n) {
    if (!seen.insert(n).second)
      return;
    for (const FeatureSpec *d : n->deps)
      visit(d);
    order.push_back(n);
  };
  for (const FeatureSpec *r : roots)
    visit(r);
  return order;
}

// 按 Kind 分桶展示 (inter → filter → factor), 桶内保持传入的拓扑相对顺序.
std::vector<const FeatureSpec *>
by_kind_then_topo(const std::vector<const FeatureSpec *> &topo) {
  std::vector<const FeatureSpec *> out;
  for (Kind k : {Kind::Inter, Kind::Filter, Kind::Factor})
    for (const FeatureSpec *s : topo)
      if (s->kind == k)
        out.push_back(s);
  return out;
}

const char *kind_name(Kind k) {
  switch (k) {
  case Kind::Filter:
    return "filter";
  case Kind::Factor:
    return "factor";
  case Kind::Inter:
    return "inter";
  }
  return "?";
}

const char *axis_name(Axis a) { return a == Axis::TimeSeries ? "时序" : "截面"; }

std::string deps_str(const FeatureSpec *s) {
  if (s->deps.empty())
    return "—";
  std::string out;
  for (std::size_t i = 0; i < s->deps.size(); ++i) {
    if (i)
      out += ", ";
    out += s->deps[i]->name;
  }
  return out;
}

// UTF-8 解码单个 codepoint (从字节偏移 i 处), clen 输出该字符占用字节数.
std::uint32_t utf8_decode_at(const std::string &s, std::size_t i,
                             std::size_t &clen) {
  unsigned char c = static_cast<unsigned char>(s[i]);
  auto byte = [&](std::size_t k) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(s[i + k])) &
           0x3Fu;
  };
  if ((c & 0x80) == 0) {
    clen = 1;
    return c;
  }
  if ((c & 0xE0) == 0xC0) {
    clen = 2;
    return ((c & 0x1Fu) << 6) | byte(1);
  }
  if ((c & 0xF0) == 0xE0) {
    clen = 3;
    return ((c & 0x0Fu) << 12) | (byte(1) << 6) | byte(2);
  }
  clen = 4;
  return ((c & 0x07u) << 18) | (byte(1) << 12) | (byte(2) << 6) | byte(3);
}

// East Asian Wide/Fullwidth 判定 (简化版 wcwidth): 中文/日文/韩文/全角符号占 2
// 个终端格, 其余 (ASCII, 数学符号 ∧∨≥≤×÷Σ·—…→ 等) 占 1 格. 表格对齐依赖这个
// 判定, 不能按字节或按字符数简单处理 (中文字符数少但视觉更宽).
bool is_wide(std::uint32_t cp) {
  return (cp >= 0x1100 && cp <= 0x115F) ||
        (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3041 && cp <= 0x33FF) ||
        (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0xA000 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE4F) ||
        (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x20000 && cp <= 0x3FFFD);
}

struct Cp {
  std::size_t off;
  std::size_t len;
  int w;
};

std::vector<Cp> decode(const std::string &s) {
  std::vector<Cp> out;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t clen;
    std::uint32_t cp = utf8_decode_at(s, i, clen);
    out.push_back({i, clen, is_wide(cp) ? 2 : 1});
    i += clen;
  }
  return out;
}

// 定宽 (按终端显示格数, 非字节/字符数): 不够补空格, 超出截断并在末尾加 "…"
// (窄字符, 占 1 格) 使总显示宽度精确等于 width, 保证各行同列对齐.
std::string fit(const std::string &s, std::size_t width) {
  std::vector<Cp> cps = decode(s);
  std::size_t total = 0;
  for (const Cp &c : cps)
    total += static_cast<std::size_t>(c.w);
  if (total <= width)
    return s + std::string(width - total, ' ');

  std::size_t budget = width > 0 ? width - 1 : 0; // 留 1 格给省略号
  std::size_t acc = 0, cut = 0;
  for (const Cp &c : cps) {
    if (acc + static_cast<std::size_t>(c.w) > budget)
      break;
    acc += static_cast<std::size_t>(c.w);
    cut = c.off + c.len;
  }
  std::string out = s.substr(0, cut) + "…";
  acc += 1;
  if (acc < width)
    out += std::string(width - acc, ' ');
  return out;
}

constexpr std::size_t W_KIND = 6;
constexpr std::size_t W_FEATURE = 14;
constexpr std::size_t W_AXIS = 4;
constexpr std::size_t W_DEPS = 26;
constexpr std::size_t W_FORMULA = 60;
constexpr std::size_t W_ASSUMPTION = 60;

void print_row(const std::string &kind, const std::string &feature,
               const std::string &axis, const std::string &deps,
               const std::string &formula, const std::string &assumption) {
  std::cout << fit(kind, W_KIND) << " | " << fit(feature, W_FEATURE) << " | "
            << fit(axis, W_AXIS) << " | " << fit(deps, W_DEPS) << " | "
            << fit(formula, W_FORMULA) << " | " << fit(assumption, W_ASSUMPTION)
            << "\n";
}

void print_header() {
  print_row("kind", "feature", "轴", "deps", "formula", "assumption");
  std::cout << std::string(W_KIND, '-') << "-+-" << std::string(W_FEATURE, '-')
            << "-+-" << std::string(W_AXIS, '-') << "-+-"
            << std::string(W_DEPS, '-') << "-+-" << std::string(W_FORMULA, '-')
            << "-+-" << std::string(W_ASSUMPTION, '-') << "\n";
}

void print_group(const std::string &title,
                 const std::vector<const FeatureSpec *> &nodes) {
  std::cout << "\n-- " << title << " (" << nodes.size() << ") --\n";
  print_header();
  for (const FeatureSpec *s : nodes) {
    print_row(kind_name(s->kind), s->name, axis_name(s->axis), deps_str(s),
              s->formula, s->assumption);
  }
}

} // namespace

void print_dependency_table() {
  // 框架固定根可达闭包 = "公共" 的基础部分 (与策略配置无关).
  std::vector<const FeatureSpec *> framework_roots(
      registry_detail::FRAMEWORK_ROOTS,
      registry_detail::FRAMEWORK_ROOTS +
          (sizeof(registry_detail::FRAMEWORK_ROOTS) /
           sizeof(registry_detail::FRAMEWORK_ROOTS[0])));
  std::vector<const FeatureSpec *> framework_set =
      reachable_order(framework_roots);
  std::unordered_set<const FeatureSpec *> framework_seen(framework_set.begin(),
                                                         framework_set.end());

  // 每个策略自己的根可达闭包 (filters ∪ weights.f ∪ rank_key).
  std::vector<std::vector<const FeatureSpec *>> per_strategy;
  for (const strategy::StrategySpec *st : strategy::STRATEGIES) {
    std::vector<const FeatureSpec *> roots;
    for (const FeatureSpec *f : st->filters)
      roots.push_back(f);
    for (const auto &fw : st->weights)
      roots.push_back(fw.f);
    roots.push_back(st->pool.rank_key);
    per_strategy.push_back(reachable_order(roots));
  }

  // common_extra = 被全部策略共同引用、且不在框架固定根闭包内的节点
  //   (单策略场景下退化为 = 该策略整个闭包; 加第二个策略后自动收窄, 无需改代码).
  std::vector<const FeatureSpec *> common_extra;
  if (!per_strategy.empty()) {
    for (const FeatureSpec *cand : per_strategy[0]) {
      if (framework_seen.count(cand))
        continue;
      bool in_all = true;
      for (std::size_t i = 1; i < per_strategy.size(); ++i) {
        if (std::find(per_strategy[i].begin(), per_strategy[i].end(), cand) ==
            per_strategy[i].end()) {
          in_all = false;
          break;
        }
      }
      if (in_all)
        common_extra.push_back(cand);
    }
  }

  std::unordered_set<const FeatureSpec *> common_set(framework_set.begin(),
                                                     framework_set.end());
  for (const FeatureSpec *f : common_extra)
    common_set.insert(f);

  // 用全局 ALL_NODES 的拓扑序重新过滤, 保证公共组内部也是合法拓扑序.
  std::vector<const FeatureSpec *> common_topo;
  for (const FeatureSpec *n : ALL_NODES)
    if (common_set.count(n))
      common_topo.push_back(n);

  std::cout << "\n================ 特征依赖表 (formula / assumption 见各节点 "
               "FeatureSpec 定义) ================\n";
  print_group("公共 — 框架固定根 + 全部策略共用", by_kind_then_topo(common_topo));

  for (std::size_t i = 0; i < strategy::STRATEGIES.size(); ++i) {
    const strategy::StrategySpec *st = strategy::STRATEGIES[i];
    std::vector<const FeatureSpec *> specific;
    for (const FeatureSpec *n : ALL_NODES) {
      if (common_set.count(n))
        continue;
      if (std::find(per_strategy[i].begin(), per_strategy[i].end(), n) !=
          per_strategy[i].end())
        specific.push_back(n);
    }
    print_group(std::string(st->name) + " 专属", by_kind_then_topo(specific));
  }
  std::cout << "=====================================================================\n";
}

} // namespace feature
