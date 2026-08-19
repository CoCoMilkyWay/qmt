Motive: 实盘量化交易; 国金证券 QMT 客户端下单; 全市场张量因子选股, 数据按 `visible_date` 归月落本地 parquet, 数据通道 BigQuant DAI + Tushare HTTP.

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
│   │   ├── config.hpp               # 全局常量 (BigQuant host/token, Tushare host/token, update 开关, 起始日, lookback, 去重窗口)
│   │   ├── misc/                    # 通用工具 (date / fs / parquet / schedule / logging / npy / mmap / progress / timer / affinity)
│   │   │                              # fs.hpp:      git_root / read_file_all / atomic_write / atomic_write_json
│   │   │                              # parquet.hpp: 统一 parquet 存储层 — month_path / meta_path / list_month_files /
│   │   │                              #              read_table / write_table_atomic (zstd + tmp+rename) /
│   │   │                              #              TableView 类型化列访问 (pipeline / pit / axis / backtest 共用)
│   │   │                              # schedule.hpp: plan_months 月度调度器 (bigquant + tushare 共用;
│   │   │                              #              关月冻结 + 开放月水位增量) + meta_fresh (_meta 单文件判定)
│   │   ├── package/yyjson/          # JSON 库 (tushare 响应解析 + output/meta.json)
│   │   ├── package/arrow/           # Arrow Flight + Parquet (从 pyarrow vendor, 见 vendor_from_pyarrow.sh)
│   │   ├── api/                     # 数据接入子系统 (数据入); 两侧 API 完全对仗
│   │   │   ├── bigquant/            # 26 张表 DAI Arrow Flight (明文 gRPC, Basic Token → JWT)
│   │   │   │   ├── dai.hpp          # DaiClient (lazy Flight + query → arrow::Table; quota → 周配额 Quota)
│   │   │   │   ├── spec.hpp         # SPECS + fetch(client, spec, start, end) (SQL 模板内置)
│   │   │   │   └── pipeline.hpp     # update: plan_months → fetch(月) → 月 parquet; Static/Snapshot → _meta parquet
│   │   │   └── tushare/             # 3 张事件表 (forecast / express / disclosure) HTTP+JSON
│   │   │       ├── http.hpp         # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│   │   │       ├── spec.hpp         # SPECS (day_params 判别 range / per-day; drop_fields 防未来信息泄漏)
│   │   │       ├── parse.hpp        # 响应 JSON → arrow::Table (列类型推断 + drop_fields 剥离)
│   │   │       └── pipeline.hpp     # update: plan_months → fetch_month → 月 parquet (与 bigquant 对仗); probe: 积分门槛探针
│   │   ├── feature/                 # 特征子系统头文件 — 计算图 (无中心枚举, 见 §特征系统)
│   │   │   ├── graph.hpp            # FeatureSpec (节点唯一身份) + consteval 可达性/拓扑排序/环检测
│   │   │   ├── registry.hpp         # 计算图挂载点: FRAMEWORK_ROOTS (CMake 从 def/basic/ 自动生成) + 全部策略引用 → consteval 推出 ALL_NODES/TS_ORDER/CS_ORDER
│   │   │   ├── report.hpp           # print_dependency_table(): 运行期打印全量特征依赖树 (含 active 标记) + 各策略 filter/factor 概览
│   │   │   ├── tensor.hpp           # Tensor 容器 (按 ALL_NODES 顺序存储, unordered_map<FeatureSpec*,int> 查行号)
│   │   │   └── def/                 # 每节点一个 header-only 文件 (文件名 = 节点名 = FeatureSpec 变量名前缀)
│   │   │       ├── basic/           # 框架固定根依赖的市场微观结构数据 (成交价/涨跌停/停牌/退市龄/两融/行业)
│   │   │       ├── factor/          # 排序因子 (Kind::Factor) + 全部中间变量 (Kind::Inter)
│   │   │       └── filter/          # 状态机最终排除位 (Kind::Filter)
│   │   ├── strategy/                # 策略层头文件 (每策略一份小管线, 叶子指向共享图节点)
│   │   │   ├── strategy.hpp         # StrategySpec / PoolSpec / FactorWeight / SF (5 固定列) 定义
│   │   │   ├── registry.hpp         # STRATEGIES[] 挂载表 + consteval 校验
│   │   │   ├── columns.hpp          # 5 列 (pool_b/pool/tradable/score/rank) 通用计算声明
│   │   │   └── def/                 # 每策略一个 spec 文件 (白名单/filter/因子权重/回测窗口)
│   │   ├── backtest/                # 回测引擎 (per-D 状态机, per-strategy 跑一遍)
│   │   └── analysis/                # 因子诊断 (IC / turnover / 分层收益, per-strategy 跑一遍)
│   └── src/
│       ├── main.cpp                 # [pending? → preflight → bigquant::update → tushare::update] → feature::build
│       │                            #   → print_dependency_table → per-strategy {backtest::run → analysis::run} → meta.json
│       │                            # 方括号段由 config::PIPELINE_UPDATE 门控 (见 §抓取开关);
│       │                            # pending 纯本地判定全 fresh ⇒ 整段跳过 (连跑零网络)
│       ├── api/
│       │   ├── bigquant/            # dai / spec / pipeline
│       │   └── tushare/             # http / spec / parse / pipeline
│       ├── feature/                 # 通用 flow (agnostic, 不出现具体 itf/节点名) + 单点真理 pit.cpp
│       │   ├── axis.cpp             # Phase 0: load_axes + load_stock_meta (per-A 静态)
│       │   ├── pit.cpp              # 【单点真理 itf】每 itf 一个 namespace block (build + cache_layout [+ post_ffill]) + 末尾 ITFS[] 表挂载
│       │   ├── load.cpp             # Phase 1 通用 flow: 仅迭代 ITFS[] (cache mmap hit / build miss → overlay → ffill), 不出现 itf 名
│       │   ├── tensor.cpp           # Tensor 实现 (按 ALL_NODES 顺序建 index_, ts_row/gather_cs_row/scatter_cs_row)
│       │   ├── ts.cpp               # Phase 2 通用 flow: per-A 并行, 迭代 TS_ORDER 调 compute_ts; kernel 在 ts.hpp (state_machine_intervals 模板)
│       │   ├── cs.cpp               # Phase 3 通用 flow: per-D 并行, 迭代 CS_ORDER 调 compute_cs; kernel (winsor_mad / z / pct_rank / factor_pipeline) 在 cs.hpp/cpp
│       │   ├── build.cpp            # 编排入口: 串 Phase 0/1/2/2s/3/3s/4 + misc::Timer 报段时
│       │   ├── report.cpp           # print_dependency_table() 实现 (全量特征表 + active 标记, 各策略 filter/factor 概览两行, 不参与计算图构建)
│       │   └── describe.cpp         # Phase 4.x: describe() 统计 + dump_tensor() 逐点导出
│       ├── strategy/
│       │   └── columns.cpp          # Phase 2s/3s: 循环 STRATEGIES[] 算 pool_b → pool → tradable → score → rank
│       ├── backtest/
│       │   └── backtest.cpp         # per-strategy 回测器实现 (输出见 backtest.hpp 顶注)
│       └── analysis/
│           └── analysis.cpp         # per-strategy 因子诊断实现 (输出见 analysis.hpp 顶注)
├── data/                            # 落地 (全 parquet, gitignored)
│   ├── _meta/                       # 单文件全量, 每次 update 覆盖刷新 (文件 mtime 即去重时间戳)
│   │   ├── cn_stock_static_data.parquet # ★ 主 meta — 真盘前 09:00 全市场快照 (BigQuant Snapshot,
│   │   │                              #   MAX(date) 一日; 含 upper_limit/lower_limit/suspended/
│   │   │                              #   st_status/pre_close/adjust_factor/crd_buy_flag 等).
│   │   │                              #   PIT hybrid overlay 给实盘当日 (row=last_d) status 填充
│   │   │                              #   suspended/st_status (历史日月度分片已写, 不动).
│   │   └── cn_stock_basic_info.parquet # 补充 meta — axis 源 A 轴 + 真静态属性 (BigQuant Static).
│   │                                  #   list_date/delist_date/list_sector/industry 等 static_data
│   │                                  #   没有的字段; 实际盘后更新, 按 -1 滞后理解, 业务上"几乎不变".
│   ├── pool/<itf>.bin               # Phase-1 PitPool cache (POD blob, mmap hit 路径; 见 §构建流水线)
│   └── YYYY-MM/                     # 月度分片 = 数据集唯一落地形态
│       └── <name>.parquet           # 该月 visible_date ∈ [01, 月末] 的服务端响应原样 (zstd 列存)
│                                    # 0 行月也落 0 行文件 (= 拉过为空); 完整性 / 去重判定 =
│                                    # 文件存在性 + mtime + 文件内 max(vd) 水位, 无额外状态文件
│                                    #
│                                    # 27 张表 (同构月度分片):
│                                    #   BigQuant 24 (DAI Arrow Flight → arrow::Table 直落):
│                                    #     all_trading_days (axis 源 D 轴, market_code='CN', 全年提前排程), holidays,
│                                    #     cn_stock_instruments,
│                                    #     cn_stock_industry_component (★ 月初一份), cn_stock_industry_change,
│                                    #     cn_stock_industry_real_bar1d, cn_stock_industry_valuation,
│                                    #     cn_stock_capital, cn_stock_dividend, cn_stock_allotment,
│                                    #     cn_stock_margin_trading_detail, cn_stock_margin_trading_market,
│                                    #     cn_stock_shareholder, cn_stock_shares, cn_stock_status, cn_stock_suspend,
│                                    #     cn_stock_name_change, cn_stock_dragon_list, cn_stock_real_bar1d, cn_stock_limit_price,
│                                    #     cn_stock_financial_{income,cashflow,balance}_general_pit,
│                                    #     cn_stock_financial_{ttm,notes}_shift
│                                    #     (cn_stock_static_data ☆ Snapshot / cn_stock_basic_info Static 走 _meta 单文件)
│                                    #   Tushare 3 (HTTP+JSON → parse → arrow::Table 直落; 日期列为 "YYYYMMDD" string):
│                                    #     forecast, express, disclosure
│                                    #
│                                    # ★ MonthFirst: cn_stock_industry_component 由 spec.freq=MonthFirst 决定每月仅取
│                                    #   visible_date=MIN(date) 一份全行业成分快照; 月内细粒度变动靠
│                                    #   cn_stock_industry_change (Day) 增量 cover.
├── py/                              # 构建/运行模式 + 报告 (run.py 调用)
│   ├── main.py                      # CMake 配置 + 编译
│   ├── mode_{debug,profile,assert,production}.py
│   ├── report.py                    # 直读 output/*.npy + meta.json → plotly HTML 报告
│   └── app/                         # 独立运维/研究脚本 (run.py 不引用)
│       ├── api.py                   # tushare pro_api 烟雾测试
│       ├── clean.py                 # 清单张 itf 的 data/YYYY-MM/*.parquet
│       ├── meta.py                  # cn_stock_basic_info.parquet 字段分布
│       └── st.py                    # cn_stock_status st_status / risk_warn 派生分布
└── doc/
    ├── bigquant/                    # BigQuant DAI 文档 + 探测脚本
    │   ├── api.md                   # 全表目录 (中英文名 / 收费 / 更新频率 / 描述)
    │   ├── used_api.md              # 项目采用 27 张表的目录子集
    │   ├── api/                     # 各表 schema .txt (BigQuant AI Studio 导出)
    │   ├── fetch.py                 # 离线 archive 导出 (AI Studio 内跑, 产出 data/YYYY-MM parquet)
    │   ├── probe.py                 # DAI 接口探测 (单日单 instrument 收敛验证)
    │   └── schema.py                # schema dump 工具
    ├── research/                    # 数据研究脚本
    │   ├── verify_valuation.py      # 复现 BigQuant valuation 字段 (parquet PIT)
    │   └── verify_nonfinancial.py   # 非财务特征数据校验 (legacy, 走旧 JSON 路径)
    └── tushare/                     # tushare API 文档
        ├── tushare.md               # 接口索引
        ├── help/                    # 通用 trick (本地化 / HTTP 协议 / 数据库落地)
        ├── basic/                   # 基础信息
        ├── financial/               # 财务报表 (forecast / express / disclosure_date / dividend / ...)
        ├── index/                   # 指数 (申万行业 / index_member_all)
        ├── margin/                  # 融资融券
        └── quote/                   # 行情 (daily / daily_basic / adj_factor / stk_limit / suspend_d)
