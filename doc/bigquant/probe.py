"""BigQuant DAI 接口探测脚本.

对 doc/bigquant/used/api.md 列出的 30 个表逐一调用 `bq dai query`,
验证表存在 + 字段可用 + 单点查询返回非空.

约定:
  - 假设 license 已开放全部 (旗舰版 / 独立订阅). 未开放的会 rc!=0 暴露.
  - 单日 + 单 instrument 收敛返回行数, 仅为 schema/通路验证.
  - 财报 shift 表用 `shift=0` 取 D 当日可见的最新一期.

用法:
    python doc/bigquant/probe.py             # 全部 probe
    python doc/bigquant/probe.py <kw>        # 按 name/itf/table 子串过滤
"""

import json
import subprocess
import sys
from collections import namedtuple

D = "2024-12-31"
INST = "000001.SZ"

Probe = namedtuple("Probe", "name itf table sql filters note")

PROBES = [
    # ===== 通用数据 =====
    Probe(
        "交易日历",
        "trading_days",
        "trading_days",
        f"SELECT * FROM trading_days WHERE date='{D}'",
        None,
        "全球交易市场日历; market_code 过滤 SSE/SZSE",
    ),
    Probe(
        "节假日",
        "holidays",
        "holidays",
        "SELECT * FROM holidays LIMIT 5",
        None,
        "更新频率日频",
    ),
    Probe(
        "中国股票代码列表",
        "cn_stock_instruments",
        "cn_stock_instruments",
        f"SELECT * FROM cn_stock_instruments WHERE instrument='{INST}'",
        {"date": [D, D]},
        "全市场股票代码 + 中文简称 + type",
    ),
    #    Probe(
    #        "中国指数代码列表",
    #        "cn_index_instruments",
    #        "cn_index_instruments",
    #        "SELECT * FROM cn_index_instruments LIMIT 5",
    #        {"date": [D, D]},
    #        "申万行业指数代码列表",
    #    ),
    #    Probe(
    #        "中国基金代码列表",
    #        "cn_fund_instruments",
    #        "cn_fund_instruments",
    #        "SELECT * FROM cn_fund_instruments LIMIT 5",
    #        {"date": [D, D]},
    #        "每天参与交易的基金代码列表",
    #    ),
    #    Probe(
    #        "中国期货代码列表",
    #        "cn_future_instruments",
    #        "cn_future_instruments",
    #        "SELECT * FROM cn_future_instruments LIMIT 5",
    #        {"date": [D, D]},
    #        "每天的期货合约列表",
    #    ),
    #    Probe(
    #        "中国可转债代码列表",
    #        "cn_cbond_instruments",
    #        "cn_cbond_instruments",
    #        "SELECT * FROM cn_cbond_instruments LIMIT 5",
    #        {"date": [D, D]},
    #        "每天的可转债代码列表",
    #    ),
    # ===== 行业板块 =====
    Probe(
        "股票行业成分 (SW2021 PIT)",
        "cn_stock_industry_component",
        "cn_stock_industry_component",
        f"SELECT instrument,industry_level1_code,industry_level1_name,industry_level2_name,industry_level3_name FROM cn_stock_industry_component WHERE instrument='{INST}'",
        {"date": [D, D]},
        "PIT 历史归属 (date 列)",
    ),
    Probe(
        "行业进出记录",
        "cn_stock_industry_change",
        "cn_stock_industry_change",
        f"SELECT * FROM cn_stock_industry_change WHERE instrument='{INST}' LIMIT 5",
        {"date": ["2024-01-01", D]},
        "更新频率日频",
    ),
    Probe(
        "行业日线数据",
        "cn_stock_industry_bar1d",
        "cn_stock_industry_bar1d",
        "SELECT * FROM cn_stock_industry_bar1d LIMIT 5",
        {"date": [D, D]},
        "行业 OHLCV; instrument=行业代码",
    ),
    Probe(
        "行业估值",
        "cn_stock_industry_valuation",
        "cn_stock_industry_valuation",
        "SELECT * FROM cn_stock_industry_valuation LIMIT 5",
        {"date": [D, D]},
        "申万 + 中信一二级行业 PE/PB",
    ),
    # ===== 股票信息 =====
    Probe(
        "股票基本信息",
        "stock_basic / _meta",
        "cn_stock_basic_info",
        f"SELECT instrument,name,exchange,list_sector,list_date,delist_date,industry,corp_nature FROM cn_stock_basic_info WHERE instrument='{INST}'",
        None,
        "L+D+P+G; list_sector 表征 主板/创业板/科创板",
    ),
    Probe(
        "股本信息 (历史变动)",
        "cn_stock_capital",
        "cn_stock_capital",
        f"SELECT * FROM cn_stock_capital WHERE instrument='{INST}' LIMIT 5",
        None,
        "股本变动记录 (publish_date, change_date, reason)",
    ),
    Probe(
        "分红送股 (实施)",
        "dividend",
        "cn_stock_dividend",
        f"SELECT * FROM cn_stock_dividend WHERE instrument='{INST}' LIMIT 5",
        None,
        "PK = (instrument, report_date); 仅实施版无 div_proc 三阶段",
    ),
    Probe(
        "配股信息",
        "cn_stock_allotment",
        "cn_stock_allotment",
        f"SELECT * FROM cn_stock_allotment WHERE instrument='{INST}' LIMIT 5",
        None,
        "更新频率日频",
    ),
    Probe(
        "融资融券明细 (个股)",
        "margin_detail",
        "cn_stock_margin_trading_detail",
        f"SELECT date,instrument,financing_balance,securities_lending_balance,financing_purchase,financing_repayment,margin_trading_balance FROM cn_stock_margin_trading_detail WHERE instrument='{INST}'",
        {"date": [D, D]},
        "financing_balance ↔ rzye, securities_lending_balance ↔ rqye",
    ),
    Probe(
        "融资融券市场统计",
        "margin_market",
        "cn_stock_margin_trading_market",
        "SELECT * FROM cn_stock_margin_trading_market",
        {"date": [D, D]},
        "全市场总融资融券; method=sum/mean",
    ),
    Probe(
        "股东户数",
        "cn_stock_shareholder",
        "cn_stock_shareholder",
        f"SELECT * FROM cn_stock_shareholder WHERE instrument='{INST}' LIMIT 5",
        None,
        "publish_date / end_date; 无 date 列",
    ),
    Probe(
        "股本数据 (日频)",
        "shares (辅助 mcap)",
        "cn_stock_shares",
        f"SELECT * FROM cn_stock_shares WHERE instrument='{INST}'",
        {"date": [D, D]},
        "total_shares / a_float_shares / free_float_shares / total_float_shares",
    ),
    Probe(
        "股票状态 (ST/停牌/涨跌停)",
        "stock_st",
        "cn_stock_status",
        f"SELECT date,instrument,st_status,is_risk_warning,suspended,price_limit_status,exdr FROM cn_stock_status WHERE instrument='{INST}'",
        {"date": [D, D]},
        "★ st_status TINYINT 0/1/2 直接 boolean",
    ),
    Probe(
        "停复牌记录",
        "suspend_d",
        "cn_stock_suspend",
        f"SELECT * FROM cn_stock_suspend WHERE instrument='{INST}'",
        {"date": [D, D]},
        "含 suspend_period + suspend_reason",
    ),
    Probe(
        "证券简称变更",
        "namechange",
        "cn_stock_name_change",
        f"SELECT * FROM cn_stock_name_change WHERE instrument='{INST}'",
        None,
        "(instrument, start_date, end_date, name); 无 date 列",
    ),
    # ===== 股票行情 =====
    Probe(
        "龙虎榜",
        "cn_stock_dragon_list",
        "cn_stock_dragon_list",
        "SELECT * FROM cn_stock_dragon_list LIMIT 5",
        {"date": [D, D]},
        "上榜事件, 非每股每日有",
    ),
    Probe(
        "股票后复权日行情 (OHLCV)",
        "bar1d (后复权)",
        "cn_stock_bar1d",
        f"SELECT date,instrument,adjust_factor,pre_close,open,close,high,low,volume,amount,change_ratio,turn,upper_limit,lower_limit FROM cn_stock_bar1d WHERE instrument='{INST}'",
        {"date": [D, D]},
        "项目前复权口径需切换或自算",
    ),
    Probe(
        "股票涨跌停价格",
        "stk_limit",
        "cn_stock_limit_price",
        f"SELECT * FROM cn_stock_limit_price WHERE instrument='{INST}'",
        {"date": [D, D]},
        "完全对应 tushare stk_limit",
    ),
    # ===== 财务原始 PIT =====
    Probe(
        "利润表 PIT (一般企业)",
        "income_vip (pit raw)",
        "cn_stock_financial_income_general_pit",
        f"SELECT date,instrument,report_date,change_type,operating_revenue,net_profit,net_profit_to_parent_shareholders FROM cn_stock_financial_income_general_pit WHERE instrument='{INST}'",
        {"date": [D, D]},
        "PIT 原生 (date=visible_date); change_type 修订追踪",
    ),
    Probe(
        "现金流量表 PIT (一般企业)",
        "cashflow_vip (pit raw)",
        "cn_stock_financial_cashflow_general_pit",
        f"SELECT date,instrument,report_date,change_type,net_cffoa,net_cffia,net_cfffa,netinc_in_cce FROM cn_stock_financial_cashflow_general_pit WHERE instrument='{INST}'",
        {"date": [D, D]},
        "net_cffoa = 经营性现金流净额",
    ),
    Probe(
        "资产负债表 PIT (一般企业)",
        "balance_vip (pit raw)",
        "cn_stock_financial_balance_general_pit",
        f"SELECT date,instrument,report_date,change_type,total_assets,total_liabilities,total_owner_equity,total_equity_to_parent_shareholders FROM cn_stock_financial_balance_general_pit WHERE instrument='{INST}'",
        {"date": [D, D]},
        "PIT 原生; total_equity_to_parent_shareholders 用于自算 PB",
    ),
    # ===== 财务衍生 =====
    Probe(
        "财务衍生 TTM (利润+现金流)",
        "ttm_shift",
        "cn_stock_financial_ttm_shift",
        f"SELECT date,instrument,report_date,shift,operating_revenue_ttm,net_profit_ttm,net_profit_to_parent_shareholders_ttm,net_cffoa_ttm,net_cffia_ttm,net_cfffa_ttm FROM cn_stock_financial_ttm_shift WHERE instrument='{INST}' AND shift=0",
        {"date": [D, D]},
        "★ shift=0 取最新一期; ttm 直接给, 砍 ttm4_ytd 自算",
    ),
    Probe(
        "财务衍生 (财务附注 LF/MRQ/TTM)",
        "notes_shift",
        "cn_stock_financial_notes_shift",
        f"SELECT date,instrument,report_date,shift,nonrecurring_income_sum_lf,nonrecurring_income_sum_mrq,nonrecurring_income_sum_ttm FROM cn_stock_financial_notes_shift WHERE instrument='{INST}' AND shift=0",
        {"date": [D, D]},
        "财务附注 LF/MRQ/TTM 三套衍生",
    ),
]


