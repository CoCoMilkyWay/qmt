Motive: 实盘量化交易; 国金证券 QMT 客户端下单; 全市场张量因子选股, 按 `visible_date` 切日落本地, 数据通道 BigQuant DAI + Tushare HTTP.

# 项目结构

```
qmt/
├── run.py                           # 统一入口: build (py/main.py) + run (py/mode_*.py)
├── app/
│   └── gjzqqmt/                     # 国金证券 QMT 客户端 (Linux Wine 跑 Windows 程序)
│       ├── run.md                   # Wine 安装 + 启动指南 (XtItClient=主端, XtMiniQmt=API端)
│       ├── 国金证券QMT交易端/       # 客户端本体 (bin.x64 是 64 位主程序)
│       └── QMT操作说明文档/         # 官方 PDF (操作/Python API/网格/VBA/算法交易)
├── cpp/                             # C++23 实现 (Clang/Linux, header-only boost + yyjson + arrow)
│   ├── projects/main/               # CMake 构建 (DEBUG / PROFILE / ASSERT / PRODUCTION)
│   ├── include/
│   │   ├── config.hpp               # 全局常量 (BigQuant host/token, Tushare host/token, lookback, 拉取窗口, 去重窗口)
│   │   ├── misc/                    # 通用工具 (date / fs / store / schedule / journal / logging / progress / timer / affinity)
│   │   │                              # fs.hpp:    git_root / read_file_all / atomic_write / atomic_write_json / serialize_json
│   │   │                              # store.hpp: 数据集落地公共层 (day path / _empty.json / lastupdate / scan_missing); 不绑定数据源
│   │   │                              # schedule.hpp: plan_fetch_segments (bigquant Day + tushare 共用调度器)
│   │   │                              # journal.hpp:  通用 (name, yyyymm) 原子整月事务 (parquet import 用)
│   │   ├── package/yyjson/          # JSON 库
│   │   ├── package/arrow/           # Arrow Flight wire transport (从 pyarrow vendor, 见 vendor_from_pyarrow.sh)
│   │   ├── api/                     # 数据接入子系统 (数据入); 两侧 API 完全对仗
│   │   │   ├── bigquant/            # 26 张表 DAI Arrow Flight (明文 gRPC, Basic Token → JWT)
│   │   │   │   ├── dai.hpp          # DaiClient (lazy Flight + query → arrow::Table)
│   │   │   │   ├── spec.hpp         # SPECS + fetch(client, spec, start, end) (SQL 模板内置)
│   │   │   │   ├── parse.hpp        # arrow::Table → yyjson 行式 JSON + bucket_by_visible_date
│   │   │   │   ├── store.hpp        # write_meta + write_by_visible_date
│   │   │   │   ├── pipeline.hpp     # update: plan → fetch → write (+ _empty / lastupdate)
│   │   │   │   └── import.hpp       # parquet 月数据库整月覆盖导入 (与 DAI 独立, journal 原子)
│   │   │   └── tushare/             # 3 张事件表 (forecast / express / disclosure) HTTP+JSON
│   │   │       ├── http.hpp         # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│   │   │       ├── spec.hpp         # SPECS + fetch(http, spec, task) + FetchStrategy (day_params 判别 range / per-day)
│   │   │       ├── store.hpp        # write_by_visible_date (与 bigquant 对仗, 整文件覆盖)
│   │   │       └── pipeline.hpp     # update: plan → fetch → write (+ _empty / lastupdate)
│   │   └── feature/                 # feature 子系统头文件 (张量出)
│   └── src/
│       ├── main.cpp                 # bigquant::update → tushare::update → feature::build → Tensor T[F][A][D]
│       ├── api/
│       │   ├── bigquant/            # https / signer / dai / parse / spec / store / pipeline
│       │   └── tushare/             # http / spec / store / pipeline
│       └── feature/                 # 4-phase 特征系统; 业务密集化 + 外层 flow 完全 agnostic
│                                    # 单点真理: pit.cpp (itf 维) + feature.cpp (feature 维)
│           ├── axis.cpp             # Phase 0: load_axes + load_stock_meta (per-A 静态)
│           ├── feature.cpp          # 【单点真理 feature】每 feature 一个 ts_xxx/cs_xxx compute fn + 末尾 FEATURES[] 表挂载
│           │                        # F 枚举顺序 = FEATURES[] 索引 = 计算顺序 (后段读已写就的 T.ts_row(prior_f, a))
│           ├── tensor.cpp           # Tensor 容器 (统一 [F][A][D] layout, ts_row 连续, gather/scatter cs_row)
│           ├── pit.cpp              # 【单点真理 itf】每 itf 一个 namespace block (prealloc + aggregate + replay + post_sort) + 末尾 ITFS[] 表挂载
│           ├── load.cpp             # Phase 1 通用 flow: 仅迭代 ITFS[] (prealloc → aggregate cache → replay → post_sort/post_ffill), 不出现 itf 名
│           ├── ts.cpp               # Phase 2 通用 flow: per-A 并行, 迭代 FEATURES[] 中 axis==TS 的 compute_ts 调; kernel 在 ts.hpp (state_machine_intervals 模板)
│           ├── cs.cpp               # Phase 3 通用 flow: per-D 并行, 迭代 FEATURES[] 中 axis==CS 的 compute_cs 调; kernel (winsor_mad / z / pct_rank / factor_pipeline) 在 cs.hpp/cpp
│           └── build.cpp            # 编排入口: 串 4 phase + misc::Timer 报段时
├── data/                            # 落地 (按 visible_date 切日, gitignored)
│   ├── _meta/                       # 单文件全量, 每次 update 覆盖刷新
│   │   ├── trading_days.json        # axis 源 D 轴 (BigQuant, market_code='CN')
│   │   ├── holidays.json            # 节假日 (BigQuant, market_code='CN')
│   │   ├── cn_stock_static_data.json # ★ 主 meta — 真盘前 09:00 全市场快照 (BigQuant Snapshot,
│   │   │                              #   MAX(date) 一日; 含 upper_limit/lower_limit/suspended/
│   │   │                              #   st_status/pre_close/adjust_factor/crd_buy_flag 等).
│   │   │                              #   PIT hybrid overlay 给实盘当日 (row=last_d) status 填充
│   │   │                              #   suspended/st_status (历史日 day file 已写, 不动).
│   │   ├── cn_stock_basic_info.json # 补充 meta — axis 源 A 轴 + 真静态属性 (BigQuant Static).
│   │   │                              #   list_date/delist_date/list_sector/industry 等 static_data
│   │   │                              #   没有的字段; 实际盘后更新, 按 -1 滞后理解, 业务上"几乎不变".
│   │   └── <name>.lastupdate        # 单表去重时间戳 (unix epoch s); 上次成功距今 < config::API_DEDUP_WINDOW_SECONDS 跳过
│   └── YYYY/
│       └── MM/
│           ├── _empty.json          # 反向稀疏标记 {name: [DD,...]} = 拉过且为空
│           └── DD/<name>.json       # 仅在该天有数据时存在 (PK 唯一, 路径 = visible_date)
│                                    # 三态: file 存在 / 在 _empty / 都不在 = 有数据 / 拉过空 / 未拉
│                                    #
│                                    # 27 张表 (全 JSON, 统一行式: [{col1:v,col2:v,...}, ...] 一行一记录, 人眼可读):
│                                    #   BigQuant 24 (DAI Arrow Flight → arrow::Table → 行式 JSON):
│                                    #     cn_stock_instruments,
│                                    #     cn_stock_industry_component (★ 月初一份), cn_stock_industry_change,
│                                    #     cn_stock_industry_real_bar1d, cn_stock_industry_valuation,
│                                    #     cn_stock_capital, cn_stock_dividend, cn_stock_allotment,
│                                    #     cn_stock_margin_trading_detail, cn_stock_margin_trading_market,
│                                    #     cn_stock_shareholder, cn_stock_shares, cn_stock_status, cn_stock_suspend,
│                                    #     cn_stock_name_change, cn_stock_dragon_list, cn_stock_real_bar1d, cn_stock_limit_price,
│                                    #     cn_stock_static_data (☆ Snapshot, _meta 单文件; PIT overlay 给 row=last_d 用),
│                                    #     cn_stock_financial_{income,cashflow,balance}_general_pit,
│                                    #     cn_stock_financial_{ttm,notes}_shift
│                                    #   Tushare 3 (HTTP+JSON 行式直落):
│                                    #     forecast, express, disclosure
│                                    #
│                                    # ★ MonthFirst: cn_stock_industry_component 由 spec.freq=MonthFirst 决定每月仅取
│                                    #   visible_date=MIN(date) 一份全行业成分快照, 该月仅 1 个 DD 子目录存在文件;
│                                    #   月内细粒度变动靠 cn_stock_industry_change (Day) 增量 cover.
├── py/                              # 构建/运行模式 (run.py 调用)
│   ├── main.py                      # CMake 配置 + 编译
│   └── mode_{debug,profile,assert,production}.py
└── doc/
    ├── bigquant/                    # BigQuant DAI 文档 + 探测脚本
    │   ├── used/                    # 采用表的 schema/example/api 索引
    │   │   ├── api.md               # 表的中英文名 / 收费 / 更新频率 / 描述
    │   │   ├── schema.md            # 字段类型 (timestamp[ns] / string / double / int8 / ...)
    │   │   └── example.md           # 前 5 行样例
    │   ├── fetch.py / probe.py      # Python 端 fetch + 探测脚本
    │   └── schema.py                # schema dump 工具
    ├── research/                    # 数据研究脚本
    │   ├── analysis.py              # 覆盖率分析
    │   └── analysis.md              # 分析结果
    └── tushare/                     # tushare API 文档
        ├── tushare.md               # 接口索引
        ├── help/                    # 通用 trick (本地化 / HTTP 协议 / 数据库落地)
        ├── basic/                   # 基础信息
        └── financial/               # 财务报表 (forecast / express / disclosure_date / dividend / ...)
```