```

## 数据源 → 张量 (build-time PIT)

**数据通道与抓取策略** — 两路通道, 抓取规则各自一套:

- **BigQuant DAI**: Arrow Flight (`grpc+tcp://bigquant.com:17010`, 明文 gRPC + Arrow IPC RecordBatch, `query(SQL, filters)` → `arrow::Table` 零拷贝, Basic Token → JWT bearer). 5 种 FetchKind 见 `cpp/include/api/bigquant/spec.hpp`, 见下表.
- **Tushare HTTP** (`:80`, 明文 JSON POST): 三张事件表 (`forecast` / `express` / `disclosure`), 各表自带 `ann_date` 字段作 visible_date; 抓取按 `InterfaceSpec::day_params` 判别 range (空 → 月段 1 次调用) / per-day (非空 → 月内逐日).

BigQuant FetchKind (`<vd>` = `TableSpec::visible_date` — Static 为空, Partition 通常是 `date`, Where 通常是 `publish_date` / `end_date`):

| kind      | freq       | SQL 写法                                                                 | filters          | 适用                                                                                                                                         |
| --------- | ---------- | ------------------------------------------------------------------------ | ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Static    | Day        | `SELECT * FROM <name>`                                                   | `{}`             | `cn_stock_basic_info` (无 date 维)                                                                                                           |
| Partition | Day        | `SELECT * FROM <name>`                                                   | `{"date":[s,e]}` | 日频网格 (行情 / 状态 / 财务 PIT / ...)                                                                                                      |
| Partition | MonthFirst | `WHERE <vd> = (SELECT MIN(<vd>) FROM <name> WHERE <vd> BETWEEN s AND e)` | `{"date":[s,e]}` | `cn_stock_industry_component` 月初一份全行业成分, 月内变动靠 `cn_stock_industry_change` (Day) 增量 cover                                     |
| Where     | Day        | `SELECT * FROM <name> WHERE <vd> >= s AND <vd> <= e`                     | `{}`             | 事件型 (`publish_date` / `end_date` 等非分区列); `all_trading_days` (是否 date 分区未验证, WHERE 最稳且配额无差价)                           |
| Snapshot  | Day        | `WHERE <vd> = (SELECT MAX(<vd>) FROM <name> WHERE <vd> BETWEEN s AND e)` | `{"date":[s,e]}` | `cn_stock_static_data` 真盘前 09:00 全市场快照, 取窗口内 MAX(date) 一日, 落 `data/_meta/<name>.parquet` 单文件; PIT overlay 给 row=last_d 用 |

