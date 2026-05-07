Motive: 实盘量化交易; 国金证券 QMT 客户端下单, tushare 维护本地财报披露时间线
Insight: 研究/回测在 bq/ 仓库; 此处只负责实盘执行 + tushare 数据本地化

qmt/
├── run.py                           # 统一入口: build (py/main.py) + run (py/mode_*.py)
├── app/
│   ├── gjzqqmt/                     # 国金证券 QMT 客户端 (Linux Wine 跑 Windows 程序)
│   │   ├── run.md                   # Wine 安装 + 启动指南 (XtItClient=主端, XtMiniQmt=API端)
│   │   ├── 国金证券QMT交易端/         # 客户端本体 (bin.x64 是 64 位主程序)
│   │   └── QMT操作说明文档/           # 官方 PDF (操作/Python API/网格/VBA/算法交易)
│   └── api/tushare/                 # tushare Python SDK 副本 (历史保留, 不再用)
├── cpp/                             # C++23 实现 (Clang/Linux, header-only boost + yyjson)
│   ├── projects/main/               # CMake 构建 (DEBUG / PROFILE / ASSERT / PRODUCTION)
│   ├── include/
│   │   ├── config.hpp               # 全局常量 (token, API host, lookback, 拉取窗口)
│   │   ├── misc/                    # 通用工具 (date / fs / logging / progress / timer)
│   │   ├── package/yyjson/          # JSON 库
│   │   └── tushare/                 # tushare 子系统头文件
│   └── src/
│       ├── main.cpp                 # tushare::update(start, today, SPECS, lookback)
│       └── tushare/
│           ├── http.cpp             # boost.beast HTTP 客户端 (走 80 端口, 无 SSL)
│           ├── spec.cpp             # 7 个 SPECS + RangeStrategy / PerDayStrategy
│           ├── store.cpp            # scan_missing / write_by_visible_date (PK upsert + _empty.json)
│           └── pipeline.cpp         # scan → plan → fetch → write 主流程
├── data/                            # tushare 落地 (按 visible_date 切日, gitignored)
│   └── YYYY/
│       └── MM/
│           ├── _empty.json          # 反向稀疏标记 {itf: [DD,...]} = 拉过且为空
│           └── DD/<itf>.json        # 仅在该天有数据时存在 (PK 唯一, 路径 = visible_date)
│                                    # 三态: file 存在 / 在 _empty / 都不在 = 有数据 / 拉过空 / 未拉
│                                    # itf ∈ {forecast, express, disclosure, st, calendar, dividend,
│                                    #        daily_basic, adj_factor, stk_limit, suspend_d, fina_indicator}
├── py/                              # 构建/运行模式 (run.py 调用)
│   ├── main.py                      # CMake 配置 + 编译
│   └── mode_{debug,profile,assert,production}.py
└── doc/
    ├── research/                    # 数据研究脚本
    │   ├── analysis.py              # 覆盖率分析 (按财季+发布偏移月统计 D/F/E)
    │   └── analysis.md              # 分析结果
    └── tushare/                     # tushare API 文档
        ├── tushare.md               # 接口索引
        ├── help/                    # 通用 trick (本地化 / HTTP 协议 / 数据库落地)
        ├── basic/                   # 基础信息 (stock_basic / trade_cal / st / bak_basic / ...)
        └── financial/               # 财务报表 (forecast / express / disclosure_date / dividend / ...)