def _run(p):
    cmd = ["bq", "dai", "query", p.sql]
    if p.filters:
        cmd += ["--filters", json.dumps(p.filters)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    return r.returncode, r.stdout or "", r.stderr or ""


def _print_probe(i, total, p, rc, out, err, ok):
    print(f"[{i}/{total}] {p.name}")
    print(f"  itf={p.itf}  table={p.table}")
    if p.note:
        print(f"  note: {p.note}")
    print(f"  SQL: {p.sql}")
    if p.filters:
        print(f"  filters: {json.dumps(p.filters, ensure_ascii=False)}")
    if ok:
        print("  OK")
        preview = "\n".join("    " + line for line in out.splitlines()[:6])
        if preview:
            print(preview)
    else:
        print(f"  FAIL  rc={rc}")
        if err:
            print(f"  stderr: {err.strip()[:600]}")
        if out:
            print(f"  stdout: {out.strip()[:600]}")
    print()


def main():
    kw = sys.argv[1] if len(sys.argv) > 1 else None
    sel = [
        p for p in PROBES if kw is None or kw in p.name or kw in p.table or kw in p.itf
    ]
    assert sel, f"无匹配 probe: {kw}"

    print(f"BigQuant DAI 接口探测  ({len(sel)}/{len(PROBES)})  D={D}  INST={INST}\n")
    n_ok = 0
    fails = []
    for i, p in enumerate(sel, 1):
        rc, out, err = _run(p)
        ok = rc == 0 and "|" in out
        if ok:
            n_ok += 1
        else:
            fails.append(p)
        _print_probe(i, len(sel), p, rc, out, err, ok)

    print("=" * 60)
    print(f"通过: {n_ok}/{len(sel)}")
    if fails:
        print(f"失败 ({len(fails)}):")
        for p in fails:
            print(f"  - {p.table:<48} itf={p.itf:<32} {p.name}")

    assert not fails, f"有 {len(fails)} 个 probe 失败"


if __name__ == "__main__":
    main()


"""
/bin/python /home/chuyin/work/qmt/doc/bigquant/probe.py
chuyin@chuyin:~/work/qmt$ /bin/python /home/chuyin/work/qmt/doc/bigquant/probe.py
BigQuant DAI 接口探测  (28/28)  D=2024-12-31  INST=000001.SZ

[1/28] 交易日历 SSE/SZSE
  itf=trade_cal  table=trading_days
  note: 替代 trade_cal; market_code 过滤 SSE/SZSE
  SQL: SELECT * FROM trading_days WHERE date='2024-12-31'
  OK
    | date                | market_code   |
    |---------------------|---------------|
    | 2024-12-31 00:00:00 | CN            |
    | 2024-12-31 00:00:00 | HK            |
    | 2024-12-31 00:00:00 | US            |
    | 2024-12-31 00:00:00 | SG            |

[2/28] 股票全量主表 (含退市)
  itf=stock_basic / _meta  table=cn_stock_basic_info
  note: L+D+P+G; list_sector 表征 主板/创业板/科创板
  SQL: SELECT instrument,name,exchange,list_sector,list_date,delist_date,industry,corp_nature FROM cn_stock_basic_info WHERE instrument='000001.SZ'
  OK
    | instrument   | name     | exchange       | list_sector   | list_date           | delist_date   | industry            | corp_nature   |
    |--------------|----------|----------------|---------------|---------------------|---------------|---------------------|---------------|
    | 000001.SZ    | 平安银行 | 深圳证券交易所 | 1             | 1991-04-03 00:00:00 | NaT           | 金融业-货币金融服务 | 其他          |

[3/28] 代码 ID 映射 (辅助)
  itf=all_instruments  table=all_instruments
  note: instrument <-> instrument_id 双向
  SQL: SELECT * FROM all_instruments LIMIT 5
  OK
    | instrument   | instrument_id   | product_code   | product_name   |
    |--------------|-----------------|----------------|----------------|
    | 000001.SZ    | 1               | 1              | 股票           |
    | 000002.SZ    | 2               | 1              | 股票           |
    | 000003.SZ    | 3               | 1              | 股票           |
    | 000004.SZ    | 4               | 1              | 股票           |

[4/28] 股票曾用名段
  itf=namechange  table=cn_stock_name_change
  note: (instrument, start_date, end_date, name)
  SQL: SELECT * FROM cn_stock_name_change WHERE instrument='000001.SZ'
  OK
    | instrument   | start_date          | end_date            | name     |
    |--------------|---------------------|---------------------|----------|
    | 000001.SZ    | 2005-01-04 00:00:00 | 2006-10-08 00:00:00 | 深发展A  |
    | 000001.SZ    | 2006-10-09 00:00:00 | 2007-06-19 00:00:00 | S深发展A |
    | 000001.SZ    | 2007-06-20 00:00:00 | 2012-08-01 00:00:00 | 深发展A  |
    | 000001.SZ    | 2012-08-02 00:00:00 | 2024-09-02 00:00:00 | 平安银行 |

[5/28] 行业归属 SW2021 PIT
  itf=index_member_all / _meta  table=cn_stock_industry_component
  note: PIT 历史归属 (date 列), 优于 tushare 当前快照
  SQL: SELECT instrument,industry_level1_code,industry_level1_name,industry_level2_name,industry_level3_name FROM cn_stock_industry_component WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | instrument   | industry_level1_code   | industry_level1_name   | industry_level2_name   | industry_level3_name   |
    |--------------|------------------------|------------------------|------------------------|------------------------|
    | 000001.SZ    | 400000                 | 银行                   | 全国性股份制银行Ⅱ      | 全国性股份制银行Ⅲ      |
    | 000001.SZ    | 480000                 | 银行                   | 银行                   | 银行                   |
    | 000001.SZ    | 480000                 | 银行                   | 股份制银行Ⅱ            | 股份制银行Ⅲ            |

[6/28] 收盘价 后复权 + 网格 OHLCV
  itf=bar1d (后复权)  table=cn_stock_bar1d
  note: 项目前复权口径需切换或自算
  SQL: SELECT date,instrument,adjust_factor,pre_close,open,close,high,low,volume,amount,change_ratio,turn,upper_limit,lower_limit FROM cn_stock_bar1d WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | adjust_factor      | pre_close          | open               | close             | high               | low               | volume    | amount        | change_ratio        | turn                 | upper_limit        | lower_limit        |
    |---------------------|--------------|--------------------|--------------------|--------------------|-------------------|--------------------|-------------------|-----------|---------------|---------------------|----------------------|--------------------|--------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 127.78445003181974 | 1527.0241778802458 | 1524.4684888796096 | 1495.078065372291 | 1532.1355558815187 | 1495.078065372291 | 147536733 | 1747242074.23 | -0.0209205020920502 | 0.007602784749679933 | 1680.3655179184298 | 1374.9606823423803 |

[7/28] 收盘价 未复权
  itf=bar1d (未复权)  table=cn_stock_real_bar1d
  note: mcap_raw = real_close × total_shares
  SQL: SELECT date,instrument,close,turn FROM cn_stock_real_bar1d WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | close   | turn                 |
    |---------------------|--------------|---------|----------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 11.7    | 0.007602784749679933 |

[8/28] 涨跌停价
  itf=stk_limit  table=cn_stock_limit_price
  note: 完全对应 tushare stk_limit
  SQL: SELECT * FROM cn_stock_limit_price WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | upper_limit   | lower_limit   |
    |---------------------|--------------|---------------|---------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 13.15         | 10.76         |

[9/28] 停复牌记录
  itf=suspend_d  table=cn_stock_suspend
  note: 含 suspend_period + suspend_reason
  SQL: SELECT * FROM cn_stock_suspend WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | suspend_period   | suspend_reason   |
    |--------|--------------|------------------|------------------|

[10/28] ST 三态 + 风险警示 + 状态
  itf=stock_st  table=cn_stock_status
  note: ★ st_status TINYINT 0/1/2 直接 boolean
  SQL: SELECT date,instrument,st_status,is_risk_warning,suspended,price_limit_status,exdr FROM cn_stock_status WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | st_status   | is_risk_warning   | suspended   | price_limit_status   | exdr   |
    |---------------------|--------------|-------------|-------------------|-------------|----------------------|--------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 0           | 0                 | 0           | 2                    | 0      |

[11/28] 股票静态信息 (盘前快照)
  itf=static_data (盘前)  table=cn_stock_static_data
  note: crd_buy/sell_flag 双向标识替代 margin_secs
  SQL: SELECT date,instrument,pre_close,upper_limit,lower_limit,adjust_factor,suspended,st_status,exchange,in_delist,crd_buy_flag,crd_sell_flag,public_float_share FROM cn_stock_static_data WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | pre_close   | upper_limit   | lower_limit   | adjust_factor      | suspended   | st_status   | exchange   | in_delist   | crd_buy_flag   | crd_sell_flag   | public_float_share   |
    |---------------------|--------------|-------------|---------------|---------------|--------------------|-------------|-------------|------------|-------------|----------------|-----------------|----------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 11.95       | 13.15         | 10.76         | 127.78445003181974 | 0           | 0           | SZSE       | 0           | 1              | 1               | 19405571850          |

[12/28] 个股估值 PE/PB/PS/PCF/DY
  itf=daily_basic (估值)  table=cn_stock_valuation
  note: ★ 替代 daily_basic; 5 个估值 raw + 2 个市值 齐全
  SQL: SELECT date,instrument,total_market_cap,float_market_cap,pe_ttm,pb,ps_ttm,pcf_net_ttm,dividend_yield_ratio FROM cn_stock_valuation WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  FAIL  rc=1
  stderr: Error: 没有权限读取数据源 cn_stock_valuation

[13/28] 股本数据
  itf=shares (辅助 mcap)  table=cn_stock_shares
  note: total_shares / a_float_shares / free_float_shares / total_float_shares
  SQL: SELECT * FROM cn_stock_shares WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | total_shares   | a_float_shares   | free_float_shares   | total_float_shares   |
    |---------------------|--------------|----------------|------------------|---------------------|----------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 19405918198.0  | 19405617528.0    | 8600976804.0        | 19405617528.0        |

[14/28] 融资融券明细 (个股)
  itf=margin_detail  table=cn_stock_margin_trading_detail
  note: financing_balance ↔ rzye, securities_lending_balance ↔ rqye
  SQL: SELECT date,instrument,financing_balance,securities_lending_balance,financing_purchase,financing_repayment,margin_trading_balance FROM cn_stock_margin_trading_detail WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | financing_balance   | securities_lending_balance   | financing_purchase   | financing_repayment   | margin_trading_balance   |
    |---------------------|--------------|---------------------|------------------------------|----------------------|-----------------------|--------------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 4580936570.0        | 5566860.0                    | 333627972.0          | 231734584.0           | 4586503430.0             |

[15/28] 融资融券市场统计 (辅助)
  itf=margin_market  table=cn_stock_margin_trading_market
  note: 全市场总融资融券, 备查
  SQL: SELECT * FROM cn_stock_margin_trading_market
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | method   | financing_balance   | financing_quantity   | financing_balance_ratio   | financing_purchase   | financing_purchase_quantity   | financing_repayment   | financing_repayment_quantity   | financing_net_purchase   | financing_net_purchase_quantity   | securities_lending_balance   | securities_lending_quantity   | securities_lending_sales   | securities_lending_sales_quantity   | securities_lending_repayment   | securities_lending_repayment_quantity   | securities_lending_net_sales   | securities_lending_net_sales_quantity   | margin_trading_balance   |
    |---------------------|----------|---------------------|----------------------|---------------------------|----------------------|-------------------------------|-----------------------|--------------------------------|--------------------------|-----------------------------------|------------------------------|-------------------------------|----------------------------|-------------------------------------|--------------------------------|-----------------------------------------|--------------------------------|-----------------------------------------|--------------------------|
    | 2024-12-31 00:00:00 | sum      | 1740224404220.0     | 138904462467.0       | 0.024312097710502648      | 108846357821.0       | 8566037449.0                  | 124619080070.0        | 9562855619.0                   | -15772722249.0           | -996818170.0                      | 5927380657.440001            | 479141239.0                   | 449084702.46999997         | 35571390.0                          | 398489841.21999997             | 33673813.0                              | 50594861.24999999              | 1897577.0                               | 1746151784877.44         |
    | 2024-12-31 00:00:00 | mean     | 474822484.0982265   | 37900262.61036835    | 6.6335873698506546e-06    | 29698869.801091406   | 2337254.4199181446            | 34002477.508867666    | 2609237.5495225103             | -4303607.7077762615      | -271983.1296043656                | 1617293.4945266033           | 130734.30804911323            | 122533.34310231922         | 9705.699863574351                   | 108728.46963710777             | 9187.943519781718                       | 13804.873465211458             | 517.7563437926331                       | 476439777.59275305       |

[16/28] 业绩预告 (公司发)
  itf=forecast_vip  table=cn_stock_profit_estimate
  note: ★ profit_st/revenue_st 触发源
  SQL: SELECT date,instrument,begin_date,end_date,fore_profit_min,fore_profit_max,fore_type,ex_date FROM cn_stock_profit_estimate WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | begin_date   | end_date   | fore_profit_min   | fore_profit_max   | fore_type   | ex_date   |
    |--------|--------------|--------------|------------|-------------------|-------------------|-------------|-----------|

[17/28] 业绩超预期 鉴定
  itf=express_vip (exceed appraisal)  table=cn_stock_profit_exceed_appraisal
  note: 项目未入张量, 仅校验接口
  SQL: SELECT date,instrument,report_date,publish_date,fore_profit,profit_exceed_rate,appraisal_result FROM cn_stock_profit_exceed_appraisal WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | publish_date   | fore_profit   | profit_exceed_rate   | appraisal_result   |
    |--------|--------------|---------------|----------------|---------------|----------------------|--------------------|

[18/28] 业绩超预期 (券商研报)
  itf=express_vip (exceed expect)  table=cn_stock_profit_exceed_expect
  SQL: SELECT date,instrument,report_date,below_type,below_desc,estimate_profit,profit FROM cn_stock_profit_exceed_expect WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | below_type   | below_desc   | estimate_profit   | profit   |
    |--------|--------------|---------------|--------------|--------------|-------------------|----------|

[19/28] 业绩低于预期
  itf=express_vip (below)  table=cn_stock_profit_below_expect
  SQL: SELECT date,instrument,report_date,below_type,below_desc,estimate_profit,profit FROM cn_stock_profit_below_expect WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | below_type   | below_desc   | estimate_profit   | profit   |
    |--------|--------------|---------------|--------------|--------------|-------------------|----------|

[20/28] 财报实际披露日 (含修订)
  itf=disclosure_date (actual)  table=cn_stock_financial_changedate
  note: 替代 disclosure_date.actual_date; 无事先 ann_date 计划
  SQL: SELECT * FROM cn_stock_financial_changedate WHERE instrument='000001.SZ' LIMIT 10
  OK
    | instrument   | report_date         | changedate            | statement_type   |
    |--------------|---------------------|-----------------------|------------------|
    | 000001.SZ    | 2005-03-31 00:00:00 | 2005-04-26            | balance_sheet    |
    | 000001.SZ    | 2005-03-31 00:00:00 | 2005-04-26            | income           |
    | 000001.SZ    | 2005-03-31 00:00:00 | 2005-04-26            | cash_flow        |
    | 000001.SZ    | 2005-06-30 00:00:00 | 2005-08-19            | balance_sheet    |

[21/28] 分红送股 (实施)
  itf=dividend  table=cn_stock_dividend
  note: 仅实施版无 div_proc 三阶段; PK = (instrument, report_date)
  SQL: SELECT * FROM cn_stock_dividend WHERE instrument='000001.SZ'
  OK
    | date                | instrument   | report_date         | publish_date        | bonus_rate   | conversed_rate   | cash_before_tax   | cash_after_tax   | register_date       | ex_date             |
    |---------------------|--------------|---------------------|---------------------|--------------|------------------|-------------------|------------------|---------------------|---------------------|
    | 2008-10-31 00:00:00 | 000001.SZ    | 2008-06-30 00:00:00 | 2008-08-21 00:00:00 | 0.3          | nan              | 0.0335            | 0.00015          | 2008-10-30 00:00:00 | 2008-10-31 00:00:00 |
    | 2012-10-19 00:00:00 | 000001.SZ    | 2012-06-30 00:00:00 | 2012-08-16 00:00:00 | nan          | nan              | 0.1               | 0.09             | 2012-10-18 00:00:00 | 2012-10-19 00:00:00 |
    | 2013-06-20 00:00:00 | 000001.SZ    | 2012-12-31 00:00:00 | 2013-03-08 00:00:00 | 0.6          | nan              | 0.17              | 0.1315           | 2013-06-19 00:00:00 | 2013-06-20 00:00:00 |
    | 2014-06-12 00:00:00 | 000001.SZ    | 2013-12-31 00:00:00 | 2014-03-07 00:00:00 | nan          | 0.2              | 0.16              | 0.152            | 2014-06-11 00:00:00 | 2014-06-12 00:00:00 |

[22/28] 分析师一致预期 (rolling, 辅助)
  itf=(辅助, 非 forecast 替代)  table=cn_stock_financial_forecast_consensus_rolling
  note: 不替代 forecast_vip (公司预告), 是分析师外推
  SQL: SELECT date,instrument,forecast_eps_fy1,forecast_np_fy1,forecast_revenue_fy1,forecast_np_yoy FROM cn_stock_financial_forecast_consensus_rolling WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | forecast_eps_fy1   | forecast_np_fy1   | forecast_revenue_fy1   | forecast_np_yoy   |
    |---------------------|--------------|--------------------|-------------------|------------------------|-------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 2.3523             | 46806058181.818   | 146455455909.09        | 0.75569514975395  |

[23/28] 财务分析-盈利能力 (PIT TTM)
  itf=fina_indicator (roe/roa)  table=cn_stock_financial_profitability
  note: ★ 直接 _ttm 字段, 砍 ttm4_ytd 自算
  SQL: SELECT date,instrument,report_date,shift,roe_avg_ttm,roa_avg_ttm,roe_period_ttm,roic_ttm,gross_profit_rate_ttm,net_profit_rate_ttm FROM cn_stock_financial_profitability WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   | roe_avg_ttm   | roa_avg_ttm   | roe_period_ttm   | roic_ttm   | gross_profit_rate_ttm   | net_profit_rate_ttm   |
    |--------|--------------|---------------|---------|---------------|---------------|------------------|------------|-------------------------|-----------------------|

[24/28] 利润表 PIT 原始
  itf=income_vip (pit raw)  table=cn_stock_financial_income_general_pit
  note: PIT 原生 (date=visible_date); change_type 修订追踪
  SQL: SELECT date,instrument,report_date,change_type,operating_revenue,net_profit,net_profit_to_parent_shareholders FROM cn_stock_financial_income_general_pit WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | change_type   | operating_revenue   | net_profit   | net_profit_to_parent_shareholders   |
    |--------|--------------|---------------|---------------|---------------------|--------------|-------------------------------------|

[25/28] 利润表 TTM 衍生
  itf=income_vip (ttm)  table=cn_stock_financial_ttm_shift
  note: ★ ttm 直接给, 砍 ttm4_ytd 自算
  SQL: SELECT date,instrument,report_date,shift,total_operating_revenue_ttm,operating_revenue_ttm,net_profit_ttm,net_profit_to_parent_shareholders_ttm FROM cn_stock_financial_ttm_shift WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   | total_operating_revenue_ttm   | operating_revenue_ttm   | net_profit_ttm   | net_profit_to_parent_shareholders_ttm   |
    |--------|--------------|---------------|---------|-------------------------------|-------------------------|------------------|-----------------------------------------|

[26/28] 利润表 LF 衍生 (最新一期)
  itf=income_vip (lf)  table=cn_stock_financial_lf_shift
  note: lf_shift 用于自算 PB (净资产) 等; 大表 313 列, 仅探活
  SQL: SELECT date,instrument,report_date,shift FROM cn_stock_financial_lf_shift WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   |
    |--------|--------------|---------------|---------|

[27/28] 现金流量表 PIT 原始
  itf=cashflow_vip (pit raw)  table=cn_stock_financial_cashflow_general_pit
  note: net_cffoa = 经营性现金流净额
  SQL: SELECT date,instrument,report_date,change_type,net_cffoa,net_cffia,net_cfffa,netinc_in_cce FROM cn_stock_financial_cashflow_general_pit WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | change_type   | net_cffoa   | net_cffia   | net_cfffa   | netinc_in_cce   |
    |--------|--------------|---------------|---------------|-------------|-------------|-------------|-----------------|

[28/28] 现金流 TTM 衍生
  itf=cashflow_vip (ttm)  table=cn_stock_financial_ttm_shift
  note: pcf_raw = mcap / net_cffoa_ttm
  SQL: SELECT date,instrument,report_date,shift,net_cffoa_ttm,net_cffia_ttm,net_cfffa_ttm FROM cn_stock_financial_ttm_shift WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   | net_cffoa_ttm   | net_cffia_ttm   | net_cfffa_ttm   |
    |--------|--------------|---------------|---------|-----------------|-----------------|-----------------|

============================================================
通过: 27/28
失败 (1):
  - cn_stock_valuation                               itf=daily_basic (估值)                 个股估值 PE/PB/PS/PCF/DY
Traceback (most recent call last):
  File "/home/chuyin/work/qmt/doc/bigquant/probe.py", line 317, in <module>
    main()
  File "/home/chuyin/work/qmt/doc/bigquant/probe.py", line 313, in main
    assert not fails, f"有 {len(fails)} 个 probe 失败"
           ^^^^^^^^^
AssertionError: 有 1 个 probe 失败
chuyin@chuyin:~/work/qmt$ 
"""
