#include "feature/report.hpp"

#include "feature/def/all.hpp"
#include "feature/registry.hpp"
#include "strategy/registry.hpp"

#include <algorithm>
#include <cassert>
#include <clocale>
#include <cstddef>
#include <cwchar>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace feature {

namespace {

// 运行期 DFS 拓扑序 (对全部已定义节点; 复用 graph.hpp 的 consteval 版本逻辑,
//   但 ALL_DEFINED 非 consteval 上下文, 这里单独写一份).
std::vector<const FeatureSpec *>
topo_order_all(const std::vector<const FeatureSpec *> &all) {
  std::vector<const FeatureSpec *> done;
  std::vector<const FeatureSpec *> stack;
  std::unordered_set<const FeatureSpec *> visited;
  std::unordered_set<const FeatureSpec *> on_stack;

  auto visit = [&](auto &&self, const FeatureSpec *n) -> void {
    if (visited.count(n))
      return;
    visited.insert(n);
    on_stack.insert(n);
    stack.push_back(n);
    for (const FeatureSpec *d : n->deps) {
      assert(!on_stack.count(d) && "依赖成环");
      self(self, d);
    }
    stack.pop_back();
    on_stack.erase(n);
    done.push_back(n);
  };

  for (const FeatureSpec *s : all)
    visit(visit, s);
  return done;
}

// 英文名字符 bigram 集合 (用于相似度; 排序后供 Jaccard 交集计数).
std::vector<std::string> bigrams(const std::string &s) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i + 1 < s.size(); ++i)
    out.push_back(s.substr(i, 2));
  std::sort(out.begin(), out.end());
  return out;
}

// 共同后缀长度 (字符数). 同后缀 (_raw / _lim / _age / _st 等) 的对仗对
//   靠此项加分聚拢, 弥补 bigram 对后缀奖励不足 (后缀 bigram 占比低被稀释).
std::size_t common_suffix_len(const std::string &a, const std::string &b) {
  std::size_t i = 0, na = a.size(), nb = b.size();
  while (i < na && i < nb && a[na - 1 - i] == b[nb - 1 - i])
    ++i;
  return i;
}

// 共同前缀长度 (字符数). 同前缀 (limit_ / low_ / mr/ms_bal_ 等) 的对仗对
//   靠此项加分聚拢.
std::size_t common_prefix_len(const std::string &a, const std::string &b) {
  std::size_t i = 0, na = a.size(), nb = b.size();
  while (i < na && i < nb && a[i] == b[i])
    ++i;
  return i;
}

// bigram Jaccard 相似度 ∈ [0,1]: 越大代表两个英文名越"对仗" (共享子串越多).
//   dn_lim/up_lim, mcap_raw/fmcap_raw, pe_raw/pb_raw, list_age/delist_age 等对仗对
//   均因共享后缀/中段取得较高分; 无关对 (mcap/susp) 近 0.
//   叠加共同后缀奖励 (common_suffix / max_len): 同后缀族 (_raw/_lim/_age/_st)
//   显著加分聚成 cluster, 弥补 bigram 对长名字后缀奖励稀释; cluster 内部
//   仍由 bigram 决定子排序 (前缀相似者就近).
double name_sim(const std::string &a, const std::string &b) {
  auto ba = bigrams(a);
  auto bb = bigrams(b);
  double jac = 0.0;
  if (!ba.empty() && !bb.empty()) {
    std::size_t i = 0, j = 0, inter = 0;
    while (i < ba.size() && j < bb.size()) {
      if (ba[i] < bb[j])
        ++i;
      else if (ba[i] > bb[j])
        ++j;
      else {
        ++inter;
        ++i;
        ++j;
      }
    }
    jac = static_cast<double>(inter) /
          static_cast<double>(ba.size() + bb.size() - inter);
  }
  std::size_t mx = std::max(a.size(), b.size());
  double suf_bonus = (mx > 0)
                         ? static_cast<double>(common_suffix_len(a, b)) /
                               static_cast<double>(mx)
                         : 0.0;
  double pre_bonus = (mx > 0)
                         ? static_cast<double>(common_prefix_len(a, b)) /
                               static_cast<double>(mx)
                         : 0.0;
  return jac + suf_bonus + pre_bonus;
}

// 相邻行相似度总和 (优化目标: 同桶内对仗的名字就近排列).
double total_sim(const std::vector<const FeatureSpec *> &order) {
  double s = 0.0;
  for (std::size_t i = 1; i < order.size(); ++i)
    s += name_sim(order[i - 1]->name, order[i]->name);
  return s;
}

