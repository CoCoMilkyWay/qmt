#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace feature {

// ============================================================================
// SW2021 一级行业 (申万 2021 版) — 31 个稳定枚举.
//   ID 0 = 未知 / 未覆盖, ID 1..31 = SW2021_L1_NAMES[ID] (1-indexed).
//
// 内部 ID 是任意稳定编号, 仅用于把行业归属编码进 float Tensor (industry_l1
// inter feature, per (D, A) 时变); 业务侧通过 name<->id 双向映射访问.
//
// 来源: cn_stock_industry_component WHERE industry='sw2021' 取
//       industry_level1_name 全集 (验证 sample 自 2024-01-02, 共 31 个).
// 顺序按 sorted 字典序固定, 后续若申万扩表需在末尾追加 (不动旧 ID 保稳定).
// ============================================================================
inline constexpr std::array<std::string_view, 32> SW2021_L1_NAMES = {{
    "未知",     // 0
    "交通运输", // 1
    "传媒",     // 2
    "公用事业", // 3
    "农林牧渔", // 4
    "医药生物", // 5
    "商贸零售", // 6
    "国防军工", // 7
    "基础化工", // 8
    "家用电器", // 9
    "建筑材料", // 10
    "建筑装饰", // 11
    "房地产",   // 12
    "有色金属", // 13
    "机械设备", // 14
    "汽车",     // 15
    "煤炭",     // 16
    "环保",     // 17
    "电力设备", // 18
    "电子",     // 19
    "石油石化", // 20
    "社会服务", // 21
    "纺织服饰", // 22
    "综合",     // 23
    "美容护理", // 24
    "计算机",   // 25
    "轻工制造", // 26
    "通信",     // 27
    "钢铁",     // 28
    "银行",     // 29
    "非银金融", // 30
    "食品饮料", // 31
}};

inline constexpr std::size_t SW2021_L1_COUNT = SW2021_L1_NAMES.size(); // 32

// name → id (1..31; 不在表内 / 空 → 0). 静态字符串比较, O(31) 线性, 启动期 +
// pool_b mask 编译用一次, 性能无关键路径.
uint8_t sw2021_l1_name_to_id(std::string_view name);

// id → name (id 越界返回 "未知")
inline std::string_view sw2021_l1_id_to_name(uint8_t id) {
  return id < SW2021_L1_COUNT ? SW2021_L1_NAMES[id] : SW2021_L1_NAMES[0];
}

} // namespace feature