# 因子张量 T[D, A, F]

- `D` = 交易日 (BigQuant `trading_days` WHERE `market_code='CN'`)
- `A` = instrument (BigQuant `cn_stock_basic_info.instrument` 全量, 含已退市)
- `F` = 下表 feature
- dtype: 统一 **float** (32 比特 float; bool 用 0.0/1.0)
- kind (与「轴」独立, 勿用 kind 推断时序/截面; 以字段表「轴」列与 `FEATURES[]` 为准):
  - `filter` (1=排除该 D-A)
  - `factor` (∈[0,1] NaN=不参与; 当前实现均为截面归一后的连续得分)
  - `inter` (中间量; 含时序与截面两类, 不一刀切)

## 数据源 → 张量 (build-time PIT)

**核心**: 偏移 `offset(itf)` 在 build 阶段一次性消化进 cutoff, 张量 `T[D, A, F]` row D 只含 T 当日信号时点已可知的信息. 下游 (策略/回测/实盘) 直读 row D, 不再处理未来数据.

**数据通道与抓取策略** — 两路通道, 抓取规则各自一套:

- **BigQuant DAI**: Arrow Flight (`grpc+tcp://bigquant.com:17010`, 明文 gRPC + Arrow IPC RecordBatch, `query(SQL, filters)` → `arrow::Table` 零拷贝, Basic Token → JWT bearer). 5 种 FetchKind 见 `cpp/include/api/bigquant/spec.hpp`, 见下表.
- **Tushare HTTP** (`:80`, 明文 JSON POST): 三张事件表 (`forecast` / `express` / `disclosure`), 各表自带 `ann_date` 字段作 visible_date; 抓取走 `FetchStrategy::day_params` 判别 range / per-day.

BigQuant FetchKind (`<vd>` = `TableSpec::visible_date` — Static 为空, Partition 通常是 `date`, Where 通常是 `publish_date` / `end_date`):

| kind      | freq       | SQL 写法                                                                 | filters          | 适用                                                                                                                                      |
| --------- | ---------- | ------------------------------------------------------------------------ | ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Static    | Day        | `SELECT * FROM <name>`                                                   | `{}`             | `cn_stock_basic_info` (无 date 维)                                                                                                        |
| Partition | Day        | `SELECT * FROM <name>`                                                   | `{"date":[s,e]}` | 日频网格 (行情 / 状态 / 财务 PIT / ...)                                                                                                   |
| Partition | MonthFirst | `WHERE <vd> = (SELECT MIN(<vd>) FROM <name> WHERE <vd> BETWEEN s AND e)` | `{"date":[s,e]}` | `cn_stock_industry_component` 月初一份全行业成分, 月内变动靠 `cn_stock_industry_change` (Day) 增量 cover                                  |
| Where     | Day        | `SELECT * FROM <name> WHERE <vd> >= s AND <vd> <= e`                     | `{}`             | 事件型 (`publish_date` / `end_date` 等非分区列)                                                                                           |
| Snapshot  | Day        | `WHERE <vd> = (SELECT MAX(<vd>) FROM <name> WHERE <vd> BETWEEN s AND e)` | `{"date":[s,e]}` | `cn_stock_static_data` 真盘前 09:00 全市场快照, 取窗口内 MAX(date) 一日, 落 `data/_meta/<name>.json` 单文件; PIT overlay 给 row=last_d 用 |

**落地** (按 `visible_date` 切日)
- 路径 `data/YYYY/MM/DD/<name>.json` — 日历日切分 (周末/节假日同样写盘); 全表统一**行式**落盘 (`[{col1:v, col2:v, ...}, ...]` 一行一记录, 人眼可读). BigQuant 经 `parse.hpp::table_to_json` 按 row 出 obj 序列化; Tushare 响应已是 JSON, 直落. 表名互不冲突, 同目录共存.
- 单文件 `_meta`: `data/_meta/{trading_days, holidays, cn_stock_static_data, cn_stock_basic_info}.json`
  - `cn_stock_static_data` (**主 meta**, Snapshot, 真盘前 09:00): DAI 取 [start, end] 内 `MAX(date)` 一日的全市场快照, 一次响应直写; 不切日 (不进 day file). 用于 hybrid PIT overlay 给实盘当日 (= `axes.last_d`) 的 `status` 字段填充真盘前值 (见 §cutoff).
  - `cn_stock_basic_info` (补充 meta, Static, 无 date 列): DAI 一次响应直写; 仅用于 `static_data` 没有的字段 (`list_date` / `delist_date` / `list_sector` / `industry` / ...). 实际盘后更新, 按 -1 滞后理解, 业务上字段几乎不变.
  - `trading_days` / `holidays` (axis 源, `emit_meta=true`): 跟普通 Partition+Day 一样按 visible_date 切日落 day file, 每轮 update 末尾 `aggregate_meta` 从全部 day file 聚合产生 (day file 是真实源, _meta 为聚合产物).
- **完整性设计** (BigQuant / Tushare 完全沿用同一套, `store::write_by_visible_date` 平行实现, 整文件覆盖语义):
  - **PK upsert** (同次响应): 同 PK 同次响应末条胜, PK 因 itf 而异 (见 `cpp/src/api/{bigquant,tushare}/spec.cpp`).
  - **三态稀疏**: file 存在 (有数据) / 在 `_empty.json` (拉过空) / 都不在 (未拉); 避免空日反复回 fetch.
  - **lastupdate 去重**: `data/_meta/<name>.lastupdate` (unix epoch s); 上次成功距今 < `config::API_DEDUP_WINDOW_SECONDS` 跳过整段 (`misc::store::should_skip_api` / `mark_api_updated`).
  - **lookback 增量回扫**: `scan_missing` 在 `[start, end]` 内, 文件不存在必拉 + 最近 `lookback_days` 日历日内必拉 (PK upsert 吃订正; 7 天 ≈ 5 交易日, 兜住当日未结算累积缺失).
  - **tmp+rename 写**: 单文件 atomic 替换, 中断不留半成品.