// 桶内贪心最近邻路径构造 (带拓扑约束): 从某起点出发, 每步从"依赖已全部入链"
//   的可用节点里选与链尾 name_sim 最高的加入. 同后缀/同前缀族因相互相似度
//   高, 会被连续选中形成 cluster; 拓扑约束由"依赖全部入链才可用"保证.
//   多起点 (全部无桶内依赖的节点) 各跑一遍, 取相邻相似度总和最高者.
std::vector<const FeatureSpec *>
greedy_nearest_neighbor(const std::vector<const FeatureSpec *> &bucket) {
  std::unordered_set<const FeatureSpec *> in_bucket(bucket.begin(),
                                                    bucket.end());
  // 桶内依赖计数 + 反向索引 (u → 依赖 u 的桶内节点).
  std::unordered_map<const FeatureSpec *, std::size_t> remaining_deps;
  std::unordered_map<const FeatureSpec *,
                     std::vector<const FeatureSpec *>>
      dependents;
  for (const FeatureSpec *s : bucket) {
    std::size_t cnt = 0;
    for (const FeatureSpec *d : s->deps)
      if (in_bucket.count(d)) {
        ++cnt;
        dependents[d].push_back(s);
      }
    remaining_deps[s] = cnt;
  }

  auto build_from = [&](const FeatureSpec *start) {
    auto rem = remaining_deps;
    std::vector<const FeatureSpec *> chain;
    std::unordered_set<const FeatureSpec *> in_chain;
    auto push = [&](const FeatureSpec *n) {
      chain.push_back(n);
      in_chain.insert(n);
      for (const FeatureSpec *dep : dependents[n])
        --rem[dep];
    };
    push(start);
    while (chain.size() < bucket.size()) {
      const FeatureSpec *best = nullptr;
      double best_sim = -1.0;
      for (const FeatureSpec *s : bucket) {
        if (in_chain.count(s) || rem[s] != 0)
          continue;
        double sim = name_sim(chain.back()->name, s->name);
        if (sim > best_sim) {
          best_sim = sim;
          best = s;
        }
      }
      if (!best)
        break; // 不应发生 (拓扑合法 + 无环)
      push(best);
    }
    return chain;
  };

  std::vector<const FeatureSpec *> best_chain;
  double best_total = -1.0;
  for (const FeatureSpec *start : bucket) {
    if (remaining_deps.at(start) != 0)
      continue;
    auto chain = build_from(start);
    double t = total_sim(chain);
    if (t > best_total) {
      best_total = t;
      best_chain = chain;
    }
  }
  // 退化兜底: 桶内全有依赖 (不应发生), 返回原序.
  if (best_chain.empty())
    return bucket;
  return best_chain;
}

// 按 Kind 分桶展示 (inter → filter → factor); 先对全部已定义节点跑拓扑序
//   (保证跨桶依赖天然满足), 再每桶内用贪心最近邻路径构造重排
//   (同桶对仗名字就近聚成 cluster).
std::vector<const FeatureSpec *>
by_kind_then_topo(const std::vector<const FeatureSpec *> &all_defined) {
  std::vector<const FeatureSpec *> topo = topo_order_all(all_defined);
  std::vector<const FeatureSpec *> out;
  for (Kind k : {Kind::Inter, Kind::Filter, Kind::Factor}) {
    std::vector<const FeatureSpec *> bucket;
    for (const FeatureSpec *s : topo)
      if (s->kind == k)
        bucket.push_back(s);
    bucket = greedy_nearest_neighbor(bucket);
    for (const FeatureSpec *s : bucket)
      out.push_back(s);
  }
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
constexpr std::size_t W_CN = 14;
constexpr std::size_t W_AXIS = 4;
constexpr std::size_t W_ACTIVE = 6;
constexpr std::size_t W_DEPS = 56;
constexpr std::size_t W_FORMULA = 60;
constexpr std::size_t W_ASSUMPTION = 60;

const char *active_str(bool active) { return active ? "✓" : "—"; }

void print_row(const std::string &kind, const std::string &feature,
               const std::string &cn_name, const std::string &axis,
               const std::string &active, const std::string &deps,
               const std::string &formula, const std::string &assumption) {
  std::cout << fit(kind, W_KIND) << " | " << fit(feature, W_FEATURE) << " | "
            << fit(cn_name, W_CN) << " | " << fit(axis, W_AXIS) << " | "
            << fit(active, W_ACTIVE) << " | " << fit(deps, W_DEPS) << " | "
            << fit(formula, W_FORMULA) << " | " << fit(assumption, W_ASSUMPTION)
            << "\n";
}

void print_header() {
  print_row("kind", "feature", "中文名", "轴", "active", "deps", "formula",
            "assumption");
  std::cout << std::string(W_KIND, '-') << "-+-" << std::string(W_FEATURE, '-')
            << "-+-" << std::string(W_CN, '-') << "-+-"
            << std::string(W_AXIS, '-') << "-+-"
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
    print_row(kind_name(s->kind), s->name, s->cn_name, axis_name(s->axis),
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
                            [](const FeatureSpec *f) { return f->cn_name; })
              << "\n";
    std::cout << st->name << " factor: "
              << join_names(st->weights,
                            [](const strategy::FactorWeight &w) {
                              return w.f->cn_name;
                            })
              << "\n";
  }
  std::cout << "=====================================================================\n";
}

} // namespace feature
