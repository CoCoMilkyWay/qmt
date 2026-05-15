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
│   │   ├── misc/                    # 通用工具 (date / fs / store / logging / progress / timer / affinity)
│   │   │                              # store.hpp: 数据集落地公共层 (day path / _empty.json / lastupdate / scan_missing); 不绑定数据源
│   │   ├── package/yyjson/          # JSON 库
│   │   ├── package/arrow/           # Arrow Flight wire transport (从 pyarrow vendor, 见 vendor_from_pyarrow.sh)
│   │   ├── api/                     # 数据接入子系统 (数据入)
│   │   │   ├── bigquant/            # 26 张表 DAI Arrow Flight + 控制面 HTTPS+HMAC
│   │   │   │   ├── https.hpp        # boost.beast over OpenSSL (走 443 控制面)
│   │   │   │   ├── signer.hpp       # HMAC-SHA256 控制面签名
│   │   │   │   ├── dai.hpp          # DaiClient (whoami / get_datasource_schema / query → arrow::Table)
│   │   │   │   ├── parse.hpp        # arrow::Table → yyjson 行式 JSON (整表 / 行子集)
│   │   │   │   ├── spec.hpp         # 26 张表 TableSpec (Static / Partition / Where × Day / MonthFirst, 内含 PK)
│   │   │   │   ├── store.hpp        # write_table_by_visible_date (PK upsert + _empty.json) / write_meta_table
│   │   │   │   └── pipeline.hpp     # scan → plan → fetch (DAI) → write
│   │   │   └── tushare/             # 3 张事件表 (forecast / express / disclosure) HTTP+JSON
│   │   │       ├── http.hpp         # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│   │   │       ├── spec.hpp         # 3 个 SPECS + RangeStrategy / PerDayStrategy
│   │   │       ├── store.hpp        # write_by_visible_date (PK upsert + _empty.json, 行式 JSON)
│   │   │       └── pipeline.hpp     # scan → plan → fetch (HTTP) → write
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
│           ├── pit.cpp              # 【单点真理 itf】每 itf 一个 namespace block (prealloc + parse + post_sort) + 末尾 ITFS[] 表挂载
│           ├── load.cpp             # Phase 1 通用 flow: 仅迭代 ITFS[] (prealloc → enumerate → 并行 parse → post_sort/post_ffill), 不出现 itf 名
│           ├── ts.cpp               # Phase 2 通用 flow: per-A 并行, 迭代 FEATURES[] 中 axis==TS 的 compute_ts 调; kernel 在 ts.hpp (state_machine_intervals 模板)
│           ├── cs.cpp               # Phase 3 通用 flow: per-D 并行, 迭代 FEATURES[] 中 axis==CS 的 compute_cs 调; kernel (winsor_mad / z / pct_rank / factor_pipeline) 在 cs.hpp/cpp
│           └── build.cpp            # 编排入口: 串 4 phase + misc::Timer 报段时
├── data/                            # 落地 (按 visible_date 切日, gitignored)
│   ├── _meta/                       # 单文件全量, 每次 update 覆盖刷新
│   │   ├── trading_days.json        # axis 源 D 轴 (BigQuant, market_code='CN')
│   │   ├── holidays.json            # 节假日 (BigQuant, market_code='CN')
│   │   ├── cn_stock_basic_info.json # axis 源 A 轴 + 静态 meta (BigQuant Static)
│   │   └── <name>.lastupdate        # 单表去重时间戳 (unix epoch s); 上次成功距今 < config::API_DEDUP_WINDOW_SECONDS 跳过
│   └── YYYY/
│       └── MM/
│           ├── _empty.json          # 反向稀疏标记 {name: [DD,...]} = 拉过且为空
│           └── DD/<name>.json       # 仅在该天有数据时存在 (PK 唯一, 路径 = visible_date)
│                                    # 三态: file 存在 / 在 _empty / 都不在 = 有数据 / 拉过空 / 未拉
│                                    #
│                                    # 26 张表 (全 JSON, 统一行式: [{col1:v,col2:v,...}, ...] 一行一记录, 人眼可读):
│                                    #   BigQuant 23 (DAI Arrow Flight → arrow::Table → 行式 JSON):
│                                    #     cn_stock_instruments,
│                                    #     cn_stock_industry_component (★ 月初一份), cn_stock_industry_change,
│                                    #     cn_stock_industry_bar1d, cn_stock_industry_valuation,
│                                    #     cn_stock_capital, cn_stock_dividend, cn_stock_allotment,
│                                    #     cn_stock_margin_trading_detail, cn_stock_margin_trading_market,
│                                    #     cn_stock_shareholder, cn_stock_shares, cn_stock_status, cn_stock_suspend,
│                                    #     cn_stock_name_change, cn_stock_dragon_list, cn_stock_bar1d, cn_stock_limit_price,
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

**数据通道**
- BigQuant DAI: 控制面 HTTPS `:443` (HMAC-SHA256 签名, `whoami` / `get_datasource_schema` / 元信息); 数据面 Arrow Flight (gRPC + Arrow IPC RecordBatch, `query(SQL, filters)` → `arrow::Table` 零拷贝).
- Tushare HTTP `:80`: 明文 JSON POST, 三张事件表 (`forecast` / `express` / `disclosure`).

**抓取策略** (`cpp/include/api/bigquant/spec.hpp`)