- 外部资料: 入库时机 (BigQuant `doc/bigquant/api.md` 更新时间列, 多数 17:00–20:00 盘后批发; Tushare `doc/tushare/help/数据更新说明.md` 及各 API 自身 doc); 公告披露时段 (`doc/exchange/公告类别和发布时间.md`, SSE/SZSE 各时段 + 非交易日 13–17 / 12–16 直通).

**cutoff** (build-time, 实盘/回测同一公式)
- 信号时点 ≜ 交易日 T 盘中, 信号计算前 1 分钟刷库; 此后每个 itf 按 `visible_date ≤ T + offset(itf)` 切片写入 row D.
- `offset` 单位 = **日历日**, 含周末/节假日.

**全部 BigQuant 表实际入库时间都是盘后 17:00+** (`api.md` 更新时间列实测). 项目按业务可推出性把所有 itf 归到下面两种 cutoff 模式之一:

| 模式              | offset | 含义                                                                                                                                                                                                                                                  | 适用                                                                          |
| ----------------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| **normal**        | `−1`   | 承认滞后, row D=T 取 T-1 day file 数据. T-1 盘后公告 + T-1 至 T 之间的周末/节假日公告 (含 SSE/SZSE 非交易日直通时段) 自然划入 T 行.                                                                                                                   | 绝大多数 itf — 默认安全选项                                                   |
| **hybrid (伪装)** | `0`    | 假装盘前: 历史 day file 按 row=v_idx 消化 (T 当日就用 T 的 day file); 最后一天 (= 实盘当日, day file 还没入库) 由 `apply_meta_overlays` 用 `cn_stock_static_data` 真盘前 09:00 快照**填充** row=last_d (仅写这一行, 历史天完全不动 ⇒ "填充而非覆盖"). | 仅 `cn_stock_status`. (`cn_stock_suspend` 同类语义但代码未消费, 见入库时机表) |

hybrid 适用判定: 该 itf 当日值在 T 当日开盘前**业务上已实际确定** (即使 API 入库晚也不会失真). 严格 PIT 下应 −1, 但 −1 会显著伤害 alpha (例: ST 翻转日整体滞后 1-2 天); hybrid 在保证因果性的同时拿回 T 当日真值:
- ✅ `cn_stock_status` (st_status / suspended): ST / 停牌当日开盘前即生效, 静态确定 → 适用 hybrid.
- ❌ `cn_stock_limit_price`: 虽然 `limit[T] = round(close[T-1] × (1±pct), 2)` 在 T-1 晚已确定, 但 ST 翻转日 pct 变化日 day file 写的仍是旧 limit (T-1 是停牌日, 写的是停牌前旧 limit); 严格 PIT 安全 ⇒ 用 normal `−1` (承认滞后).
- ❌ 其他盘后 itf (industry / dividend / forecast / 财务 / ...) — 全 normal `−1`.

**入张量映射** (cutoff 后按 itf 类落 row D)
- **网格** (D=trade_date, A=instrument): 每条记录唯一 (D, A) 单元. row D 取 `max{ visible_date ≤ T + offset }` (offset=0 → 自身; offset=−1 → 上一交易日, 周末/假日 visible_date 不存在自动跳过).
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见 §字段表 deps (例: `forecast` / `income_general_pit` / `cashflow_general_pit` / `balance_general_pit` 按 `report_date`, `dividend` 按 `publish_date`, `name_change` 按 `end_date`). 状态机型 (`profit_st` / `revenue_st` / `dividend_st`) 同样按此 cutoff 回放 `visible_date` 升序流.
- **月初快照** (D, A): `cn_stock_industry_component` 月初落一份, build 时取 `max{ visible_date ≤ T }` 的快照广播到 (D, A); 月内细变动叠加 `cn_stock_industry_change` 事件流 → `industry_l1` inter feature.
- **overlay** (row=last_d 单行填充): `cn_stock_static_data` (`_meta` 单文件) → `status.{suspended, st_status}` 2 字段. 仅写 row=last_d, 历史日子完全不动. 详见 §cutoff 表里的 hybrid 模式.
- **asset 静态**: `cn_stock_basic_info` 全量 snapshot 广播到 (D, A); `list_date / delist_date` 决定 (D, A) 行有效, 不走 `visible_date`.
- **axis**: `trading_days` WHERE `market_code='CN'` 生成 D 轴; `cn_stock_basic_info.instrument` 全量生成 A 轴.

**一致性** (build 完成 → 张量 PIT-clean, 下游无未来数据风险)
- **跨次修正** (replay 安全): 修正写新 day-file, 旧版本留存不被覆盖 → replay 任意 T 按上述 cutoff 自动选当时可见版本 (同次去重见 §落地完整性 PK upsert).
- **回测 = 实盘**: 同一份 build 代码 + 同一组 offset → 同一份 PIT 张量.
- 已知 best-effort 瑕疵 (不可消除, 接受):
  1. 公告级时间戳缺失 → 同 `visible_date` 内盘前/盘中/盘后无法区分, 统一按盘后保守 → 计入 next-day cutoff (实盘错过 T 当日盘前直通公告, 与回测一致).
  2. `cn_stock_basic_info` 仅当前 snapshot → `list_sector` / `industry` 历史变更无法回溯; 行业变更已用 `cn_stock_industry_component` (月初) + `cn_stock_industry_change` (日频) 补救, 但 `list_sector` 转板股全期按当前归类.
  3. `cn_stock_status` hybrid 伪装的隐含假设: ST / 停牌当日盘前已生效 (公告 → 停牌一日 → 复牌即生效新标识). 极端情况下 T 盘中突发停牌不会反映到 row D=T (因 day file 盘后才入库), 信号刷库时拿到的是 T-1 终态延续, 但 `cn_stock_static_data` 真盘前 09:00 快照对实盘当日已捕获 (next-day cutoff). 想绝对保守可把 status 退回 normal `−1`.

「模式」列扩展取值 (`normal` / `hybrid` 见上方 cutoff 模式表): `真盘前` (offset=0, 入库 < 信号时点, 不需 overlay) / `axis` (axis 源, 不入张量) / `static` (Static 表, 无 date 维, 按 -1 滞后理解).