**落地** (`data/YYYY-MM/<name>.parquet` 月度分片, 全 parquet)
- 月度表: 每表每月一个文件, 内容 = 该月 `visible_date ∈ [01, 月末]` 的服务端响应原样 (zstd 列存, 行结构 / 去重语义信任服务端 PIT). BigQuant fetch 直接得 `arrow::Table`; Tushare 响应 JSON 经 `parse::docs_to_table` 转 `arrow::Table` (列类型推断, `drop_fields` 剥离); 两侧同走 `misc::pq::write_table_atomic` 落盘.
- 单文件 `_meta`: `data/_meta/{cn_stock_static_data, cn_stock_basic_info}.parquet`
  - `cn_stock_static_data` (**主 meta**, Snapshot, 真盘前 09:00): DAI 取窗口内 `MAX(date)` 一日的全市场快照, 一次响应整刷. 用于 hybrid PIT overlay 给实盘当日 (= `axes.last_d`) 的 `status` 字段填充真盘前值 (见 §cutoff). 刷新判定 `meta_fresh`: 文件内快照日 ≥ horizon(avail_hour=9) → skip ⇒ 每天 09:00 后首个触发整刷一次.
  - `cn_stock_basic_info` (补充 meta, Static, 无 date 列): DAI 一次响应整刷; 仅用于 `static_data` 没有的字段 (`list_date` / `delist_date` / `list_sector` / `industry` / ...). 实际盘后更新, 按 -1 滞后理解, 业务上字段几乎不变. 无水位可言 ⇒ 写盘日 == today → skip (日级整刷).
  - `all_trading_days` / `holidays` (axis 源): 普通月度表, `axis.cpp` 直接扫全部月 parquet 读出 D 轴 (小表, 每月 KB 级).
- **完整性设计** (BigQuant / Tushare 共用 `misc::plan_months` 单点调度; 判定全部落在"单文件存在性 + mtime + 文件内 max(vd) 水位", 无额外状态文件. 背景: DAI 配额按**返回 cell 数**计费, 与查询次数/扫描窗口无关 ⇒ 省流量 = 让重复查询返回 0 新行):
  - **关月冻结** (月末 < today − `lookback_days`): 文件存在且写盘日 ≥ 月末 + lookback → 跳过; 否则整月重拉覆盖一次后永久冻结. 这是**唯一的完整性兜底** — 月内增量漏掉的服务端回填/修订在此全部吃回.
  - **开放月水位增量** (月末仍在 lookback 窗口内, 含当月): 每表一个 `avail_hour` (spec 内声明: day X 数据于 X 日该小时后完整; 盘后批统一 20 / 真盘前 9~10 / 排程提前 0 / 全天涓流 24) ⇒ horizon = 当前已完整的最晚数据日. 水位 W = 月文件内 max(visible_date); 只拉 `vd ∈ [W+1, min(月末, horizon)]` **append** 到月文件, 已到水位连查询都不发. 稳态下每表每天只为新增一天数据付费一次; 月内完整性降一级 (漏回填), 关月兜回.
  - **节流** (连跑零网络): 文件 mtime 距今 < `PIPELINE_DEDUP_WINDOW_SECONDS` → 本表直接跳过; 0 行探测响应只 touch mtime (探测本身计入 dedup 窗). main 入口先跑两侧 `pending()` 纯本地判定, 全部 fresh ⇒ 跳过 preflight + 联网, 直接 build.
  - **tmp+rename 原子写**: 单文件替换, 中断不留半成品.
- 外部资料: 入库时机 (BigQuant `doc/bigquant/api.md` 更新时间列, 多数 17:00–20:00 盘后批发; Tushare `doc/tushare/help/数据更新说明.md` 及各 API 自身 doc); 公告披露时段 (`doc/exchange/公告类别和发布时间.md`, SSE/SZSE 各时段 + 非交易日 13–17 / 12–16 直通).

**cutoff** (build-time, 实盘/回测同一公式): 偏移 `offset(itf)` 在 build 阶段一次性消化, 张量 row D 只含 T 当日信号时点已可知的信息, 下游 (策略/回测/实盘) 直读 row D 不再处理未来数据.
- 信号时点 ≜ 交易日 T 盘中, 信号计算前 1 分钟刷库; 此后每个 itf 按 `visible_date ≤ T + offset(itf)` 切片写入 row D.
- `offset` 单位 = **日历日**, 含周末/节假日.
- 全部 BigQuant 表实际入库时间都是盘后 17:00+ (`api.md` 更新时间列实测), 项目按业务可推出性把所有 itf 归到下面两种模式之一:

| 模式              | offset | 含义                                                                                                                                                                                                                                    | 适用                                                                          |
| ----------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| **normal**        | `−1`   | 承认滞后, row D=T 取 T-1 数据. T-1 盘后公告 + T-1 至 T 之间的周末/节假日公告 (含 SSE/SZSE 非交易日直通时段) 自然划入 T 行.                                                                                                              | 绝大多数 itf — 默认安全选项                                                   |
| **hybrid (伪装)** | `0`    | 假装盘前: 历史按 row=v_idx 消化 (T 当日就用 T 当日记录); 最后一天 (= 实盘当日, 当日记录还没入库) 由 `apply_meta_overlays` 用 `cn_stock_static_data` 真盘前 09:00 快照**填充** row=last_d (仅写这一行, 历史天完全不动 ⇒ "填充而非覆盖"). | 仅 `cn_stock_status`. (`cn_stock_suspend` 同类语义但代码未消费, 见入库时机表) |

