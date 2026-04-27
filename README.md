Motive: 实盘量化交易; 国金证券 QMT 客户端下单, tushare 维护本地财报披露时间线
Insight: 研究/回测在 bq/ 仓库; 此处只负责实盘执行 + tushare 数据本地化

qmt/
├── gjzqqmt/                         # 国金证券 QMT 客户端 (Linux Wine 跑 Windows 程序)
│   ├── run.md                       # Wine 安装 + 启动指南 (XtItClient=主端, XtMiniQmt=API端)
│   ├── gjqmt_setup.exe              # 安装器 (32 位, 必须 wine64+wine32 双装)
│   ├── 国金证券QMT交易端/             # 客户端本体 (bin.x64 是 64 位主程序)
│   └── QMT操作说明文档/               # 官方 PDF (操作/Python API/网格/VBA/算法交易)
├── data/                            # tushare 财报披露数据 (按 visible_date 落盘)
│   └── YYYY/MM/DD/*.json            # disclosure / forecast / express
├── scripts/                         # tushare 数据工具
│   ├── set_token.py                 # 设置 tushare token
│   ├── update_bulk.py               # 全量拉取 (2015 起, 月范围 + 单日两种接口模式)
│   └── update_incre.py              # 增量更新 (扫 data/ 找最后一天接着拉)
└── doc/
    ├── research/                    # 数据研究脚本
    │   ├── analysis.py              # 覆盖率分析 (按财季+发布偏移月统计 D/F/E)
    │   └── analysis.md              # 分析结果
    └── tushare/                     # tushare API 文档
        ├── tushare.md               # 接口索引
        ├── basic/                   # 基础信息 (stock_basic / trade_cal / namechange / ...)
        └── financial/               # 财务报表 (income / balancesheet / cashflow / forecast / express / ...)