| 类       | name                                      | 通道                | 入库时间 (api.md)    | visible_date   | 模式       | 偏移 |
| -------- | ----------------------------------------- | ------------------- | -------------------- | -------------- | ---------- | ---- |
| axis     | `trading_days` / `holidays`               | BigQuant            | 定期 (新年度排程)    | `date`         | axis       | —    |
| overlay  | `cn_stock_static_data`                    | BigQuant Snapshot   | **真盘前** 09:00     | `date` (MAX)   | 真盘前     | 0    |
| asset    | `cn_stock_basic_info`                     | BigQuant Static     | 每轮 update 覆盖刷新 | —              | static     | -1   |
| 网格     | `cn_stock_instruments`                    | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 月初快照 | `cn_stock_industry_component`             | BigQuant MonthFirst | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_change`                | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_real_bar1d`            | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_valuation`             | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_real_bar1d`                     | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_limit_price`                    | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_status`                         | BigQuant            | 盘后 17:00           | `date`         | **hybrid** | 0    |
| 网格     | `cn_stock_suspend`                        | BigQuant            | 盘后 17:00           | `date`         | **hybrid** | 0    |
| 网格     | `cn_stock_shares`                         | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_margin_trading_detail`          | BigQuant            | **真盘前** 10:00     | `date`         | 真盘前     | 0    |
| 网格     | `cn_stock_margin_trading_market`          | BigQuant            | **真盘前** 10:00     | `date`         | 真盘前     | 0    |
| 网格     | `cn_stock_dragon_list`                    | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 事件     | `cn_stock_capital`                        | BigQuant            | 盘后 17:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_dividend`                       | BigQuant            | 盘后 17:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_allotment`                      | BigQuant            | 盘后 17:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_shareholder`                    | BigQuant            | 盘后 17:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_name_change`                    | BigQuant            | 盘后 17:00           | `end_date`     | normal     | −1   |
| 财务 PIT | `cn_stock_financial_income_general_pit`   | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 PIT | `cn_stock_financial_cashflow_general_pit` | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 PIT | `cn_stock_financial_balance_general_pit`  | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 TTM | `cn_stock_financial_ttm_shift`            | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 财务附注 | `cn_stock_financial_notes_shift`          | BigQuant            | 盘后 17:00           | `date`         | normal     | −1   |
| 事件     | `forecast`                                | Tushare             | 公告实时 (通常盘后)  | `ann_date`     | normal     | −1   |
| 事件     | `express`                                 | Tushare             | 公告实时 (通常盘后)  | `ann_date`     | normal     | −1   |
| 事件     | `disclosure`                              | Tushare             | 公告实时 (披露计划)  | `ann_date`     | normal     | −1   |

## 字段表

本节是 feature 的「契约 / 数学定义」(描述"做什么"); 实现镜像在 `cpp/src/feature/feature.cpp` 的 `FEATURES[]` 表 + `impl::ts_*` / `impl::cs_*` (描述"怎么做"). 增减/修改 feature 须同步两处.

排序 (本表, 阅读用): filter → factor → inter; inter 内部按 causal (raw → derived); 相关字段就近. (注: `cpp/include/feature/feature.hpp` 的 `F` 枚举顺序 = 计算顺序, 与本表排序独立; enum 大段仅 TS / CS 两类, 段内按"相似聚集"对仗 — raw 网格 / raw 财务 / raw meta 派生 / derived / filter / pool.)

列约定:
- `轴`: `时序` = per A 沿 D 计算 (无截面依赖, A 维可并行); `截面` = per D 沿 A 计算 (有截面依赖, A 维不可并行). `filter` / `factor` / `inter` 三类均可能出现两种轴之一, 仅读本行.
- `deps`: `itf:<name>` ≡ 该 itf 经 §入张量统一规则 切到 (D, A); 其它为 inter / filter feature 名 或 `meta:<field>` (asset 静态).
- `assumption`: `—` = 定义自洽; 形如 `[元]` `[%]` `[ratio]` `[股]` 的方括号前缀标注 inter 输出单位.

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm12** (高季节性, 12 个月 ≡ 4 报告期): 取 `cn_stock_financial_ttm_shift.*_ttm` (shift=0) 或 mcap_raw / TTM 字段自算 (估值类, 支持负值).
- **ttm3** (低季节性, 3 个月单期 snapshot ≡ MRQ): 取 `cn_stock_financial_balance_general_pit.*` (latest shift) 自算; 例 pb = mcap_raw / total_owner_equity.

| kind   | feature      | 轴   | deps                                                                               | formula                                                                                                                                                                                                                                                | assumption                                                                                                                                                                                                                                                                                                  |
| ------ | ------------ | ---- | ---------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| filter | profit_st    | 时序 | itf:forecast, itf:cn_stock_financial_income_general_pit                            | 状态机 (per A):                                                                                                                                                                                                                                        | 正式 PIT 年报出现即终止; 未披露的股票不出, 4 月底安全网兜底                                                                                                                                                                                                                                                 |
|        |              |      |                                                                                    | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.last_parent_net < 0` 时                                                                                                                                                           |                                                                                                                                                                                                                                                                                                             |
|        |              |      |                                                                                    | 按 `forecast.ann_date` 触发, 至 `cn_stock_financial_income_general_pit.report_date == forecast.end_date` 或 `(end_date.Y+1, 4, monthend)` 终止 (取较早)                                                                                                |                                                                                                                                                                                                                                                                                                             |
| filter | revenue_st   | 时序 | itf:forecast, itf:cn_stock_financial_income_general_pit, rev_raw, meta:list_sector | 状态机 (per A) ∧ `meta.list_sector == 1 ∧ rev_raw < (3e8 if end_date.Y ≥ 2024 else 1e8)`:                                                                                                                                                              | 同 profit_st 终止条件; `rev_raw` 仅用于区间内营收阈值判定; 主板判定 inline `meta.list_sector[a] == 1` (int8 编码: 1=主板 / 2=创业板 / 3=科创板 / 4=北交所; asset 静态)                                                                                                                                      |
|        |              |      |                                                                                    | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101` 时                                                                                                                              |                                                                                                                                                                                                                                                                                                             |
|        |              |      |                                                                                    | 按 `forecast.ann_date` 触发, 至 `cn_stock_financial_income_general_pit.report_date == forecast.end_date` 或 `(end_date.Y+1, 4, monthend)` 终止 (取较早)                                                                                                |                                                                                                                                                                                                                                                                                                             |
| filter | dividend_st  | 时序 | itf:cn_stock_dividend, ni_raw, share_raw, meta:list_sector                         | `meta.list_sector == 1 ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_after_tax × share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7`                                                                                                                                      | 3y 窗口 = `dividend.report_date.Y ∈ [Y-3, Y-1]` (Y = `dividend.publish_date.Y`); share_raw 取 `publish_date` 当日快照; 单位均 [元]                                                                                                                                                                          |
| filter | trading_st   | 时序 | low_p, low_mc                                                                      | `rolling_20D(low_p ∨ low_mc).all()`                                                                                                                                                                                                                    | —                                                                                                                                                                                                                                                                                                           |
| filter | risk_warn    | 时序 | itf:cn_stock_status                                                                | 派生 4 态 (int8 → float; 0=正常, 1=ST, 2=*ST, 3=退市整理期): 历史 day file 由 `cn_stock_status.st_status` (1/2 → 1/2) ∧ `is_risk_warning` (st_status==0 ∧ rw!=0 → 3) 派生; 实盘当日由 `cn_stock_static_data.in_delist` (=1 → 3) ∧ `st_status` 派生     | —                                                                                                                                                                                                                                                                                                           |
| filter | new_list     | 时序 | list_age                                                                           | `0 ≤ list_age < 60`                                                                                                                                                                                                                                    | —                                                                                                                                                                                                                                                                                                           |
| factor | close        | 截面 | close_raw                                                                          | `pct_rank(z(winsor_mad(1 / close_raw)))` + 截面均值填充                                                                                                                                                                                                | —                                                                                                                                                                                                                                                                                                           |
| factor | mcap         | 截面 | mcap_raw                                                                           | `pct_rank(z(winsor_mad(1 / mcap_raw)))` + 截面均值填充                                                                                                                                                                                                 | —                                                                                                                                                                                                                                                                                                           |
| factor | fmcap        | 截面 | fmcap_raw                                                                          | `pct_rank(z(winsor_mad(1 / fmcap_raw)))` + 截面均值填充                                                                                                                                                                                                | —                                                                                                                                                                                                                                                                                                           |
| factor | pe_ttm12     | 截面 | pe_raw                                                                             | `pct_rank(z(winsor_mad(1 / pe_raw)))` + 截面均值填充                                                                                                                                                                                                   | —                                                                                                                                                                                                                                                                                                           |
| factor | pb_ttm3      | 截面 | pb_raw                                                                             | `pct_rank(z(winsor_mad(1 / pb_raw)))` + 截面均值填充                                                                                                                                                                                                   | —                                                                                                                                                                                                                                                                                                           |
| factor | ps_ttm12     | 截面 | ps_raw                                                                             | `pct_rank(z(winsor_mad(1 / ps_raw)))` + 截面均值填充                                                                                                                                                                                                   | —                                                                                                                                                                                                                                                                                                           |
| factor | pcf_ttm12    | 截面 | pcf_raw                                                                            | `pct_rank(z(winsor_mad(1 / pcf_raw)))` + 截面均值填充                                                                                                                                                                                                  | —                                                                                                                                                                                                                                                                                                           |
| factor | roe_ttm12    | 截面 | roe_raw                                                                            | `pct_rank(z(winsor_mad(roe_raw)))` + 截面均值填充                                                                                                                                                                                                      | —                                                                                                                                                                                                                                                                                                           |
| factor | roa_ttm12    | 截面 | roa_raw                                                                            | `pct_rank(z(winsor_mad(roa_raw)))` + 截面均值填充                                                                                                                                                                                                      | —                                                                                                                                                                                                                                                                                                           |
| factor | dy_ttm12     | 截面 | dy_raw                                                                             | `pct_rank(z(winsor_mad(dy_raw)))` + 截面均值填充                                                                                                                                                                                                       | —                                                                                                                                                                                                                                                                                                           |
| inter  | close_raw    | 时序 | itf:cn_stock_real_bar1d                                                            | `cn_stock_real_bar1d.close` (不复权真价)                                                                                                                                                                                                               | [元/股, 不复权真价]; PIT-immutable (历史不随除权动作改写), 与 `limit_price` / `total_shares` 同口径; 给 `mcap_raw` / `limit_up` / `limit_dn` / `low_p` / `cs_close` 等"估值/物理量"用 (真市值 / 真涨跌停判定 / 真低价股). `adjust_factor` 由 PitPool 内部携带, 仅 `daily_return` 内部用                     |
| inter  | daily_return | 时序 | itf:cn_stock_real_bar1d (close + adjust_factor)                                    | `(close[d]·adjust_factor[d]) / (close[d-1]·adjust_factor[d-1]) - 1` (后复权链式; 内部从 PitPool 直读, 不依赖 tensor close_raw)                                                                                                                         | [ratio]; **后复权链式 = 含分红再投入的真持有收益**, 除权日 close 真跳 + af 反向跳 ⇒ close·af 平滑无负跳, 仅含日内真实涨跌. 前复权不 causal (会用未来除权事件追溯调整起点价), 不采用. 复权属"特征内部细节", 不暴露 tensor 顶层. `d==0` 或前一日 close/af 非 finite/0 → NaN. 下游 benchmark = pool 内等权均值 |
| inter  | up_lim       | 时序 | itf:cn_stock_limit_price                                                           | `cn_stock_limit_price.upper_limit[d-1]` (未复权, **内部主动 -1**)                                                                                                                                                                                      | [元/股]; 内部 -1 偏移对齐: tensor 顶层所有 itf 已统一 CUTOFF 到 "T 当日可见"; close_raw[D]=D-1 实际收盘, 需与 D-1 适用涨跌停比较才能判 "D-1 日是否封板", 故 `ts_up_lim` 取 `limit_price[d-1]` (复权口径与 close_raw 一致). `d==0` → NaN                                                                     |
| inter  | dn_lim       | 时序 | itf:cn_stock_limit_price                                                           | `cn_stock_limit_price.lower_limit[d-1]` (未复权, **内部主动 -1**)                                                                                                                                                                                      | [元/股]; 同 `up_lim` 偏移逻辑                                                                                                                                                                                                                                                                               |
| inter  | susp         | 时序 | itf:cn_stock_status                                                                | `cn_stock_status.suspended == 1`                                                                                                                                                                                                                       | [bool]; 当日是否停牌                                                                                                                                                                                                                                                                                        |
| inter  | share_raw    | 时序 | itf:cn_stock_shares                                                                | `cn_stock_shares.total_shares`                                                                                                                                                                                                                         | [股]                                                                                                                                                                                                                                                                                                        |
| inter  | mcap_raw     | 时序 | close_raw, share_raw                                                               | `close_raw × share_raw`                                                                                                                                                                                                                                | [元, **真市值**]; 用 `close_raw` 真价 × 当日 `total_shares` ⇒ 当日真实总市值. 不用 `close_hfq × shares` (除权日 hfq close 已下调而 shares 也变, 二者乘积非真市值)                                                                                                                                           |
| inter  | fmcap_raw    | 时序 | close_raw, itf:cn_stock_shares                                                     | `close_raw × cn_stock_shares.total_float_shares`                                                                                                                                                                                                       | [元, **真流通市值**]; 同 `mcap_raw` 真值原则                                                                                                                                                                                                                                                                |
| inter  | pe_raw       | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                         | `mcap_raw / ttm.net_profit_to_parent_shareholders_ttm` (取 shift=0 latest visible)                                                                                                                                                                     | [ratio]; ttm12; 支持负 PE (分母 < 0 不剔). 取 ttm_shift `shift=0` 行 (= 该 visible_date BigQuant 自定义"最新报告期"), 沿 v 单调推进取最新; 分母 == 0 → NaN                                                                                                                                                  |
| inter  | pb_raw       | 时序 | mcap_raw, itf:cn_stock_financial_balance_general_pit                               | `mcap_raw / balance.total_owner_equity` (取 latest report_date 的 latest visible 行)                                                                                                                                                                   | [ratio]; ttm3 (MRQ); 用含少数股东 `total_owner_equity` (≠ parent equity; 实测 BigQuant `cn_stock_valuation.pb` 用这个口径 — CATL 11% 误差 / 茅台 3-5% 误差 排查得). 同 visible_date 多 report_date 取 max(report_date); 分母 ≤ 0 → NaN                                                                      |
| inter  | ps_raw       | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                         | `mcap_raw / ttm.total_operating_revenue_ttm` (shift=0 latest visible)                                                                                                                                                                                  | [ratio]; ttm12; 用 `total_operating_revenue_ttm` (含利息/保费, ≠ `operating_revenue_ttm`; 实测 BigQuant 茅台 2% 误差 排查得). 分母 ≤ 0 → NaN                                                                                                                                                                |
| inter  | dy_raw       | 时序 | itf:cn_stock_dividend, share_raw, mcap_raw                                         | `Σ(dividend.cash_after_tax for ex_date ∈ (D − 365d, D]) × share_raw[D] / mcap_raw[D]`                                                                                                                                                                  | [ratio]; ttm12; **窗口走 `ex_date` 日历日** (= 持有人当日已落袋); 累计每股分红 × **当前股本** (∝ T 当日 total_shares, 复现 BigQuant `dividend_yield_ratio` 0% 误差通过). 无事件 → 0 (非 NaN); mcap 缺/0 → NaN                                                                                               |
| inter  | pcf_raw      | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                         | `mcap_raw / ttm.net_cffoa_ttm` (shift=0 latest visible)                                                                                                                                                                                                | [ratio]; ttm12; 经营现金流可负 (烧钱企业 → 负 PCF), 不剔; 分母 == 0 → NaN                                                                                                                                                                                                                                   |
| inter  | roe_raw      | 时序 | itf:cn_stock_financial_ttm_shift, itf:cn_stock_financial_balance_general_pit       | `ttm.net_profit_to_parent_shareholders_ttm / balance.total_equity_to_parent_shareholders × 100`                                                                                                                                                        | [%]; ttm12; 经典 ROE = shareholder return on equity, 分子分母都用归母口径 (与 PB 的 total_owner_equity 不同; PB 跟 BigQuant 实测口径, ROE 跟教科书). 分母 ≤ 0 → NaN                                                                                                                                         |
| inter  | roa_raw      | 时序 | itf:cn_stock_financial_ttm_shift, itf:cn_stock_financial_balance_general_pit       | `ttm.net_profit_to_parent_shareholders_ttm / balance.total_assets × 100`                                                                                                                                                                               | [%]; ttm12; 分子取归母 NI (与 ROE 一致, 便于横向比较), 分母用总资产; 分母 ≤ 0 → NaN                                                                                                                                                                                                                         |
| inter  | rev_raw      | 时序 | itf:cn_stock_financial_ttm_shift                                                   | `ttm.total_operating_revenue_ttm` (shift=0 latest visible)                                                                                                                                                                                             | [元]; ttm12; 与 `ps_raw` 同源 (含利息/保费); 给 `revenue_st` 过滤用                                                                                                                                                                                                                                         |
| inter  | ni_raw       | 时序 | itf:cn_stock_financial_income_general_pit                                          | `mean(latest 2 annuals.net_profit_to_parent_shareholders)` if N ≥ 2 else `latest 1` (`fs_quarter_index == 4` 过滤)                                                                                                                                     | [元]; 严格只取年报 (`fs_quarter_index == 4`); 同 report_date 多版本取 latest visible (PIT 修正语义); 给 `dividend_st` 阈值用. 2 条平均稳定阈值; 0 条 → NaN                                                                                                                                                  |
| inter  | list_age     | 时序 | meta:list_date                                                                     | `D − meta.list_date` if `D ≥ list_date` else NaN                                                                                                                                                                                                       | [日历日]; 仅上市当日及之后写值 (上市当日=0), 否则 NaN; 下游 `is_finite` 判"已上市"                                                                                                                                                                                                                          |
| inter  | delist_age   | 时序 | meta:delist_date                                                                   | `D − meta.delist_date` if `D ≥ delist_date` else NaN                                                                                                                                                                                                   | [日历日]; 仅退市当日及之后写值 (退市当日=0), 否则 NaN; 下游 `is_finite` 判"已退市"                                                                                                                                                                                                                          |
| inter  | is_margin    | 时序 | itf:cn_stock_margin_trading_detail                                                 | `1.0 if itf:cn_stock_margin_trading_detail (D, A) 存在 else 0.0`                                                                                                                                                                                       | [bool]; 当日是否融资融券标的                                                                                                                                                                                                                                                                                |
| inter  | mr_bal_raw   | 时序 | itf:cn_stock_margin_trading_detail                                                 | `cn_stock_margin_trading_detail.financing_balance`                                                                                                                                                                                                     | [元]; 融资余额; per-A grid post_ffill                                                                                                                                                                                                                                                                       |
| inter  | ms_bal_raw   | 时序 | itf:cn_stock_margin_trading_detail                                                 | `cn_stock_margin_trading_detail.securities_lending_balance`                                                                                                                                                                                            | [元]; 融券余额; 同上                                                                                                                                                                                                                                                                                        |
| inter  | industry_l1  | 时序 | itf:cn_stock_industry_component, itf:cn_stock_industry_change                      | base = 最近一份月初 `cn_stock_industry_component WHERE industry='sw2021'` 取 `industry_level1_name` → SW2021 一级行业 ID 广播; 月内累加 `cn_stock_industry_change WHERE industry='sw2021' AND industry_level=1 AND change_flag=1` 事件 (写入新行业 ID) | [uint8 ID, 存为 float]; 0=未知 / 1..31 = SW2021 一级行业 (映射表见 `cpp/include/feature/industry.hpp::SW2021_L1_NAMES`); per (D, A); 月初快照 + 月内增量回放, 上市前/无事件期保持 0                                                                                                                         |
| inter  | low_p        | 时序 | close_raw                                                                          | `close_raw < 1.0`                                                                                                                                                                                                                                      | [bool]                                                                                                                                                                                                                                                                                                      |
| inter  | low_mc       | 时序 | mcap_raw, meta:list_sector                                                         | `mcap_raw < (5e8 if meta.list_sector == 1 else 3e8)`                                                                                                                                                                                                   | [bool]; 主板判定 inline `meta.list_sector[a] == 1`                                                                                                                                                                                                                                                          |
| inter  | limit_up     | 时序 | close_raw, up_lim                                                                  | `close_raw ≥ up_lim − 1e-4`                                                                                                                                                                                                                            | [bool]; 策略涨停判定                                                                                                                                                                                                                                                                                        |
| inter  | limit_dn     | 时序 | close_raw, dn_lim                                                                  | `close_raw ≤ dn_lim + 1e-4`                                                                                                                                                                                                                            | [bool]; 策略跌停判定                                                                                                                                                                                                                                                                                        |
| inter  | pool_b       | 时序 | susp, is_margin, delist_age, meta:exchange, meta:list_sector, industry_l1          | `meta.exchange ∈ POOL_EXCHANGE_WHITELIST ∧ meta.list_sector ∈ POOL_LIST_SECTOR_WHITELIST ∧ industry_l1 ∈ POOL_INDUSTRY_L1_WHITELIST ∧ ¬susp ∧ ¬is_finite(delist_age) ∧ (true if POOL_INCLUDE_MARGIN else ¬is_margin)`                                  | [bool]; basic pool, 白名单 + 开关集中在 `config.hpp`. exchange 白名单走中文全称匹配 cn_stock_basic_info.exchange ("上海证券交易所"/"深圳证券交易所"/"北京证券交易所"); list_sector 走 int8 集合 (默认 {1}=主板); industry_l1 启动期一次性把中文白名单转 SW2021 ID mask, 运行期 inline 查                    |
| inter  | pool         | 截面 | pool_b, mcap_raw                                                                   | `pool_b ∧ rank(mcap_raw asc) ≤ POOL_UNIVERSE_SIZE` (per D, within `pool_b`; 默认 100)                                                                                                                                                                  | [bool]; 排名母集 (pct_rank 母集 + nth-smallest 母集)                                                                                                                                                                                                                                                        |
| inter  | tradable     | 截面 | pool, profit_st, revenue_st, dividend_st, trading_st, risk_warn, new_list          | `pool ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)`                                                                                                                                                                   | [bool]; 选股母集 (策略实际可买入的 a). pool 不变以稳定 factor pct_rank 口径; 下游策略每日 top-K 在 tradable 内挑                                                                                                                                                                                            |

**涨跌停交易约束** (下游策略, 不在张量内):
- 物理约束 (做不到): 涨停日不买入 / 跌停日不卖出.
- 策略主动 (业务选择): 涨停日不卖出 (赌 T+1 超额) / 跌停日不买入 (避 T+1 风险).

## 构建流水线 (data → Tensor)

`feature::build()` 串 4 phase 全过程式; 入口 `cpp/src/feature/build.cpp`.

**Phase 切分动机**

| phase  | 数据形态          | 任务粒度 | 并行性             | 主要工作                                            |
| ------ | ----------------- | -------- | ------------------ | --------------------------------------------------- |
| 0 axes | 标量级元数据      | 主线程   | 无                 | 一次性确定 D / A / per-A 静态                       |
| 1 load | itf raw aggregate | itf ≈ 12 | cache + replay     | dayfile 聚合 cache + **PIT cutoff 落到 row D 索引** |
| 2 时序 | 列式 (per-A 全 D) | a ≈ 5500 | embarrassingly (A) | 单调时间序列计算 + 状态机                           |
| 3 截面 | 行式 (per-D 全 A) | d ≈ 2750 | embarrassingly (D) | 截面归一 + universe 选取                            |

**设计原则** (业务密集化 + 性能选择, 改字段表/计算图不动外层):
- **agnostic 外层 + 单点真理**: `pit.cpp` (itf 维, 每 itf 一组 `{prealloc, aggregate, replay, post_sort?, post_ffill?}` + `ITFS[]` 表挂载), `feature.cpp` (feature 维, 每 feature 一个 `ts_xxx` / `cs_xxx` + `FEATURES[]` 表挂载); 外层 flow (`load.cpp` / `ts.cpp` / `cs.cpp` / `build.cpp`) 仅通过函数指针表迭代调度, 不出现任何具体 itf 名 / feature 名.
- **aggregate 与 PIT/张量解耦**: `aggregate/<itf>.bin` 只缓存 dayfile JSON 解析后的 raw rows, hash 仅依赖该 itf 的 dayfile `relpath + size + mtime`; 不依赖 `Axes` / `PitPool` / `Tensor` / feature/factor. 张量表和特征计算频繁改时, dayfile 不变即可复用 aggregate.
- **PIT cutoff 在 Phase 1 replay 一次性消化**: `replay` 内 `row = v_idx - itf::CUTOFF` 直接定位行 D, 写完后 `pool[a, d]` 即 "T 当日合法可见数据". Phase 2/3 不再做任何时间偏移 — 杜绝下游漏算 cutoff 导致的未来数据泄漏.
- **F 枚举顺序 = 计算顺序 = 隐式 topo sort**: 调度器 (`ts.cpp` / `cs.cpp`) 仅按 `FEATURES[]` 索引顺序串行调; 只要 "新 feature 加在其依赖之后", 后段直接读 `T.ts_row(prior_f, a)` 即可, 无需运行时 topo / 依赖锁.
- **网格无锁 + 事件 per-A 锁**: 网格 itf 因 `(a, v_idx)` slot 唯一 → 完全无锁写; 事件 itf 多对一 emplace, 锁粒度精到 `vector<mutex>(n_a)` (非全局, 非 per-itf), 接近无争用.
- **F 段独立 A*D layout (a-major / d-minor)**: Phase 2 的 `ts_row(f, a)` 是连续 span (cache friendly, 主路径); Phase 3 的 `gather/scatter_cs_row(f, d)` 是 stride-D copy (3 buffer 复用, 一次性付出).

```text
Phase 0 axes  (主线程; axis.cpp + tensor.cpp)
  # 形态: 标量级, 串行. 后续所有 phase 共用的索引基线 — 只跑一次, 无并行收益可言.
  axes ← load_axes()
    D ← read data/_meta/trading_days.json, 取 market_code='CN' 的 date 升序去重
    A ← read data/_meta/cn_stock_basic_info.json, 取全量 instrument (含已退市) 升序
    + 反向索引 date_idx / code_idx, sys_days 缓存 date_days
    floor_date(d) = max{i : dates[i] ≤ d}    # 周末/节假日 visible_date 自动落到上一交易日
  meta ← load_stock_meta(axes)               # per-A 静态: name / list_date / delist_date /
                                             #   list_sector (int8: 1=主板/2=创业板/3=科创板/4=北交所) /
                                             #   exchange (中文全称, 与 cn_stock_basic_info.exchange 一致)
                                             #   industry_l1 是时变, 见 Phase 2 inter feature, 不入 meta
  T    ← Tensor(axes)                        # F 段独立 A*D float, NaN 初始化, a-major / d-minor
                                             #   ts_row(f,a) = 连续 D span (Phase 2 主路径)
                                             #   gather/scatter_cs_row(f,d) = stride D copy (Phase 3 入口)