hybrid 适用判定: 该 itf 当日值在 T 当日开盘前**业务上已实际确定** (即使 API 入库晚也不会失真). 严格 PIT 下应 −1, 但 −1 会显著伤害 alpha (例: ST 翻转日整体滞后 1-2 天); hybrid 在保证因果性的同时拿回 T 当日真值:
- ✅ `cn_stock_status` (st_status / suspended): ST / 停牌当日开盘前即生效, 静态确定 → 适用 hybrid.
- ❌ `cn_stock_limit_price`: 虽然 `limit[T] = round(close[T-1] × (1±pct), 2)` 在 T-1 晚已确定, 但 ST 翻转日 pct 变化日的记录写的仍是旧 limit (T-1 是停牌日, 写的是停牌前旧 limit); 严格 PIT 安全 ⇒ 用 normal `−1` (承认滞后).
- ❌ 其他盘后 itf (industry / dividend / forecast / 财务 / ...) — 全 normal `−1`.

**入张量映射** (cutoff 后按 itf 类落 row D)
- **网格** (D=trade_date, A=instrument): 每条记录唯一 (D, A) 单元. row D 取 `max{ visible_date ≤ T + offset }` (offset=0 → 自身; offset=−1 → 上一交易日, 周末/假日 visible_date 不存在自动跳过).
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见各节点 `deps` (例: `forecast` / `income_general_pit` / `cashflow_general_pit` / `balance_general_pit` 按 `report_date`, `dividend` 按 `publish_date`, `name_change` 按 `end_date`). 状态机型 (`profit_st` / `revenue_st` / `dividend_st`) 同样按此 cutoff 回放 `visible_date` 升序流.
- **月初快照** (D, A): `cn_stock_industry_component` 月初落一份, build 时取 `max{ visible_date ≤ T }` 的快照广播到 (D, A); 月内细变动叠加 `cn_stock_industry_change` 事件流 → `industry_l1` inter feature.
- **overlay** (row=last_d 单行填充): `cn_stock_static_data` (`_meta` 单文件) → `status.{suspended, st_status}` 2 字段. 仅写 row=last_d, 历史日子完全不动. 详见 §cutoff 表里的 hybrid 模式. 快照新鲜度不变量 (快照日 == D 轴 last_d 的日期, 即"用 last_d 当日真盘前快照填 last_d 行") 仅在 `config::PIPELINE_UPDATE = true` 时断言 — 盘前/凌晨跑时当日快照未生成、last_d 也还是上一交易日, 自洽通过; 离线跑用旧快照, 见 §抓取开关.
- **asset 静态**: `cn_stock_basic_info` 全量 snapshot 广播到 (D, A); `list_date / delist_date` 决定 (D, A) 行有效, 不走 `visible_date`.
- **axis**: `all_trading_days` WHERE `market_code='CN'` (截到 today) 生成 D 轴; `cn_stock_basic_info.instrument` 全量生成 A 轴.

**一致性** (build 完成 → 张量 PIT-clean, 下游无未来数据风险)
- **跨次修正** (replay 安全): 服务端 PIT 表以新 `visible_date` 行发布修订, 历史行不改写 → 修订落进所属月的 parquet, replay 任意 T 按上述 cutoff 自动选当时可见版本. 关月冻结后不再变动; 开放月水位增量天然吃到新 `visible_date` 行的修订, 对旧日期的回填月内看不见, 关月整月重拉时吃回.
- **回测 = 实盘**: 同一份 build 代码 + 同一组 offset → 同一份 PIT 张量.
- 已知 best-effort 瑕疵 (不可消除, 接受):
  1. 公告级时间戳缺失 → 同 `visible_date` 内盘前/盘中/盘后无法区分, 统一按盘后保守 → 计入 next-day cutoff (实盘错过 T 当日盘前直通公告, 与回测一致).
  2. `cn_stock_basic_info` 仅当前 snapshot → `list_sector` / `industry` 历史变更无法回溯; 行业变更已用 `cn_stock_industry_component` (月初) + `cn_stock_industry_change` (日频) 补救, 但 `list_sector` 转板股全期按当前归类.
  3. `cn_stock_status` hybrid 伪装的隐含假设: ST / 停牌当日盘前已生效 (公告 → 停牌一日 → 复牌即生效新标识). 极端情况下 T 盘中突发停牌不会反映到 row D=T (数据盘后才入库), 信号刷库时拿到的是 T-1 终态延续, 但 `cn_stock_static_data` 真盘前 09:00 快照对实盘当日已捕获 (next-day cutoff). 想绝对保守可把 status 退回 normal `−1`.