| kind      | freq       | SQL 写法                                                                 | filters          | 适用                                                                                                     |
| --------- | ---------- | ------------------------------------------------------------------------ | ---------------- | -------------------------------------------------------------------------------------------------------- |
| Static    | Day        | `SELECT * FROM <name>`                                                   | `{}`             | `cn_stock_basic_info` (无 date 维)                                                                       |
| Partition | Day        | `SELECT * FROM <name>`                                                   | `{"date":[s,e]}` | 日频网格 (行情 / 状态 / 财务 PIT / ...)                                                                  |
| Partition | MonthFirst | `WHERE <vd> = (SELECT MIN(<vd>) FROM <name> WHERE <vd> BETWEEN s AND e)` | `{"date":[s,e]}` | `cn_stock_industry_component` 月初一份全行业成分, 月内变动靠 `cn_stock_industry_change` (Day) 增量 cover |
| Where     | Day        | `SELECT * FROM <name> WHERE <vd> >= s AND <vd> <= e`                     | `{}`             | 事件型 (`publish_date` / `end_date` 等非分区列)                                                          |

`<vd>` = `TableSpec::visible_date` — Static 为空, Partition 通常是 `date`, Where 通常是 `publish_date` / `end_date`. Tushare 走 HTTP+JSON, 各表自带 `ann_date` 字段作 visible_date.

**落地** (按 `visible_date` 切日)
- 路径 `data/YYYY/MM/DD/<name>.json` — 日历日切分 (周末/节假日同样写盘); 全表统一**行式**落盘 (`[{col1:v, col2:v, ...}, ...]` 一行一记录, 人眼可读). BigQuant 走 DAI Arrow Flight → `arrow::Table` → `parse.hpp::table_to_json` (按 row 出 obj 序列化), Tushare 走 HTTP+JSON 直落. 表名互不冲突, 同目录共存.
- axis + 静态 (单文件): `data/_meta/{trading_days, holidays, cn_stock_basic_info}.json` 不走 per-day, 每次 update 覆盖刷新.
- **完整性设计** (BigQuant / Tushare 完全沿用同一套, `store.hpp` 平行实现):
  - **PK upsert** (同次响应): 同 PK 同次响应末条胜, PK 因 itf 而异 (见 `cpp/src/api/{bigquant,tushare}/spec.cpp`).
  - **三态稀疏**: file 存在 (有数据) / 在 `_empty.json` (拉过空) / 都不在 (未拉); 避免空日反复回 fetch.
  - **lastupdate 去重**: `data/_meta/<name>.lastupdate` (unix epoch s); 上次成功距今 < `config::API_DEDUP_WINDOW_SECONDS` 跳过整段 (`misc::store::should_skip_api` / `mark_api_updated`).
  - **lookback 增量回扫**: `scan_missing` 在 `[start, end]` 内, 文件不存在必拉 + 最近 `lookback_days` 日历日内必拉 (PK upsert 吃订正; 7 天 ≈ 5 交易日, 兜住当日未结算累积缺失).
  - **tmp+rename 写**: 单文件 atomic 替换, 中断不留半成品.
- 月初快照: `cn_stock_industry_component` 每月仅在 `visible_date = MIN(date)` 一天落盘.
- 入库时机源: BigQuant 见 `doc/bigquant/used/api.md` 更新时间列 (多数 17:00–19:30 盘后批发); Tushare 见 `doc/tushare/help/数据更新说明.md` 及各 API 自身 doc.
- 公告披露时段背景: `doc/exchange/公告类别和发布时间.md` (SSE/SZSE 各时段 + 非交易日 13–17 / 12–16 直通).

**cutoff** (build-time, 实盘/回测同一公式)
- 信号时点 ≜ 交易日 T 盘中, 信号计算前 1 分钟刷库; 此后每个 itf 按 `visible_date ≤ T + offset(itf)` 切片写入 row D.
- `offset` 单位 = **日历日**, 含周末/节假日; 取值见下表 `偏移` 列:
  - 入库时点 < 信号时点 (盘前已到位): `0` — T 当日记录可见.
  - 入库时点 ≥ 信号时点 (盘后 / "实时" / "不定期"): `−1` — 截至 T−1 calendar; 自然把 T−1 盘后 + T−1 至 T 之间的周末/节假日公告 (含 SSE/SZSE 非交易日直通时段) 划入 T 行.

**入张量映射** (cutoff 后按 itf 类落 row D)
- **网格** (D=trade_date, A=instrument): 每条记录唯一 (D, A) 单元. row D 取 `max{ visible_date ≤ T + offset }` (offset=0 → 自身; offset=−1 → 上一交易日, 周末/假日 visible_date 不存在自动跳过).
- **事件 sparse PIT** (D, A): 每 (A, group_key) 取 `visible_date ≤ T + offset` 的最新一条. group_key 见 §字段表 deps (例: `forecast` / `income_general_pit` / `cashflow_general_pit` / `balance_general_pit` 按 `report_date`, `dividend` 按 `publish_date`, `name_change` 按 `end_date`). 状态机型 (`profit_st` / `revenue_st` / `dividend_st`) 同样按此 cutoff 回放 `visible_date` 升序流.
- **月初快照** (D, A): `cn_stock_industry_component` 月初落一份, build 时取 `max{ visible_date ≤ T }` 的快照广播到 (D, A); 月内细变动叠加 `cn_stock_industry_change` 事件流 → `industry_l1` inter feature.
- **asset 静态**: `cn_stock_basic_info` 全量 snapshot 广播到 (D, A); `list_date / delist_date` 决定 (D, A) 行有效, 不走 `visible_date`.
- **axis**: `trading_days` WHERE `market_code='CN'` 生成 D 轴; `cn_stock_basic_info.instrument` 全量生成 A 轴.