Phase 1 PIT load  (per-itf aggregate cache; load.cpp 通用 flow + pit.cpp 单点 itf 表)
  # 形态: dayfile JSON → aggregate/<itf>.bin raw rows → replay 到 PitPool.
  # 关键: aggregate 只缓存原始行, 不依赖 Axes/PitPool/Tensor/feature; cutoff 在 replay 一次性落到 row 索引.
  # 并发: replay 阶段网格 itf 写入完全无锁 (slot 唯一); 事件 itf 仅 per-A mutex 锁 emplace, 争用接近 0.
  # 编码: miss 时走 yyjson_read 解析 dayfile; hit 时直接读二进制 raw rows.
  #       itf.aggregate / itf.replay 在自身 namespace 决定字段映射, 外层 flow 不感知 schema.

  for itf in pit.cpp::ITFS[]:                # 仅迭代 ITFS[] 表, 不出现具体 itf 名
    itf.prealloc(axes, pool)                 # 网格: 字段 vector A*D NaN/0; 事件: EventStore[A] 空链

  for itf in ITFS[]:
    files ← enumerate data/YYYY/MM/DD/<itf.file_name>.json
    hash  ← H(relpath, size, mtime) over files
    rows  ← read aggregate/<itf.file_name>.bin if hash hit
             else parse json dayfiles via itf.aggregate(arr, day, rows), then persist aggregate
    itf.replay(rows, axes, pool, mu_or_null)
                                             # row = axes.floor_date(row.day) - itf::CUTOFF
                                             #   (CUTOFF=0 → row=v_idx; CUTOFF=-1 → row=v_idx+1; row≥n_d 越界 skip)
                                             # 网格: mu=nullptr; pool.<itf>.<field>[a*n_d + row] 无锁写
                                             # 事件: mu[a] 锁后 pool.<itf>[a].emplace_back(Ev{v=row,…})

  for itf in ITFS[] where itf.post_sort:     # 事件 itf 末段 sort by v 升序 — 给 Phase 2 单调指针扫
    itf.post_sort(pool)

  apply_meta_overlays(axes, pool)            # hybrid 伪装收尾 (必须在 post_ffill 之前):
                                             #   读 data/_meta/cn_stock_static_data.json (真盘前 09:00 快照)
                                             #   把 suspended / st_status 2 字段填充到
                                             #   row = axes.n_d()-1 (= last_d, 实盘当日).
                                             #   仅写 row=last_d 一行, 历史天不动 ⇒ "填充而非覆盖".
                                             #   _meta 不存在 ⇒ silent noop (历史回测兜底).

  for itf in ITFS[] where itf.post_ffill:    # 网格 itf per-A forward fill (停牌期间继承前值)
    itf.post_ffill(axes, pool)

