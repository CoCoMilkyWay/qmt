#pragma once

#include "feature/axis.hpp"
#include "feature/pit.hpp"

namespace feature {

// Phase 1: 通用 flow — 仅迭代 pit.hpp 的 ITFS[] 表, 不出现具体 itf 名.
//
// per-itf:
//   1) 算 cache_key (FNV: POOL_VERSION + itf.file_name + 月 parquet
//      relpath/size/mtime + axes 语义 hash (dates+codes 内容)).
//   2) 试 hit: mmap data/pool/<itf>.bin → 校验 magic/version/key → cache_layout(Map)
//      把 pool 字段 PoolArr 视图指向 mmap 区 (零反序列化 / 零 copy).
//   3) miss: itf.build(axes, files, pool) 端到端从月度 parquet 并行写入 pool;
//      cache_layout(Write) dump 到 .bin (atomic 落盘).
// 全部 itf 收尾:
//   apply_meta_overlays (读 static_data 写 row=last_d 一行, mmap COW 触发)
//   post_ffill (网格 per-A linear, mmap COW dirty 页)
// 这两段不入 cache, 每次都跑最新代码 (改 ffill / overlay 逻辑不用 bump POOL_VERSION).
//
// 增减 itf 完全不动本文件.
void load_pit(const Axes &, PitPool &);

} // namespace feature