**一致性** (build 完成 → 张量 PIT-clean, 下游无未来数据风险)
- **同次去重**: 同 PK 同次响应末条胜 (PK 因 itf 而异, 见 `cpp/src/api/bigquant/spec.cpp` 与 `cpp/src/api/tushare/spec.cpp`).
- **跨次修正** (replay 安全): 修正写新 day-file, 旧版本留存不被覆盖 → replay 任意 T 按上述 cutoff 自动选当时可见版本.
- **回测 = 实盘**: 同一份 build 代码 + 同一组 offset → 同一份 PIT 张量.
- 已知 best-effort 瑕疵 (不可消除, 接受):
  1. 公告级时间戳缺失 → 同 `visible_date` 内盘前/盘中/盘后无法区分, 统一按盘后保守 → 计入 next-day cutoff (实盘错过 T 当日盘前直通公告, 与回测一致).
  2. `cn_stock_basic_info` 仅当前 snapshot → `list_sector` / `industry` 历史变更无法回溯; 行业变更已用 `cn_stock_industry_component` (月初) + `cn_stock_industry_change` (日频) 补救, 但 `list_sector` 转板股全期按当前归类.
  3. `cn_stock_suspend` 入库时点文档未明示, 按"通常盘前"取 `offset=0`; 极端情况下 T 盘中刚发布的停牌可能在信号刷库时尚未入库, 想绝对保守可改 `−1`.

| 类       | name                                      | 通道                | 入库时机                                           | visible_date   | 偏移 |
| -------- | ----------------------------------------- | ------------------- | -------------------------------------------------- | -------------- | ---- |
| axis     | `trading_days` / `holidays`               | BigQuant            | 定期 (新年度排程)                                  | `date`         | —    |
| asset    | `cn_stock_basic_info`                     | BigQuant Static     | 每次 update 覆盖刷新                               | —              | —    |
| 月初快照 | `cn_stock_industry_component`             | BigQuant MonthFirst | 月初首日 (SW2021 一级 / cs / sw2014 三套)          | `date`         | 0    |
| 网格     | `cn_stock_industry_change`                | BigQuant            | **盘后** (行业进出事件)                            | `date`         | −1   |
| 网格     | `cn_stock_industry_bar1d`                 | BigQuant            | **盘后**                                           | `date`         | −1   |
| 网格     | `cn_stock_industry_valuation`             | BigQuant            | **盘后**                                           | `date`         | −1   |
| 网格     | `cn_stock_instruments`                    | BigQuant            | **盘前** (当日全市场可交易列表)                    | `date`         | 0    |
| 网格     | `cn_stock_bar1d`                          | BigQuant            | **盘后** (后复权 OHLCV + `adjust_factor`)          | `date`         | −1   |
| 网格     | `cn_stock_limit_price`                    | BigQuant            | **盘前** (`upper_limit` / `lower_limit`)           | `date`         | 0    |
| 网格     | `cn_stock_status`                         | BigQuant            | **盘前** 09:20 全量快照 (st_status/suspended/...)  | `date`         | 0    |
| 网格     | `cn_stock_suspend`                        | BigQuant            | 通常**盘前** (`suspend_period`+`suspend_reason`)   | `date`         | 0    |
| 网格     | `cn_stock_shares`                         | BigQuant            | **盘后**                                           | `date`         | −1   |
| 网格     | `cn_stock_margin_trading_detail`          | BigQuant            | **盘前** (T 日入库 T−1 明细)                       | `date`         | 0    |
| 网格     | `cn_stock_margin_trading_market`          | BigQuant            | **盘前**                                           | `date`         | 0    |
| 网格     | `cn_stock_dragon_list`                    | BigQuant            | **盘后** (+1/+2/+5 日涨跌幅列含未来信息, 不入张量) | `date`         | −1   |
| 事件     | `cn_stock_capital`                        | BigQuant            | 公告实时 (通常盘后)                                | `publish_date` | −1   |
| 事件     | `cn_stock_dividend`                       | BigQuant            | 公告实时                                           | `publish_date` | −1   |
| 事件     | `cn_stock_allotment`                      | BigQuant            | 公告实时                                           | `publish_date` | −1   |
| 事件     | `cn_stock_shareholder`                    | BigQuant            | 公告实时                                           | `publish_date` | −1   |
| 事件     | `cn_stock_name_change`                    | BigQuant            | 简称失效日 (本段区间确知)                          | `end_date`     | −1   |
| 财务 PIT | `cn_stock_financial_income_general_pit`   | BigQuant            | 公告实时 (随财报)                                  | `date`         | −1   |
| 财务 PIT | `cn_stock_financial_cashflow_general_pit` | BigQuant            | 公告实时                                           | `date`         | −1   |
| 财务 PIT | `cn_stock_financial_balance_general_pit`  | BigQuant            | 公告实时                                           | `date`         | −1   |
| 财务 TTM | `cn_stock_financial_ttm_shift`            | BigQuant            | 公告实时                                           | `date`         | −1   |
| 财务附注 | `cn_stock_financial_notes_shift`          | BigQuant            | 公告实时                                           | `date`         | −1   |
| 事件     | `forecast`                                | Tushare             | 公告实时 (通常盘后)                                | `ann_date`     | −1   |
| 事件     | `express`                                 | Tushare             | 公告实时 (通常盘后)                                | `ann_date`     | −1   |
| 事件     | `disclosure`                              | Tushare             | 公告实时 (披露计划公告/修订)                       | `ann_date`     | −1   |