Phase 2 时序  (per-A 并行; ts.cpp 通用 flow + feature.cpp 单点 feature 表)
  # 形态: 列式 (per-A 全 D), A 维 embarrassingly parallel; D 内强 causal (滚动/状态机).
  # 不变量: 每 worker 独占一段 ts_row(*, a), 无写冲突; FEATURES[] TS 段顺序 = 计算顺序,
  #         后段直读 T.ts_row(prior_f, a) (例: low_p 读 close_raw; revenue_st 读 rev_raw).
  # NaN 策略: 不人为补 0 — 上市前/退市后/长期停牌的 NaN 自然流到下游, 让数据可用性显式可判.

  parallel for a in [0, n_a) (n_threads = core_count):
    for f in FEATURES[] where axis == TimeSeries:
      f.compute_ts(a, axes, pool, meta, T)   # 写 T.ts_row(F::self, a); 读 PitPool / StockMeta / 前段 T.ts_row

  # FEATURES[] 索引 = F 枚举值 = 计算顺序; enum 大段仅 TS / CS, 段内对仗如下:
  #   raw 网格      (PitPool dense 直读, pool[a, d] 已是 row D 合法数据):
  #     close_raw   ← cn_stock_real_bar1d.close                              (不复权真价 [元/股]; PIT-immutable)
  #                   (PitPool 同时携带 adjust_factor, 不入 tensor 顶层, 仅 ts_daily_return 内部用)
  #     share_raw   ← cn_stock_shares.total_shares[a, d]                    (单位股)
  #     mcap_raw    ← close_raw × share_raw                                 (单位元, 真市值)
  #     fmcap_raw   ← close_raw × cn_stock_shares.total_float_shares[a, d]   (真流通市值)
  #     up_lim / dn_lim ← cn_stock_limit_price.upper_limit / lower_limit[a, d-1]  (内部主动 -1, 对齐 close_raw[D]=D-1)
  #     susp        ← cn_stock_status.suspended[a, d] == 1
  #     is_margin   ← cn_stock_margin_trading_detail[a, d] 存在性
  #     mr_bal_raw  ← cn_stock_margin_trading_detail.financing_balance[a, d]
  #     ms_bal_raw  ← cn_stock_margin_trading_detail.securities_lending_balance[a, d]
  #     risk_warn   ← pool.status.st_status[a, d]                            (派生 4 态 0/1/2/3; 每日快照, 不需回放; 详见 §字段表)
  #     industry_l1 ← 最近月初 cn_stock_industry_component WHERE industry='sw2021' 取 industry_level1_name
  #                   并按 cn_stock_industry_change 月内累加
  #   raw 财务 (TBD)  ← cn_stock_financial_ttm_shift (shift=0)
  #                  + cn_stock_financial_balance_general_pit (latest shift)
  #                  + cn_stock_financial_income_general_pit (年报筛选)
  #                  + cn_stock_dividend (12M sum)
  #     pe_raw / ps_raw / pcf_raw / dy_raw / pb_raw / roe_raw / roa_raw / rev_raw / ni_raw
  #   raw meta 派生 (per-A 动态: 每天 +1; PIT — 仅在事件日及之后写值, 否则 NaN):
  #     list_age   ← date_days[d] − parse(meta.list_date[a])   if D ≥ list_date   else NaN
  #     delist_age ← date_days[d] − parse(meta.delist_date[a]) if D ≥ delist_date else NaN
  #   derived       (T 内依赖):
  #     daily_return ← (close[d]·af[d]) / (close[d-1]·af[d-1]) - 1           # 后复权链式; 内部从 PitPool 读 close+adjust_factor
  #                                                                         # (含分红再投入的真持有收益; 除权日平滑无负跳)
  #                                                                         # d==0 或前一日 close/af NaN/0 → NaN
  #     low_p        ← close_raw < 1.0                                       # 真低价股 (用真价)
  #     low_mc       ← mcap_raw < (5e8 if meta.list_sector[a] == 1 else 3e8) # 真市值阈值
  #     limit_up / limit_dn ← close_raw vs up_lim / dn_lim                   # 真价 vs 真涨跌停 (二者同未复权口径)
  #   filter        (state machine; ts.hpp::state_machine_intervals 模板):
  #     profit_st   ← OR over { forecast 触发 → off=min(income_general_pit 同 report_date, ceil(Y+1,4,30)) }
  #                   on_d=trigger.v, 区间 [on_d, off_d) 写 1
  #     revenue_st  ← forecast 触发 → off=min(income_general_pit 同 report_date, ceil(Y+1,4,30)),
  #                   区间内再叠 (meta.list_sector == 1 ∧ rev_raw < threshold(end_date.Y))
  #     dividend_st ← 阶梯 forward fill: 每 dividend event 重算 3y_sum (累加历史 events with
  #                     report_date.Y ∈ [publish_y-3, publish_y-1] 的 cash_after_tax × share_raw[event.v]),
  #                     仅 meta.list_sector==1 (主板); 区间 [e.v, next.v) 内按 (ni_raw>0 ∧ 3y_sum 双阈) 写 1.
  #                     warmup_d = max(axes 起点+3y, list_date+3y), 之前一律 0 (3y 窗口不完整, 不偏严).
  #     trading_st  ← rolling 20D over (low_p ∨ low_mc).all()                # 单调计数
  #     new_list    ← is_finite(list_age) ∧ list_age < 60
  #   pool (TS):
  #     pool_b   ← (meta.exchange ∈ config::POOL_EXCHANGE_WHITELIST)        # 中文全称 string
  #                ∧ (meta.list_sector ∈ config::POOL_LIST_SECTOR_WHITELIST) # int8 集合, {1}=仅主板
  #                ∧ (industry_l1[a, d] ∈ ID-mask of POOL_INDUSTRY_L1_WHITELIST)  # 时变, mask 启动期一次性建
  #                ∧ ¬susp ∧ ¬is_finite(delist_age)
  #                ∧ (true if config::POOL_INCLUDE_MARGIN else ¬is_margin)