| 类       | name                                      | 通道                | 入库时间 (api.md)    | visible_date   | 模式       | 偏移 |
| -------- | ----------------------------------------- | ------------------- | -------------------- | -------------- | ---------- | ---- |
| axis     | `all_trading_days` / `holidays`           | BigQuant            | 定期 (新年度排程)    | `date`         | axis       | —    |
| overlay  | `cn_stock_static_data`                    | BigQuant Snapshot   | **真盘前** 09:00     | `date` (MAX)   | 真盘前     | 0    |
| asset    | `cn_stock_basic_info`                     | BigQuant Static     | 每轮 update 覆盖刷新 | —              | static     | -1   |
| 网格     | `cn_stock_instruments`                    | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 月初快照 | `cn_stock_industry_component`             | BigQuant MonthFirst | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_change`                | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_real_bar1d`            | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_industry_valuation`             | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_real_bar1d`                     | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_limit_price`                    | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_status`                         | BigQuant            | 盘后 20:00           | `date`         | **hybrid** | 0    |
| 网格     | `cn_stock_suspend`                        | BigQuant            | 盘后 20:00           | `date`         | **hybrid** | 0    |
| 网格     | `cn_stock_shares`                         | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 网格     | `cn_stock_margin_trading_detail`          | BigQuant            | **真盘前** 10:00     | `date`         | 真盘前     | 0    |
| 网格     | `cn_stock_margin_trading_market`          | BigQuant            | **真盘前** 10:00     | `date`         | 真盘前     | 0    |
| 网格     | `cn_stock_dragon_list`                    | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 事件     | `cn_stock_capital`                        | BigQuant            | 盘后 20:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_dividend`                       | BigQuant            | 盘后 20:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_allotment`                      | BigQuant            | 盘后 20:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_shareholder`                    | BigQuant            | 盘后 20:00           | `publish_date` | normal     | −1   |
| 事件     | `cn_stock_name_change`                    | BigQuant            | 盘后 20:00           | `end_date`     | normal     | −1   |
| 财务 PIT | `cn_stock_financial_income_general_pit`   | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 PIT | `cn_stock_financial_cashflow_general_pit` | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 PIT | `cn_stock_financial_balance_general_pit`  | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务 TTM | `cn_stock_financial_ttm_shift`            | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 财务附注 | `cn_stock_financial_notes_shift`          | BigQuant            | 盘后 20:00           | `date`         | normal     | −1   |
| 事件     | `forecast`                                | Tushare             | 公告实时 (通常盘后)  | `ann_date`     | normal     | −1   |
| 事件     | `express`                                 | Tushare             | 公告实时 (通常盘后)  | `ann_date`     | normal     | −1   |
| 事件     | `disclosure`                              | Tushare             | 公告实时 (披露计划)  | `ann_date`     | normal     | −1   |

## 特征系统 (无中心枚举, header-only, 编译期自动挂载)

张量 `T[node, A, D]`: `D` = 交易日 (`all_trading_days` WHERE `market_code='CN'`, 截到 today; 多市场 `trading_days` 已弃用, CN 行当日盘后才入库会导致盘中缺行/水位错乱); `A` = instrument (`cn_stock_basic_info.instrument` 全量, 含已退市); `node` = 一个 `FeatureSpec` (无中心枚举, 身份 = 其 `&<name>_spec` 地址, `Tensor` 按 `ALL_NODES` 拓扑序分配 `mats[i]`, `unordered_map<const FeatureSpec*,int>` 做地址→下标查询); dtype 统一 float (bool 用 0.0/1.0); kind (与轴独立, 以 `FeatureSpec::axis` 为准) 分 `Filter` (1=排除该 D-A) / `Factor` (∈[0,1], NaN=不参与, 截面归一后的连续得分) / `Inter` (中间量, 时序或截面皆可). 每个策略额外拥有固定 5 列 (`pool_b/pool/tradable/score/rank`), 存在独立的 `Tensor.strat_mats` (不占共享节点存储位), 见 §策略层.

**单点真理落在每个节点自己的定义文件里, README 不维护副本.** 每个特征节点 (raw / 中间量 / filter / factor, 三者在计算图层面完全等价) 是 `cpp/include/feature/def/{basic,factor,filter}/<name>.hpp` 下的一个 header-only 文件, 文件名 = 节点名, 声明恰一个:

```cpp
namespace feature::def {
inline void ts_<name>(...);          // 或 cs_<name>, 依 axis 而定
inline constexpr FeatureSpec <name>_spec{
    "<name>", Kind::Inter, Axis::TimeSeries,
    <name>_deps,           // std::span<const FeatureSpec*const>, 空则无依赖
    &ts_<name>, nullptr,   // TS 填 compute_ts, CS 填 compute_cs, 另一个 nullptr
    /*must_be_finite=*/false,
    /*formula=*/"...",     // 必填, 编译期 static_assert 校验非空
    /*assumption=*/"...",  // 必填 (单位/边界条件/关键假设; 无则写 "—")
};
}
```

`&<name>_spec` (取地址) 即该节点编译期稳定的身份 (`inline` 保证跨 TU 同址), 用于三处、且**只**出现在这三处:
- 依赖声明: 被依赖方 `#include` 本文件, 在自己的 `deps` 数组里放 `&<name>_spec`
- `Tensor` 存取: `T.ts_row(<name>_spec, a)` / `T.gather_cs_row(<name>_spec, d, ...)`
- 策略引用: `StrategySpec.filters` / `FactorWeight.f` / `PoolSpec.rank_key`

**计算图完全自动推导, 无需手动注册**: `feature/registry.hpp` 从两类"根" (`FRAMEWORK_ROOTS` — 框架自身固定需要的少量 raw 节点, 与 `strategy::STRATEGIES[]` 引用到的全部节点) 出发, 用 `graph.hpp` 里的 `consteval` 函数沿 `deps` 做反向可达性 + 拓扑排序, 推出 `ALL_NODES` / `TS_ORDER` / `CS_ORDER`; 同时做环检测和"TS 节点不得依赖 CS 节点"的轴校验, 违规直接编译失败. 不在根可达闭包内的节点文件即使存在也不会进入计算 (不触发计算, 不占 `Tensor` 存储). CMake (`file(GLOB ... CONFIGURE_DEPENDS)`) 把 `def/**/*.hpp` 收进构建系统给 IDE 看, 同时也是 `FRAMEWORK_ROOTS` / 下面"全量特征清单"两份生成文件的输入源 (见下一段) —— 但真正决定"哪些节点参与计算"的仍然只是 `consteval` 沿 `deps` 的可达性推导本身, glob 只负责把"目录里有哪些文件"这个事实喂给它, 不做任何依赖/可达性判断.

`basic/` 目录例外: 里面是框架结算 / 策略白名单计算 (`backtest.cpp` / `strategy/columns.cpp`) 直接依赖的市场微观结构原始数据 (成交价/涨跌停/停牌/退市龄/两融/行业), 与具体策略无关, 是 `FRAMEWORK_ROOTS` 的来源. `FRAMEWORK_ROOTS` 本身由 `cpp/projects/main/CMakeLists.txt` 在 configure 期 glob `feature/def/basic/*.hpp` 自动生成到 `<build>/generated/feature/framework_roots.hpp` (`registry.hpp` `#include "feature/framework_roots.hpp"` 拿到的就是这份生成文件, 不再手写数组) —— `basic/` 目录内容本身即等价于框架固定根这份清单, 新增/删除 `basic/*.hpp` 后重新 cmake configure 即自动同步, 不需要手动改 `registry.hpp`. `axis` 字段: `TimeSeries` = per A 沿 D 计算 (无截面依赖, A 维可并行); `CrossSection` = per D 沿 A 计算 (有截面依赖, A 维不可并行); 其余字段 (`must_be_finite`/`formula`/`assumption`) 含义见上面代码块内注释.

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm12** (高季节性, 12 个月 ≡ 4 报告期): 取 `cn_stock_financial_ttm_shift.*_ttm` (shift=0) 或 mcap_raw / TTM 字段自算 (估值类, 支持负值).
- **ttm1** (瞬时估值 / MRQ, 最新一期 snapshot): 取 `cn_stock_financial_balance_general_pit.*` (latest) 自算; 例 pb = mcap_raw / total_owner_equity.