## 字段表

本节是 feature 的「契约 / 数学定义」(描述"做什么"); 实现镜像在 `cpp/src/feature/feature.cpp` 的 `FEATURES[]` 表 + `impl::ts_*` / `impl::cs_*` (描述"怎么做"). 增减/修改 feature 须同步两处.

排序 (本表, 阅读用): filter → factor → inter; inter 内部按 causal (raw → derived); 相关字段就近. (注: `cpp/include/feature/feature.hpp` 的 `F` 枚举顺序 = 计算顺序, 与本表排序独立; enum 大段仅 TS / CS 两类, 段内按"相似聚集"对仗 — raw 网格 / raw 财务 / raw meta 派生 / derived / filter / pool.) `assumption` 列 `—` = 定义自洽; 形如 `[元]` `[%]` `[ratio]` `[股]` 的方括号前缀标注 inter 输出单位.

`deps` 列约定: `itf:<name>` ≡ 该 itf 经 §入张量统一规则 切到 (D, A); 其它为 inter / filter feature 名 或 `meta:<field>` (asset 静态).

`轴` 列: `时序` = per A 沿 D 计算 (无截面依赖, A 维可并行); `截面` = per D 沿 A 计算 (有截面依赖, A 维不可并行). `filter` / `factor` / `inter` 三类均可能出现两种轴之一, 仅读本行.

估值/盈利因子按 `<base>_ttm<N>` 命名, period 由季节性决定:
- **ttm4** (高季节性, 4 报告期 ≡ 1 年): 取 `cn_stock_financial_ttm_shift.*_ttm` (shift=0) 或 mcap_raw / TTM 字段自算 (估值类, 支持负值).
- **ttm1** (低季节性, 单期 snapshot ≡ MRQ): 取 `cn_stock_financial_balance_general_pit.*` (latest shift) 自算; 例 pb = mcap_raw / total_owner_equity.

tensor 内特征数据本身就要保证没有未来信息, 最大化保证数据安全.