Phase 3 截面  (per-D 并行; cs.cpp 通用 flow + feature.cpp 单点 feature 表)
  # 形态: 行式 (per-D 全 A), D 维 embarrassingly parallel; A 内强 causal (winsor/z/rank).
  # 入口代价: gather_cs_row 是 stride-D copy (vs Phase 2 ts_row 连续 span); 每 worker
  #          thread-local 3 buffer (length=n_a) 复用, 避免反复分配.
  # 不变量: FEATURES[] CS 段顺序 = 计算顺序 (tradable 在最后, 读 pool + 6 filter).

  parallel for d in [0, n_d) (n_threads = core_count):
    bufs ← thread-local {a, b, c}: 3 × vector<float>(n_a)
    for f in FEATURES[] where axis == CrossSection:
      f.compute_cs(d, axes, T, bufs)         # 写 T.scatter_cs_row(F::self, d, …)

  # CS 业务一览 (实现见 feature.cpp::impl::cs_*):
  #   factor 流水: cs.hpp::factor_pipeline(d, src, dst, invert, T, buf):
  #     buf[A] ← T.gather_cs_row(src, d)
  #     if invert: buf[a] ← 1/buf[a]         # NaN 或 0 → NaN; 价/估值类 invert=true (越小越优), 收益类 invert=false
  #     buf ← winsor_mad(buf, k=3)           # 截断到 [med ± k·MAD]; 全等/<2 finite → 跳过
  #     buf ← z(buf)                         # (x − mean)/std, 跳 NaN; var≤0 → 跳过
  #     buf ← pct_rank(buf)                  # 升序百分位 ∈ [0,1], 同值平均秩, 跳 NaN
  #     T.scatter_cs_row(dst, d) ← buf
  #   close (← close_raw, invert), mcap, fmcap, pe_ttm12, pb_ttm3, ps_ttm12, pcf_ttm12 (全 invert)
  #   roe_ttm12, roa_ttm12, dy_ttm12 (无 invert)
  #   pool: cands ← {a : pool_b[a,d]=1 ∧ is_finite(mcap_raw[a,d])}
  #         pool[a,d] ← 1.0 if a ∈ nth_smallest(cands by mcap_raw, UNIVERSE_SIZE) else 0.0
  #   tradable: pool[a,d] ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)[a,d]