**依赖表格现场生成, 不在文档里维护副本**: `main.cpp` 在 `feature::build()` 之后调用 `feature::print_dependency_table()` (`config::FEATURE_TABLE_ENABLE` 门控), 运行期打印定宽表格 (kind / feature / 轴 / active / deps / formula / assumption, 每行截断到固定宽度). 表格覆盖**全部**已定义特征 (含未被任何策略引用的), 组内按 Kind (inter → filter → factor) 分桶、桶内拓扑序; `active` 列标记该节点是否在 `feature::ALL_NODES` (真正参与计算的可达闭包) 内. "全部已定义特征"清单本身也是自动生成、无需手动维护: `cpp/projects/main/CMakeLists.txt` 在 configure 期 glob `feature/def/{basic,factor,filter}/*.hpp`, 按约定 (文件名 == 节点名 == `FeatureSpec` 变量名前缀) 拼出 `#include` + `&<name>_spec` 列表, 生成到 `<build>/generated/feature/def/all.hpp` (`report.cpp` `#include "feature/def/all.hpp"` 拿到的就是这份生成文件); 它只给这张表用, 不被 `registry.hpp` 引用, 不影响可达性裁剪, 未激活的节点依然不触发计算/不占 `Tensor` 存储; 新增/删除 def 文件后重新 cmake configure 即自动同步. 表格下方再按策略打印两行 (`<策略名> filter: ...` / `<策略名> factor: ...`) 列出该策略实际使用的节点名. 想看当前全部特征的公式/假设, 直接跑一次 build 看 stdout, 或读对应 `def/**/<name>.hpp` 里的 `formula`/`assumption` 字段 (二者同源, 不可能不一致).

## 构建流水线 (data → Tensor → 策略)

`feature::build()` 串 0/1/2/2s/3/3s/4 共 7 个 phase 全过程式; 入口 `cpp/src/feature/build.cpp`; build 完成后 `main.cpp` 逐策略跑 `backtest::run` + `analysis::run`.

**Phase 切分动机**

| phase   | 数据形态          | 任务粒度       | 并行性             | 主要工作                                                      |
| ------- | ----------------- | -------------- | ------------------ | ------------------------------------------------------------- |
| 0 axes  | 标量级元数据      | 主线程         | 无                 | 一次性确定 D / A / per-A 静态                                 |
| 1 load  | itf PIT pool      | itf ≈ 12       | mmap cache + build | hit: mmap pool.bin; miss: 并行读月 parquet → 直写 pool + dump |
| 2 时序  | 列式 (per-A 全 D) | a ≈ 5500       | embarrassingly (A) | 共享图 TS_ORDER 节点: 单调时间序列计算 + 状态机               |
| 2s 策略 | 列式 (per-A 全 D) | a ≈ 5500       | embarrassingly (A) | 逐策略算 `pool_b` (依赖共享 TS 节点)                          |
| 3 截面  | 行式 (per-D 全 A) | d ≈ 2750       | embarrassingly (D) | 共享图 CS_ORDER 节点: 截面归一 (winsor/z/pct_rank/中性化)     |
| 3s 策略 | 行式 (per-D 全 A) | d ≈ 2750       | embarrassingly (D) | 逐策略算 `pool → tradable → score → rank`                     |
| 4 校验  | 逐节点/逐策略列   | ALL_NODES 规模 | 串行 (轻量)        | `must_be_finite` 契约 + 策略 5 列全 finite 断言               |

**设计原则** (业务密集化 + 性能选择, 改计算图不动外层; 同步点仅 phase 间硬屏障 [`build.cpp` 顺序 `join` + `misc::Timer` 报段时], phase 内无屏障):
- **agnostic 外层 + 单点真理**: `pit.cpp` (itf 维, 每 itf 一组 `{build, cache_layout, post_ffill?}` + `ITFS[]` 表挂载) 与 `feature/def/**/*.hpp` (节点维, 每节点一个 `FeatureSpec`); 外层 flow (`load.cpp` / `ts.cpp` / `cs.cpp` / `build.cpp`) 仅通过函数指针/`FeatureSpec*` 表迭代调度, 不出现任何具体 itf 名 / 节点名.
- **pool cache 零反序列化**: `data/pool/<itf>.bin` 是 `PitPool` 字段的紧凑 POD blob 拼接 (header + section table + raw bytes). hit 路径 `mmap(MAP_PRIVATE)` → `PoolArr.map_view` 把 PitPool 字段指针指过去 ⇒ **零 copy / 零反序列化 / 零 hash lookup** (后续 overlay / ffill 的少量写入由 OS COW 落匿名页, 不脏文件). cache key = FNV(POOL_VERSION + itf name + 该 itf 全部月 parquet `relpath/size/mtime` + axes 语义 hash (dates+codes 内容)). 月 parquet 与轴不变 ⇒ cache 永远 hit; 开放月重拉只打穿该 itf, 其他 itf 不受影响.
- **PIT cutoff 在 Phase 1 build 一次性消化**: `build` 内 `row = floor_date(visible_date) - itf::CUTOFF` 直接定位行 D, 写完后 `pool[a, d]` 即 "T 当日合法可见数据". Phase 2/3 不再做任何时间偏移 — 杜绝下游漏算 cutoff 导致的未来数据泄漏.
- **计算顺序 = 编译期拓扑序, 无运行时依赖锁**: `registry.hpp` 的 `TS_ORDER` / `CS_ORDER` 已是合法拓扑序 (见 §特征系统); 调度器 (`ts.cpp` / `cs.cpp`) 仅按该顺序串行调, 后段直接读 `T.ts_row(prior_spec, a)` 即可.
- **网格无锁 + 事件 per-A 锁**: 网格 itf 因 `(a, v_idx)` slot 唯一 → 完全无锁写; 事件 itf 多对一 emplace, 锁粒度精到 `vector<mutex>(n_a)` (非全局, 非 per-itf), 接近无争用.
- **独立 A×D layout (a-major / d-minor)**: Phase 2 的 `ts_row(spec, a)` 是连续 span (cache friendly, 主路径); Phase 3 的 `gather/scatter_cs_row(spec, d)` 是 stride-D copy (3 buffer 复用, 一次性付出); 策略块 `strat_mats` 布局相同, 各策略固定 5 列独立存储, 不占共享图节点位.
- **策略与共享图解耦**: Phase 2/3 只算图上通用节点 (raw/中间量/filter/factor), 对策略配置一无所知; Phase 2s/3s 才引入 `strategy::STRATEGIES[]`, 把每策略的 `pool_b/pool/tradable/score/rank` 算成独立的 5 列, 新增策略不影响共享图.