| kind   | feature      | 轴   | deps                                                                         | formula                                                                                                                                                                                                          | assumption                                                                                                                         |
| ------ | ------------ | ---- | ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| filter | profit_st    | 时序 | itf:forecast, itf:cn_stock_financial_changedate                              | 状态机 (per A):                                                                                                                                                                                                  | `changedate` 为实际披露 PIT 信号; 未披露的股票不出, 4 月底安全网兜底                                                               |
|        |              |      |                                                                              | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.last_parent_net < 0` 时                                                                                                                     |                                                                                                                                    |
|        |              |      |                                                                              | 按 `forecast.ann_date` 触发, 至 `changedate` (同 end_date) 或 `(end_date.Y+1, 4, monthend)` 终止 (取较早)                                                                                                        |                                                                                                                                    |
| filter | revenue_st   | 时序 | itf:forecast, itf:cn_stock_financial_changedate, rev_raw, meta:list_sector   | 状态机 (per A) ∧ `meta.list_sector ∈ MAIN_BOARD ∧ rev_raw < (3e8 if end_date.Y ≥ 2024 else 1e8)`:                                                                                                                | 同 profit_st 终止条件; `MAIN_BOARD` 判定 inline `meta.list_sector[a]` (asset 静态, 全 D 同值)                                      |
|        |              |      |                                                                              | `forecast.end_date.M == 12 ∧ forecast.type ∈ {首亏, 续亏} ∧ forecast.end_date.Y ≥ 2021 ∧ forecast.ann_date ≥ 20210101` 时                                                                                        |                                                                                                                                    |
|        |              |      |                                                                              | 按 `forecast.ann_date` 触发, 至 `changedate` 或 `(end_date.Y+1, 4, monthend)` 终止 (取较早)                                                                                                                      |                                                                                                                                    |
| filter | dividend_st  | 时序 | itf:cn_stock_dividend, ni_raw, share_raw, meta:list_sector                   | `meta.list_sector ∈ MAIN_BOARD ∧ ni_raw > 0 ∧ 3y_sum(dividend.cash_after_tax × share_raw) < 0.30 × ni_raw ∧ 3y_sum < 5e7`                                                                                        | 3y 窗口 = `dividend.report_date.Y ∈ [Y-3, Y-1]` (Y = `dividend.publish_date.Y`); share_raw 取 `publish_date` 当日快照; 单位均 [元] |
| filter | trading_st   | 时序 | low_p, low_mc                                                                | `rolling_20D(low_p ∨ low_mc).all()`                                                                                                                                                                              | —                                                                                                                                  |
| filter | risk_warn    | 时序 | itf:cn_stock_status                                                          | `cn_stock_status.st_status ∈ {1, 2}` (1=ST, 2=*ST)                                                                                                                                                               | 每日盘前全量快照, 无需状态机/回放; 数据起点前一律 0                                                                                |
| filter | new_list     | 时序 | list_age                                                                     | `0 ≤ list_age < 60`                                                                                                                                                                                              | —                                                                                                                                  |
| factor | close        | 截面 | close_raw                                                                    | `pct_rank(z(winsor_mad(1 / close_raw)))`                                                                                                                                                                         | —                                                                                                                                  |
| factor | mcap         | 截面 | mcap_raw                                                                     | `pct_rank(z(winsor_mad(1 / mcap_raw)))`                                                                                                                                                                          | —                                                                                                                                  |
| factor | fmcap        | 截面 | fmcap_raw                                                                    | `pct_rank(z(winsor_mad(1 / fmcap_raw)))`                                                                                                                                                                         | —                                                                                                                                  |
| factor | pe_ttm4      | 截面 | pe_raw                                                                       | `pct_rank(z(winsor_mad(1 / pe_raw)))`                                                                                                                                                                            | —                                                                                                                                  |
| factor | pb_ttm1      | 截面 | pb_raw                                                                       | `pct_rank(z(winsor_mad(1 / pb_raw)))`                                                                                                                                                                            | —                                                                                                                                  |
| factor | ps_ttm4      | 截面 | ps_raw                                                                       | `pct_rank(z(winsor_mad(1 / ps_raw)))`                                                                                                                                                                            | —                                                                                                                                  |
| factor | pcf_ttm4     | 截面 | pcf_raw                                                                      | `pct_rank(z(winsor_mad(1 / pcf_raw)))`                                                                                                                                                                           | —                                                                                                                                  |
| factor | roe_ttm4     | 截面 | roe_raw                                                                      | `pct_rank(z(winsor_mad(roe_raw)))`                                                                                                                                                                               | —                                                                                                                                  |
| factor | roa_ttm4     | 截面 | roa_raw                                                                      | `pct_rank(z(winsor_mad(roa_raw)))`                                                                                                                                                                               | —                                                                                                                                  |
| factor | dy_ttm4      | 截面 | dy_raw                                                                       | `pct_rank(z(winsor_mad(dy_raw)))`                                                                                                                                                                                | —                                                                                                                                  |
| inter  | close_raw    | 时序 | itf:cn_stock_bar1d                                                           | `cn_stock_bar1d.close` (后复权)                                                                                                                                                                                  | [元/股]                                                                                                                            |
| inter  | daily_return | 时序 | close_raw                                                                    | `close_raw[d] / close_raw[d-1] - 1`                                                                                                                                                                              | [ratio]; 链式日收益; `d==0` 或 `close_raw[d-1]` NaN/0 → NaN; 下游 benchmark = pool 内等权 daily_return 均值                        |
| inter  | up_lim       | 时序 | itf:cn_stock_limit_price                                                     | `cn_stock_limit_price.upper_limit` (未复权)                                                                                                                                                                      | [元/股]                                                                                                                            |
| inter  | dn_lim       | 时序 | itf:cn_stock_limit_price                                                     | `cn_stock_limit_price.lower_limit` (未复权)                                                                                                                                                                      | [元/股]                                                                                                                            |
| inter  | susp         | 时序 | itf:cn_stock_status                                                          | `cn_stock_status.suspended == 1`                                                                                                                                                                                 | [bool]; 当日是否停牌                                                                                                               |
| inter  | share_raw    | 时序 | itf:cn_stock_shares                                                          | `cn_stock_shares.total_shares`                                                                                                                                                                                   | [股]                                                                                                                               |
| inter  | mcap_raw     | 时序 | close_raw, share_raw                                                         | `close_raw × share_raw`                                                                                                                                                                                          | [元]                                                                                                                               |
| inter  | fmcap_raw    | 时序 | close_raw, itf:cn_stock_shares                                               | `close_raw × cn_stock_shares.total_float_shares`                                                                                                                                                                 | [元]                                                                                                                               |
| inter  | pe_raw       | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                   | TBD                                                                                                                                                                                                              | [ratio]; ttm4; 支持负 PE                                                                                                           |
| inter  | pb_raw       | 时序 | mcap_raw, itf:cn_stock_financial_balance_general_pit                         | TBD                                                                                                                                                                                                              | [ratio]; ttm1 (MRQ)                                                                                                                |
| inter  | ps_raw       | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                   | TBD                                                                                                                                                                                                              | [ratio]; ttm4                                                                                                                      |
| inter  | dy_raw       | 时序 | itf:cn_stock_dividend, share_raw, mcap_raw                                   | TBD                                                                                                                                                                                                              | [ratio]; ttm12M                                                                                                                    |
| inter  | pcf_raw      | 时序 | mcap_raw, itf:cn_stock_financial_ttm_shift                                   | TBD                                                                                                                                                                                                              | [ratio]; ttm4                                                                                                                      |
| inter  | roe_raw      | 时序 | itf:cn_stock_financial_ttm_shift, itf:cn_stock_financial_balance_general_pit | TBD                                                                                                                                                                                                              | [%]; ttm4                                                                                                                          |
| inter  | roa_raw      | 时序 | itf:cn_stock_financial_ttm_shift, itf:cn_stock_financial_balance_general_pit | TBD                                                                                                                                                                                                              | [%]; ttm4                                                                                                                          |
| inter  | rev_raw      | 时序 | itf:cn_stock_financial_ttm_shift                                             | TBD                                                                                                                                                                                                              | [元]; ttm4                                                                                                                         |
| inter  | ni_raw       | 时序 | itf:cn_stock_financial_income_general_pit                                    | TBD                                                                                                                                                                                                              | [元]; 严格只取年报 (M=12) 记录                                                                                                     |
| inter  | list_age     | 时序 | meta:list_date                                                               | `D − meta.list_date` if `D ≥ list_date` else NaN                                                                                                                                                                 | [日历日]; 仅上市当日及之后写值 (上市当日=0), 否则 NaN; 下游 `is_finite` 判"已上市"                                                 |
| inter  | delist_age   | 时序 | meta:delist_date                                                             | `D − meta.delist_date` if `D ≥ delist_date` else NaN                                                                                                                                                             | [日历日]; 仅退市当日及之后写值 (退市当日=0), 否则 NaN; 下游 `is_finite` 判"已退市"                                                 |
| inter  | is_margin    | 时序 | itf:cn_stock_margin_trading_detail                                           | `1.0 if itf:cn_stock_margin_trading_detail (D, A) 存在 else 0.0`                                                                                                                                                 | [bool]; 当日是否融资融券标的                                                                                                       |
| inter  | mr_bal_raw   | 时序 | itf:cn_stock_margin_trading_detail                                           | `cn_stock_margin_trading_detail.financing_balance`                                                                                                                                                               | [元]; 融资余额; per-A grid post_ffill                                                                                              |
| inter  | ms_bal_raw   | 时序 | itf:cn_stock_margin_trading_detail                                           | `cn_stock_margin_trading_detail.securities_lending_balance`                                                                                                                                                      | [元]; 融券余额; 同上                                                                                                               |
| inter  | industry_l1  | 时序 | itf:cn_stock_industry_component, itf:cn_stock_industry_change                | base = 最近一份月初 `cn_stock_industry_component WHERE industry='sw2021'` 取 `industry_level1_name` 广播; 月内累加 `cn_stock_industry_change` 事件                                                               | per (D, A); 月初快照 + 月内增量回放                                                                                                |
| inter  | low_p        | 时序 | close_raw                                                                    | `close_raw < 1.0`                                                                                                                                                                                                | [bool]                                                                                                                             |
| inter  | low_mc       | 时序 | mcap_raw, meta:list_sector                                                   | `mcap_raw < (5e8 if meta.list_sector ∈ MAIN_BOARD else 3e8)`                                                                                                                                                     | [bool]; MAIN_BOARD 判定 inline meta.list_sector[a]                                                                                 |
| inter  | limit_up     | 时序 | close_raw, up_lim                                                            | `close_raw ≥ up_lim − 1e-4`                                                                                                                                                                                      | [bool]; 策略涨停判定                                                                                                               |
| inter  | limit_dn     | 时序 | close_raw, dn_lim                                                            | `close_raw ≤ dn_lim + 1e-4`                                                                                                                                                                                      | [bool]; 策略跌停判定                                                                                                               |
| inter  | pool_b       | 时序 | susp, is_margin, delist_age, meta:exchange, meta:list_sector, industry_l1    | `meta.exchange ∈ POOL_EXCHANGE_WHITELIST ∧ meta.list_sector ∈ POOL_MARKET_WHITELIST ∧ industry_l1 ∈ POOL_INDUSTRY_L1_WHITELIST ∧ ¬susp ∧ ¬is_finite(delist_age) ∧ (true if POOL_INCLUDE_MARGIN else ¬is_margin)` | [bool]; basic pool, 白名单 + 开关集中在 `config.hpp`                                                                               |
| inter  | pool         | 截面 | pool_b, mcap_raw                                                             | `pool_b ∧ rank(mcap_raw asc) ≤ UNIVERSE_SIZE` (per D, within `pool_b`; 默认 UNIVERSE_SIZE = 80)                                                                                                                  | [bool]; 排名母集 (pct_rank 母集 + nth-smallest 母集)                                                                               |
| inter  | tradable     | 截面 | pool, profit_st, revenue_st, dividend_st, trading_st, risk_warn, new_list    | `pool ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)`                                                                                                                             | [bool]; 选股母集 (策略实际可买入的 a). pool 不变以稳定 factor pct_rank 口径; 下游策略每日 top-K 在 tradable 内挑                   |

涨停时不会买入 (做不到): 物理约束
跌停时不会卖出 (做不到): 物理约束
涨停时不会卖出 (赌 T+1 超额): 策略主动意图
跌停时不会买入 (避 T+1 风险): 策略主动意图

## 构建流水线 (data → Tensor)

`feature::build()` 串 4 phase 全过程式; 入口 `cpp/src/feature/build.cpp`.

**Phase 切分动机**

| phase  | 数据形态          | 任务粒度             | 并行性             | 主要工作                              |
| ------ | ----------------- | -------------------- | ------------------ | ------------------------------------- |
| 0 axes | 标量级元数据      | 主线程               | 无                 | 一次性确定 D / A / per-A 静态         |
| 1 load | 文件级 raw (json) | (day, itf) ≈ 3650×26 | embarrassingly     | 解析 + **PIT cutoff 落到 row D 索引** |
| 2 时序 | 列式 (per-A 全 D) | a ≈ 5500             | embarrassingly (A) | 单调时间序列计算 + 状态机             |
| 3 截面 | 行式 (per-D 全 A) | d ≈ 2750             | embarrassingly (D) | 截面归一 + universe 选取              |

**业务密集化 + agnostic 外层** — 改字段表/计算图不动外层:
- `pit.cpp` (itf 维 单点真理): 每 itf 一组 `{prealloc, parse, post_sort?, post_ffill?}` + 末尾 `ITFS[]` 表挂载.
- `feature.cpp` (feature 维 单点真理): 每 feature 一个 `ts_xxx` / `cs_xxx` compute fn + 末尾 `FEATURES[]` 表挂载.
- 外层 flow (`load.cpp` / `ts.cpp` / `cs.cpp` / `build.cpp`) 仅通过函数指针表迭代调度, 不出现任何具体 itf 名 / feature 名.

**关键设计选择** (动机性的, 散落点上提):
- **PIT cutoff 在 Phase 1 一次性消化**: `parse` 内 `row = v_idx - itf::CUTOFF` 直接定位行 D, 写完后 `pool[a, d]` 即 "T 当日合法可见数据". Phase 2/3 不再做任何时间偏移 — 杜绝下游漏算 cutoff 导致的未来数据泄漏.
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
  meta ← load_stock_meta(axes)               # per-A 静态: list_date / delist_date / list_sector / exchange
                                             #   (industry_l1 是时变, 见 Phase 2 inter feature, 不入 meta)
  T    ← Tensor(axes)                        # F 段独立 A*D float, NaN 初始化, a-major / d-minor
                                             #   ts_row(f,a) = 连续 D span (Phase 2 主路径)
                                             #   gather/scatter_cs_row(f,d) = stride D copy (Phase 3 入口)

Phase 1 PIT load  (per-(day, itf) 并行; load.cpp 通用 flow + pit.cpp 单点 itf 表)
  # 形态: 文件级, 任务粒度 = (day, itf). 文件之间无依赖 → 任务池抢占式分发.
  # 关键: cutoff 在此 一次性 落到 row 索引, 写后 pool[a, d] 即 "T 当日合法可见数据"; 下游 0 时间偏移.
  # 并发: 网格 itf 写入完全无锁 (slot 唯一); 事件 itf 仅 per-A mutex 锁 emplace, 争用接近 0.
  # 编码: 全部走 yyjson_read, 统一行式 (array-of-objs); 行迭代 emplace 事件 / 网格.
  #       itf.parse 在自身 namespace 决定字段映射, 外层 flow 不感知 schema.

  for itf in pit.cpp::ITFS[]:                # 仅迭代 ITFS[] 表, 不出现具体 itf 名
    itf.prealloc(axes, pool)                 # 网格: 字段 vector A*D NaN/0; 事件: EventStore[A] 空链

  tasks ← enumerate data/YYYY/MM/DD/<itf.file_name>.json over ITFS[]:
    v_idx ← axes.floor_date(file's day = visible_date)
    skip if v_idx < 0                        # visible_date 早于 dates[0], 无 row D 可写
    skip if 网格 itf ∧ file's day ∉ axes.date_idx   # 网格 file's day 须为交易日 (data 自身保证)

  parallel for task in tasks (n_threads = misc::Affinity::core_count()):
    rec ← task.itf.read(task.path)           # json → yyjson_val (行式 array-of-objs, 顶层 yyjson_arr)
    task.itf.parse(rec, v_idx, axes, pool, mu_or_null)
                                             # row = v_idx - itf::CUTOFF
                                             #   (CUTOFF=0 → row=v_idx; CUTOFF=-1 → row=v_idx+1; row≥n_d 越界 skip)
                                             # 网格: mu=nullptr; pool.<itf>.<field>[a*n_d + row] 无锁写
                                             # 事件: mu[a] 锁后 pool.<itf>[a].emplace_back(Ev{v=row,…})

  for itf in ITFS[] where itf.post_sort:     # 事件 itf 末段 sort by v 升序 — 给 Phase 2 单调指针扫
    itf.post_sort(pool)

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
  #     close_raw   ← cn_stock_bar1d.close[a, d]                            (后复权)
  #     share_raw   ← cn_stock_shares.total_shares[a, d]                    (单位股)
  #     mcap_raw    ← close_raw × share_raw                                 (单位元)
  #     fmcap_raw   ← close_raw × cn_stock_shares.total_float_shares[a, d]
  #     up_lim / dn_lim ← cn_stock_limit_price.upper_limit / lower_limit[a, d]
  #     susp        ← cn_stock_status.suspended[a, d] == 1
  #     is_margin   ← cn_stock_margin_trading_detail[a, d] 存在性
  #     mr_bal_raw  ← cn_stock_margin_trading_detail.financing_balance[a, d]
  #     ms_bal_raw  ← cn_stock_margin_trading_detail.securities_lending_balance[a, d]
  #     risk_warn   ← cn_stock_status.st_status[a, d] ∈ {1, 2}              (每日快照, 不需回放)
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
  #     daily_return ← close_raw[d]/close_raw[d-1] - 1                       # d==0 或 close_raw[d-1] NaN/0 → NaN
  #     low_p        ← close_raw < 1.0
  #     low_mc       ← mcap_raw < (5e8 if meta.list_sector[a] ∈ MAIN_BOARD else 3e8)
  #     limit_up / limit_dn ← close_raw vs up_lim / dn_lim
  #   filter        (state machine; ts.hpp::state_machine_intervals 模板):
  #     profit_st   ← OR over { forecast 触发 → off=min(cn_stock_financial_changedate 同 end_date, ceil(Y+1,4,30)) }
  #                   on_d=trigger.v, 区间 [on_d, off_d) 写 1
  #     revenue_st  ← profit_st 同区间, 区间内再叠 (meta.list_sector ∈ MAIN_BOARD ∧ rev_raw < threshold(end_date.Y))
  #     dividend_st ← 阶梯 forward fill: 每 dividend event 重算 3y_sum (累加历史 events with
  #                     report_date.Y ∈ [publish_y-3, publish_y-1] 的 cash_after_tax × share_raw[event.v]),
  #                     仅 MAIN_BOARD; 区间 [e.v, next.v) 内按 (ni_raw>0 ∧ 3y_sum 双阈) 写 1.
  #                     warmup_d = max(axes 起点+3y, list_date+3y), 之前一律 0 (3y 窗口不完整, 不偏严).
  #     trading_st  ← rolling 20D over (low_p ∨ low_mc).all()                # 单调计数
  #     new_list    ← is_finite(list_age) ∧ list_age < 60
  #   pool (TS):
  #     pool_b   ← (meta.exchange ∈ config::POOL_EXCHANGE_WHITELIST)
  #                ∧ (meta.list_sector ∈ config::POOL_MARKET_WHITELIST)
  #                ∧ (industry_l1 ∈ config::POOL_INDUSTRY_L1_WHITELIST) ∧ ¬susp
  #                ∧ ¬is_finite(delist_age)
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
  #   close (← close_raw, invert), mcap, fmcap, pe_ttm4, pb_ttm1, ps_ttm4, pcf_ttm4 (全 invert)
  #   roe_ttm4, roa_ttm4, dy_ttm4 (无 invert)
  #   pool: cands ← {a : pool_b[a,d]=1 ∧ is_finite(mcap_raw[a,d])}
  #         pool[a,d] ← 1.0 if a ∈ nth_smallest(cands by mcap_raw, UNIVERSE_SIZE) else 0.0
  #   tradable: pool[a,d] ∧ ¬(profit_st ∨ revenue_st ∨ dividend_st ∨ trading_st ∨ risk_warn ∨ new_list)[a,d]
```