```

并发模型规格 (动机/不变量已在各 Phase 头部展开, 此处仅列数据 + 同步点)
- Phase 0: 主线程; 全量 in-memory; 跑一次.
- Phase 1: per-itf aggregate cache; cache miss 才扫 dayfile + parse JSON, cache hit 直接 replay raw rows; 网格 `mu=nullptr`, 事件 `vector<mutex>(n_a)`; 末段 `post_sort` / `post_ffill` 单线程串行.
- Phase 2: 任务数 ≈ n_a (5500); 每 worker 独占 `T.ts_row(*, a)`.
- Phase 3: 任务数 ≈ n_d (2750); 每 worker 独占 `cs_row` 段 + thread-local 3 buffer (length=n_a).
- 同步点: 仅 phase 间硬屏障 (`build.cpp` 顺序 `join` + `misc::Timer` 报段时), phase 内无屏障.

## 增减用法 (改计算图 / 字段表)

新增/修改/删除一个 itf:
1. `cpp/include/api/{bigquant,tushare}/spec.hpp`: 在 `SPECS[]` 末尾追加 spec (BigQuant 的 `TableSpec{name, visible_date, FetchKind, FetchFreq, Category, pk}` / Tushare 的 `InterfaceSpec`).
2. `cpp/include/feature/pit.hpp`: 加/改 typed `Grid<…>` / `<…>Ev` struct, 在 `PitPool` 加成员.
3. `cpp/src/feature/pit.cpp`: 加 `namespace itf_<name> { prealloc, aggregate, replay, [post_sort, post_ffill] }` 一组 dense block; `aggregate` 从 `yyjson_val arr` 读 raw rows, `replay` 基于当前 `Axes`/cutoff 写 `PitPool`.
4. `cpp/src/feature/pit.cpp`: `ITFS[]` 末尾追加一行.
   外层 `load.cpp` / `build.cpp` 不动.

新增/修改/删除一个 feature:
1. `cpp/include/feature/feature.hpp`: 在 `F` 枚举对应位置加一行 (位置 = 计算顺序; 后于其依赖).
2. `cpp/src/feature/feature.cpp`: 在 `namespace impl` 加 `ts_<name>` (签名 `TsComputeFn`) 或 `cs_<name>` (签名 `CsComputeFn`).
3. `cpp/src/feature/feature.cpp`: `FEATURES[]` 对应位置加一行 `{name, kind, axis, &impl::ts_xxx | nullptr, &impl::cs_xxx | nullptr}`.
   外层 `ts.cpp` / `cs.cpp` / `build.cpp` 不动. 依赖通过 enum 顺序保证 (无需 topo sort).

通用 kernel (跨 feature 共用): `feature/ts.hpp` 暴露 `state_machine_intervals<TEv>` 模板; `feature/cs.hpp` 暴露 `winsor_mad / z / pct_rank / factor_pipeline`. 大多数新 factor 一行 `factor_pipeline(d, F::xxx_raw, F::xxx, invert, T, b.a)` 即可.