```text
Phase 0 axes  (主线程; axis.cpp + tensor.cpp)
  # 形态: 标量级, 串行. 后续所有 phase 共用的索引基线 — 只跑一次, 无并行收益可言.
  axes ← load_axes()
    D ← scan data/YYYY-MM/all_trading_days.parquet 全月, 取 market_code='CN' 的 date 升序去重, 截到 today
    A ← read data/_meta/cn_stock_basic_info.parquet, 取全量 instrument (含已退市) 升序
    + 反向索引 date_idx / code_idx, sys_days 缓存 date_days
    floor_date(d) = max{i : dates[i] ≤ d}    # 周末/节假日 visible_date 自动落到上一交易日
  meta ← load_stock_meta(axes)               # per-A 静态: name / list_date / delist_date /
                                             #   list_sector (int8: 1=主板/2=创业板/3=科创板/4=北交所) /
                                             #   exchange (中文全称, 与 cn_stock_basic_info.exchange 一致)
                                             #   industry_l1 是时变节点 (见 def/basic/), 不入 meta
  T    ← Tensor(axes, ALL_NODES, N_STRAT_SLOTS)  # 共享图段按 ALL_NODES 顺序建 index_ (地址→下标);
                                             #   ts_row(spec,a) = 连续 D span (Phase 2 主路径)
                                             #   gather/scatter_cs_row(spec,d) = stride D copy (Phase 3 入口)
                                             #   strat_mats 另存各策略固定 5 列

Phase 1 PIT load  (per-itf pool cache via mmap; load.cpp 通用 flow + pit.cpp 单点 itf 表)
  # 形态: 月度 parquet → (miss 时) 直写 PitPool → 落 data/pool/<itf>.bin (POD blob 紧凑拼接).
  #       hit 时直接 mmap(MAP_PRIVATE) → PoolArr.map_view 把 pool 字段指过去, 零反序列化.
  # 关键: cache 跟该 itf 的月 parquet (FNV: relpath+size+mtime) + axes 语义 hash 绑定.
  #       cutoff 在 build 一次性落到 row 索引; overlay / ffill 永远跑最新代码 (mmap COW).
  # 并发: build 按月并行 (per-月 worker); 网格 itf 写入完全无锁 (date 精确命中交易日 ⇒
  #       (a, row) slot 唯一, 跨月不相交); 事件 itf 仅 per-A mutex 锁 emplace.
  #       hit 路径单线程亚毫秒 (只建 page table); 业务首次访问由 OS readahead 并发拉页.

  for itf in pit.cpp::ITFS[]:                # 仅迭代 ITFS[] 表, 不出现具体 itf 名
    files ← enumerate data/YYYY-MM/<itf.file_name>.parquet
    key   ← FNV(POOL_VERSION, itf.file_name, axes_hash(dates+codes), [files: relpath+size+mtime])

    if mmap(data/pool/<itf>.bin) header.key == key:
        itf.cache_layout(pool, visitor=Map)  # 每段 PoolArr.data_ ← mmap_base + section.offset
        # hit: 总耗时 μs 级 — 不读字节, 不分配, 不 hash lookup
    else:
        itf.build(axes, files, pool)         # 并行读月 parquet (TableView 列访问) → 直写 pool 字段:
                                             #   row = axes.floor_date(visible_date) - itf::CUTOFF
                                             #     (CUTOFF=0 → row=v_idx; CUTOFF=-1 → row=v_idx+1; 越界 skip)
                                             #   网格: date 精确命中交易日, pool.<itf>.<field>[a*n_d + row] 无锁写
                                             #   事件: mu[a] 锁后 chain.push(Ev{v=row,…}); 全 itf 终走
                                             #         sort_chains (stable by v) + finalize (压平到 arena)
        dump_pool_cache(itf, key, pool)      # cache_layout(visitor=Write) 拼 header + table + blob → atomic_write

  apply_meta_overlays(axes, pool)            # hybrid 伪装收尾 (必须在 post_ffill 之前):
                                             #   读 data/_meta/cn_stock_static_data.parquet (真盘前 09:00 快照)
                                             #   把 suspended / st_status 2 字段填充到 row=last_d (实盘当日).
                                             #   mmap COW: 只该页被 dirty, 不脏 cache 文件.

  for itf in ITFS[] where itf.post_ffill:    # 网格 itf per-A forward fill (停牌期间继承前值)
    itf.post_ffill(axes, pool)               # 同样走 mmap COW, ffill 逻辑改不用 bump POOL_VERSION

Phase 2 时序  (per-A 并行; ts.cpp 通用 flow + registry.hpp::TS_ORDER)
  # 形态: 列式 (per-A 全 D), A 维 embarrassingly parallel; D 内强 causal (滚动/状态机).
  # 不变量: 每 worker 独占一段 ts_row(*, a), 无写冲突; TS_ORDER 已是合法拓扑序,
  #         后段直读 T.ts_row(prior_spec, a) (例: low_p 读 close_raw; revenue_st 读 rev_raw).
  # NaN 策略: 不人为补 0 — 上市前/退市后/长期停牌的 NaN 自然流到下游, 让数据可用性显式可判.
  # 具体每个节点算什么/为什么 → §特征系统 (公式/假设即代码里的 FeatureSpec::formula/assumption, 不重复贴此处).

  parallel for a in [0, n_a) (n_threads = core_count):
    for spec in TS_ORDER:
      spec->compute_ts(a, axes, pool, meta, T)   # 写 T.ts_row(*spec, a); 读 PitPool / StockMeta / 前段 T.ts_row

Phase 2s 策略 TS 列  (per-A 并行; strategy/columns.cpp::compute_ts_columns)
  # 依赖共享 TS 节点 (susp / is_margin / list_age / delist_age / industry_l1), 逐策略算 pool_b
  # (exchange/list_sector/industry_l1 白名单 ∧ 已上市 ∧ ¬susp ∧ ¬退市 ∧ margin 开关), 写 T.strat_mats.

  parallel for a in [0, n_a):
    for s in strategy::STRATEGIES:
      compute pool_b for (s, a)   # 写 T.strat_ts_row(slot(s_idx, SF::pool_b), a)

Phase 3 截面  (per-D 并行; cs.cpp 通用 flow + registry.hpp::CS_ORDER)
  # 形态: 行式 (per-D 全 A), D 维 embarrassingly parallel; A 内强 causal (winsor/z/rank).
  # 入口代价: gather_cs_row 是 stride-D copy (vs Phase 2 ts_row 连续 span); 每 worker
  #          thread-local 3 buffer (length=n_a) 复用, 避免反复分配.
  # 不变量: CS_ORDER 已是合法拓扑序. 截面参与集统一先 mask_offmarket(d) — 当日未上市
  #   (list_age NaN) 或已退市 (delist_age finite) 的 a 置 NaN, 不进 winsor 分位 / OLS / pct_rank.

  parallel for d in [0, n_d) (n_threads = core_count):
    bufs ← thread-local {a, b, c}: 3 × vector<float>(n_a)
    for spec in CS_ORDER:
      spec->compute_cs(d, axes, T, bufs)         # 写 T.scatter_cs_row(*spec, d, …)

  # 通用 CS kernel (cs.hpp, 跨 factor 共用):
  #   factor_pipeline(d, src, dst, invert, T, bufs): gather → mask_offmarket → [invert 1/x]
  #     → winsor_mad(k=3) → z → pct_rank → scatter (close/mcap/fmcap/cffoa_ttm12 等直接用)
  #   neutral_pipeline(d, src, dst, invert, T, bufs): gather → mask_offmarket → [invert] →
  #     winsorize_quantile(1%,99%) → neutralize(行业+log(mcap) OLS 残差, FWL 等价) → z →
  #     pct_rank → 均值填充 → scatter (估值/盈利 ttm 系因子用; 倒数类 pe/pb/ps/pcf invert=true)
  #   中性化口径实测对齐果仁 "中性BP/EP/CP/SP/ROE/ROA/股息率"
  #     (winsor 1%-99% + 申万一级 + log 总市值 OLS 残差, 全市场截面); 负值参与拟合.

Phase 3s 策略 CS 列  (per-D 并行; strategy/columns.cpp::compute_cs_columns)
  # 依赖共享 CS 节点 (rank_key / factor 全集) + Phase 2s 的 pool_b, 逐策略算:
  #   pool     ← pool_b ∧ rank(rank_key) ≤ universe_size (per D, within pool_b)
  #   tradable ← pool ∧ ¬OR(filters)
  #   score    ← Σ w·factor / Σ w·1{finite}                (全截面可算, 供 analysis IC)
  #   rank     ← score 在 tradable 内的 1-based 降序排名, 0 = 不在母集
  # 回测 top-N / exit / watch 与实盘选股读同一个 rank 列 ⇒ "回测 = 实盘" 收敛到单一入口.

  parallel for d in [0, n_d):
    for s in strategy::STRATEGIES:
      compute pool/tradable/score/rank for (s, d)

Phase 4 校验  (串行, 轻量)
  for spec in ALL_NODES where spec->must_be_finite:
    T.assert_finite(*spec)              # 状态机 bool / factor pipeline 输出等契约列必须全 finite
  for slot in [0, N_STRAT_SLOTS):
    T.assert_finite_strat(slot)         # 策略 5 列契约上全 finite
```

