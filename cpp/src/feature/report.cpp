#include "feature/report.hpp"

#include "feature/def/all.hpp"
#include "feature/registry.hpp"
#include "strategy/registry.hpp"

#include <cassert>
#include <clocale>
#include <cwchar>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace feature {

namespace {

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

// 逗号拼接一组名字 (供策略专属 filter/factor 概览行用); 空则显示 "—".
template <class Items, class NameOf>
std::string join_names(const Items &items, NameOf name_of) {
  if (items.empty())
    return "—";
  std::string out;
  bool first = true;
  for (const auto &it : items) {
    if (!first)
      out += ", ";
    first = false;
    out += name_of(it);
  }
  return out;
}

// 按字节偏移切分 UTF-8 字符, 每个字符的终端显示宽度用 libc 的 wcwidth 判定
// (中文/日文/韩文/全角符号占 2 格, 其余占 1 格), 无需手写宽字符区间表.
struct Cp {
  std::size_t off;
  std::size_t len;
  int w;
};

std::vector<Cp> decode(const std::string &s) {
  static const bool locale_ok =
      std::setlocale(LC_CTYPE, "C.UTF-8") != nullptr;
  assert(locale_ok && "需要 C.UTF-8 locale 支持才能计算终端宽度");

  std::vector<Cp> out;
  std::mbstate_t state{};
  std::size_t i = 0;
  while (i < s.size()) {
    wchar_t wc;
    std::size_t len = std::mbrtowc(&wc, s.data() + i, s.size() - i, &state);
    assert(len != static_cast<std::size_t>(-1) &&
           len != static_cast<std::size_t>(-2) && "非法 UTF-8 序列");
    if (len == 0)
      len = 1; // 嵌入的 '\0'
    int w = wcwidth(wc);
    out.push_back({i, len, w > 0 ? w : 1});
    i += len;
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
constexpr std::size_t W_ACTIVE = 6;
constexpr std::size_t W_DEPS = 78;
constexpr std::size_t W_FORMULA = 60;
constexpr std::size_t W_ASSUMPTION = 60;

const char *active_str(bool active) { return active ? "✓" : "—"; }

void print_row(const std::string &kind, const std::string &feature,
               const std::string &axis, const std::string &active,
               const std::string &deps, const std::string &formula,
               const std::string &assumption) {
  std::cout << fit(kind, W_KIND) << " | " << fit(feature, W_FEATURE) << " | "
            << fit(axis, W_AXIS) << " | " << fit(active, W_ACTIVE) << " | "
            << fit(deps, W_DEPS) << " | " << fit(formula, W_FORMULA) << " | "
            << fit(assumption, W_ASSUMPTION) << "\n";
}

void print_header() {
  print_row("kind", "feature", "轴", "active", "deps", "formula",
            "assumption");
  std::cout << std::string(W_KIND, '-') << "-+-" << std::string(W_FEATURE, '-')
            << "-+-" << std::string(W_AXIS, '-') << "-+-"
            << std::string(W_ACTIVE, '-') << "-+-" << std::string(W_DEPS, '-')
            << "-+-" << std::string(W_FORMULA, '-') << "-+-"
            << std::string(W_ASSUMPTION, '-') << "\n";
}

// active_set: 判定节点是否在真正的计算图 (feature::ALL_NODES) 内; 仅供展示,
// 不影响 active_set 本身的裁剪逻辑 (那部分完全由 feature/registry.hpp 决定).
void print_group(const std::string &title,
                 const std::vector<const FeatureSpec *> &nodes,
                 const std::unordered_set<const FeatureSpec *> &active_set) {
  std::cout << "\n-- " << title << " (" << nodes.size() << ") --\n";
  print_header();
  for (const FeatureSpec *s : nodes) {
    print_row(kind_name(s->kind), s->name, axis_name(s->axis),
              active_str(active_set.count(s) != 0), deps_str(s), s->formula,
              s->assumption);
  }
}

} // namespace

void print_dependency_table() {
  // 全部已定义特征 (含未被任何策略引用的, 见 feature/def/all.hpp); active
  // 标记来自真正参与计算的 feature::ALL_NODES (feature/registry.hpp 裁剪结果).
  std::vector<const FeatureSpec *> all_defined(
      registry_detail::ALL_DEFINED_NODES.begin(),
      registry_detail::ALL_DEFINED_NODES.end());
  std::unordered_set<const FeatureSpec *> active_set(ALL_NODES.begin(),
                                                     ALL_NODES.end());

  std::cout << "\n================ 特征依赖表 (formula / assumption 见各节点 "
               "FeatureSpec 定义) ================\n";
  print_group("全部特征 (含未激活)", by_kind_then_topo(all_defined),
              active_set);

  std::cout << "\n-- 策略使用情况 --\n";
  for (const strategy::StrategySpec *st : strategy::STRATEGIES) {
    std::cout << st->name << " filter: "
              << join_names(st->filters,
                            [](const FeatureSpec *f) { return f->name; })
              << "\n";
    std::cout << st->name << " factor: "
              << join_names(st->weights,
                            [](const strategy::FactorWeight &w) {
                              return w.f->name;
                            })
              << "\n";
  }
  std::cout << "=====================================================================\n";
}

} // namespace feature