并发模型规格 (动机/不变量已在各 Phase 头部展开, 此处仅列数据 + 同步点)
- Phase 0: 主线程; 全量 in-memory; 跑一次.
- Phase 1: 任务数 ≈ 3650 day × |ITFS| (≈ 26); 网格 `mu=nullptr`, 事件 `vector<mutex>(n_a)`; 末段 `post_sort` / `post_ffill` 单线程串行.
- Phase 2: 任务数 ≈ n_a (5500); 每 worker 独占 `T.ts_row(*, a)`.
- Phase 3: 任务数 ≈ n_d (2750); 每 worker 独占 `cs_row` 段 + thread-local 3 buffer (length=n_a).
- 同步点: 仅 phase 间硬屏障 (`build.cpp` 顺序 `join` + `misc::Timer` 报段时), phase 内无屏障.

## 增减用法 (改计算图 / 字段表)

新增/修改/删除一个 itf:
1. `cpp/include/api/{bigquant,tushare}/spec.hpp`: 在 `SPECS[]` 末尾追加 spec (BigQuant 的 `TableSpec{name, visible_date, FetchKind, FetchFreq, Category, pk}` / Tushare 的 `InterfaceSpec`).
2. `cpp/include/feature/pit.hpp`: 加/改 typed `Grid<…>` / `<…>Ev` struct, 在 `PitPool` 加成员.
3. `cpp/src/feature/pit.cpp`: 加 `namespace itf_<name> { prealloc, parse, [post_sort, post_ffill] }` 一组 dense block; 统一从 `yyjson_val arr` (行式 array-of-objs) 行迭代读 record.
4. `cpp/src/feature/pit.cpp`: `ITFS[]` 末尾追加一行.
   外层 `load.cpp` / `build.cpp` 不动.

新增/修改/删除一个 feature:
1. `cpp/include/feature/feature.hpp`: 在 `F` 枚举对应位置加一行 (位置 = 计算顺序; 后于其依赖).
2. `cpp/src/feature/feature.cpp`: 在 `namespace impl` 加 `ts_<name>` (签名 `TsComputeFn`) 或 `cs_<name>` (签名 `CsComputeFn`).
3. `cpp/src/feature/feature.cpp`: `FEATURES[]` 对应位置加一行 `{name, kind, axis, &impl::ts_xxx | nullptr, &impl::cs_xxx | nullptr}`.
   外层 `ts.cpp` / `cs.cpp` / `build.cpp` 不动. 依赖通过 enum 顺序保证 (无需 topo sort).

通用 kernel (跨 feature 共用): `feature/ts.hpp` 暴露 `state_machine_intervals<TEv>` 模板; `feature/cs.hpp` 暴露 `winsor_mad / z / pct_rank / factor_pipeline`. 大多数新 factor 一行 `factor_pipeline(d, F::xxx_raw, F::xxx, invert, T, b.a)` 即可.