## 策略层 (每策略一份固定小管线)

每个策略是 `cpp/include/strategy/def/<name>.hpp` 下的一个 `StrategySpec`, 叶子全部指向共享计算图节点 (`FeatureSpec` 指针), 只在 `cpp/include/strategy/registry.hpp::STRATEGIES[]` 挂载一行:

```cpp
inline constexpr std::array<const StrategySpec *, N> STRATEGIES = {{
    &def::small_cap,
    // &def::<new_strategy>,
}};
```

`StrategySpec` 字段:
- `pool`: `PoolSpec` — exchange/list_sector/行业白名单 + `rank_key` (排名用的共享节点, 如 `mcap_raw_spec`) + `rank_asc` + `universe_size`.
- `filters`: `std::span<const FeatureSpec *const>`, 全部必须 `Kind::Filter` (registry consteval 校验).
- `weights`: `std::span<const FactorWeight>` (`{f, w}`), `f` 必须 `Kind::Factor` 且 `w > 0`.
- `bt_start_date` / `hold_n` / `exit_ratio`: per-策略回测参数 (成本 / `capital_base` 是券商账户属性, 留在 `config.hpp` 全策略共享).

每策略固定绑定 5 列 (`strategy::SF`), 存 `Tensor.strat_mats` (不占共享图 `mats`), 计算见 §构建流水线 Phase 2s/3s: `pool_b` (静态白名单母集) → `pool` (截面 universe) → `tradable` (可买母集) → `score` (加权因子) → `rank` (tradable 内降序排名, 0=不在母集). 回测/实盘选股/分析全部只读这 5 列, 不重复实现选股逻辑.

`main.cpp` 对 `strategy::STRATEGIES[]` 循环跑 `backtest::run` + `analysis::run`, 各自输出到独立的 `output/strategy/<name>/{backtest,analysis}/`; 共享的 `feature::ALL_NODES` 计算图与 `backtest::NameTimeline` 只算一次. 涨跌停交易约束 (下单层面, 不在张量内): 物理约束 (做不到) — 涨停日不买入 / 跌停日不卖出; 策略主动 (业务选择) — 涨停日不卖出 (赌 T+1 超额) / 跌停日不买入 (避 T+1 风险).

## 增减用法

新增/修改/删除一个 itf (数据源接入):
1. `cpp/src/api/{bigquant,tushare}/spec.cpp`: 在 `SPECS[]` 追加 spec (BigQuant 的 `TableSpec{name, visible_date, FetchKind, FetchFreq}` / Tushare 的 `InterfaceSpec{name, api, day_params, drop_fields}`).
2. `cpp/include/feature/pit.hpp`: 加/改 typed `Grid<…>` / `<…>Ev` struct (字段必须 POD: `int32_t` 日期 / `uint8_t` enum / float / int), 在 `PitPool` 加 `PoolArr<T>` / `EventStore<Ev>` 成员.
3. `cpp/src/feature/pit.cpp`: 加 `namespace itf_<name> { build, cache_layout, [post_ffill] }` 一组 dense block; `build` 端到端从月度 parquet (TableView 列访问) 直接写入 pool (内部 prealloc → 并行读月 → emplace → sort → finalize 一路串通); `cache_layout(pool, visitor)` 按固定顺序对每个 PoolArr/EventStore 调 `v.section(...)`.
4. `cpp/src/feature/pit.cpp`: `ITFS[]` 末尾追加一行 (`{file_name, &build, &cache_layout, post_ffill_or_nullptr}`).
   外层 `load.cpp` / `build.cpp` 不动. 改 PitPool 字段 / Ev struct / cache_layout 顺序时, `load.cpp::POOL_VERSION` +1.

新增一个 raw / 中间量 / filter / factor 节点 (见 §特征系统):
1. 写 `cpp/include/feature/def/{basic,factor,filter}/<name>.hpp`: 一个 `ts_<name>`/`cs_<name>` + 一个 `inline constexpr FeatureSpec <name>_spec` (含必填的 `formula`/`assumption`).
2. 在需要它的地方 `#include` 这个文件并引用 `&<name>_spec`: 某个已挂载节点的 `deps` 数组, 或某个 `StrategySpec` 的 `filters`/`weights`/`pool.rank_key`.
   没有第三步 — 不改 `registry.hpp`, 不改 `ts.cpp`/`cs.cpp`/`build.cpp`, 计算图/顺序/环检测全部编译期自动完成. 通用 kernel: `feature/ts.hpp::state_machine_intervals<TEv>` 模板; `feature/cs.hpp::winsor_mad/z/pct_rank/factor_pipeline/neutral_pipeline`. 大多数新 factor 一行 `factor_pipeline(d, xxx_raw_spec, xxx_spec, invert, T, b.a)` 即可.

新增一个策略:
1. 写 `cpp/include/strategy/def/<name>.hpp`: 一个 `inline constexpr StrategySpec <name>` (白名单/filters/weights/回测窗口), 叶子引用现有或新增的 `FeatureSpec`.
2. `cpp/include/strategy/registry.hpp::STRATEGIES[]` 追加一行 `&def::<name>`.
   `consteval registry_detail::validate()` 会校验名字唯一非空 / filters 全 `Kind::Filter` / weights 全 `Kind::Factor` 且 w>0 / 参数域合法; `feature/registry.hpp` 会自动把该策略新引用到的节点纳入计算图 (无需再改共享图任何文件).
