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
        "cn_stock_industry_real_bar1d",
        "cn_stock_industry_real_bar1d",
        "SELECT * FROM cn_stock_industry_real_bar1d LIMIT 5",
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
        "股票不复权日行情 (OHLCV)",
        "cn_stock_real_bar1d",
        "cn_stock_real_bar1d",
        f"SELECT date,instrument,adjust_factor,pre_close,open,close,high,low,volume,amount,change_ratio,turn,upper_limit,lower_limit FROM cn_stock_real_bar1d WHERE instrument='{INST}'",
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
        "★ shift=0 取最新一期; ttm 直接给, 砍 ttm12 自算",
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
chuyin@chuyin:~/work/qmt$ /bin/python /home/chuyin/work/qmt/doc/bigquant/probe.py
BigQuant DAI 接口探测  (26/26)  D=2024-12-31  INST=000001.SZ

[1/26] 交易日历
  itf=trading_days  table=trading_days
  note: 全球交易市场日历; market_code 过滤 SSE/SZSE
  SQL: SELECT * FROM trading_days WHERE date='2024-12-31'
  OK
    | date                | market_code   |
    |---------------------|---------------|
    | 2024-12-31 00:00:00 | CN            |
    | 2024-12-31 00:00:00 | HK            |
    | 2024-12-31 00:00:00 | US            |
    | 2024-12-31 00:00:00 | SG            |

[2/26] 节假日
  itf=holidays  table=holidays
  note: 更新频率日频
  SQL: SELECT * FROM holidays LIMIT 5
  OK
    | date                | market_code   |
    |---------------------|---------------|
    | 2005-01-03 00:00:00 | CN            |
    | 2005-02-07 00:00:00 | CN            |
    | 2005-02-08 00:00:00 | CN            |
    | 2005-02-09 00:00:00 | CN            |

[3/26] 中国股票代码列表
  itf=cn_stock_instruments  table=cn_stock_instruments
  note: 全市场股票代码 + 中文简称 + type
  SQL: SELECT * FROM cn_stock_instruments WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | name     | type   |
    |---------------------|--------------|----------|--------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 平安银行 | stock  |

[4/26] 股票行业成分 (SW2021 PIT)
  itf=cn_stock_industry_component  table=cn_stock_industry_component
  note: PIT 历史归属 (date 列)
  SQL: SELECT instrument,industry_level1_code,industry_level1_name,industry_level2_name,industry_level3_name FROM cn_stock_industry_component WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | instrument   | industry_level1_code   | industry_level1_name   | industry_level2_name   | industry_level3_name   |
    |--------------|------------------------|------------------------|------------------------|------------------------|
    | 000001.SZ    | 400000                 | 银行                   | 全国性股份制银行Ⅱ      | 全国性股份制银行Ⅲ      |
    | 000001.SZ    | 480000                 | 银行                   | 银行                   | 银行                   |
    | 000001.SZ    | 480000                 | 银行                   | 股份制银行Ⅱ            | 股份制银行Ⅲ            |

[5/26] 行业进出记录
  itf=cn_stock_industry_change  table=cn_stock_industry_change
  note: 更新频率日频
  SQL: SELECT * FROM cn_stock_industry_change WHERE instrument='000001.SZ' LIMIT 5
  filters: {"date": ["2024-01-01", "2024-12-31"]}
  OK
    | date   | instrument   | industry   | industry_level   | industry_code   | industry_name   | change_flag   |
    |--------|--------------|------------|------------------|-----------------|-----------------|---------------|

[6/26] 行业日线数据
  itf=cn_stock_industry_real_bar1d  table=cn_stock_industry_real_bar1d
  note: 行业 OHLCV; instrument=行业代码
  SQL: SELECT * FROM cn_stock_industry_real_bar1d LIMIT 5
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | method   | pre_close          | open               | close              | high              | low                | volume    | deal_number   | amount        | change_ratio           | turn                 |
    |---------------------|--------------|----------|--------------------|--------------------|--------------------|-------------------|--------------------|-----------|---------------|---------------|------------------------|----------------------|
    | 2024-12-31 00:00:00 | 101010       | 1        | 9.386666666666667  | 9.413333333333334  | 9.386666666666667  | 9.53              | 9.348333333333334  | 515947360 | 278506        | 4008507876.87 | 0.0                    | 0.016096247062480026 |
    | 2024-12-31 00:00:00 | 101010       | 2        | 12.545940395138766 | 12.574209446499589 | 12.504144324761146 | 12.64941215913488 | 12.466558177203202 | 515947360 | 278506        | 4008507876.87 | -0.0033314418099590856 | 0.016096247062480026 |
    | 2024-12-31 00:00:00 | 101010       | 3        | 8.835595292818114  | 8.854686539912215  | 8.783069841260708  | 8.894762665673529 | 8.761137071392422  | 515947360 | 278506        | 4008507876.87 | -0.005944755256060721  | 0.016096247062480026 |
    | 2024-12-31 00:00:00 | 102010       | 1        | 4.354              | 4.359999999999999  | 4.316              | 4.412             | 4.295999999999999  | 227251249 | 137517        | 1348754710.79 | -0.008727606798346407  | 0.009998506667030843 |

[7/26] 行业估值
  itf=cn_stock_industry_valuation  table=cn_stock_industry_valuation
  note: 申万 + 中信一二级行业 PE/PB
  SQL: SELECT * FROM cn_stock_industry_valuation LIMIT 5
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | industry   | industry_level   | industry_code   | industry_name   | component_nums   | pe_trailing   | pe_ttm   | pb   |
    |---------------------|--------------|------------|------------------|-----------------|-----------------|------------------|---------------|----------|------|
    | 2024-12-31 00:00:00 | 100000       | cs         | 1                | 100000          | 石油石化        | 37               | 11.4          | 13.19    | 0.13 |
    | 2024-12-31 00:00:00 | 101000       | cs         | 2                | 101000          | 石油开采Ⅱ       | 6                | 9.1           | 9.67     | 0.1  |
    | 2024-12-31 00:00:00 | 102000       | cs         | 2                | 102000          | 石油化工        | 20               | 13.81         | 19.19    | 0.17 |
    | 2024-12-31 00:00:00 | 103000       | cs         | 2                | 103000          | 油服工程        | 11               | 17.17         | 18.25    | 0.39 |

[8/26] 股票基本信息
  itf=stock_basic / _meta  table=cn_stock_basic_info
  note: L+D+P+G; list_sector 表征 主板/创业板/科创板
  SQL: SELECT instrument,name,exchange,list_sector,list_date,delist_date,industry,corp_nature FROM cn_stock_basic_info WHERE instrument='000001.SZ'
  OK
    | instrument   | name     | exchange       | list_sector   | list_date           | delist_date   | industry            | corp_nature   |
    |--------------|----------|----------------|---------------|---------------------|---------------|---------------------|---------------|
    | 000001.SZ    | 平安银行 | 深圳证券交易所 | 1             | 1991-04-03 00:00:00 | NaT           | 金融业-货币金融服务 | 其他          |

[9/26] 股本信息 (历史变动)
  itf=cn_stock_capital  table=cn_stock_capital
  note: 股本变动记录 (publish_date, change_date, reason)
  SQL: SELECT * FROM cn_stock_capital WHERE instrument='000001.SZ' LIMIT 5
  OK
    | instrument   | publish_date        | change_date         | reason   | total_shares   | float_a     | float_b   | restricted_a   | restricted_b   | prefer_shares   | shares_hk   | shares_aboard   | shares_a_total   | shares_b_total   | float_total   | rest_total   |
    |--------------|---------------------|---------------------|----------|----------------|-------------|-----------|----------------|----------------|-----------------|-------------|-----------------|------------------|------------------|---------------|--------------|
    | 000001.SZ    | 1991-04-03 00:00:00 | 1991-04-03 00:00:00 | 34       | 48500171.0     | 26500000.0  | 0.0       | 0.0            | 0.0            | nan             | 0.0         | 0.0             | 26500000.0       | 0.0              | 26500000.0    | 0.0          |
    | 000001.SZ    | 1991-12-31 00:00:00 | 1991-12-31 00:00:00 | 27       | 89751643.0     | 39490723.0  | 0.0       | 0.0            | 0.0            | nan             | 0.0         | 0.0             | 39490723.0       | 0.0              | 39490723.0    | 0.0          |
    | 000001.SZ    | 1994-06-30 00:00:00 | 1994-06-30 00:00:00 | 11       | 269417899.0    | 179122251.0 | 0.0       | 0.0            | 0.0            | nan             | 0.0         | 0.0             | 179122251.0      | 0.0              | 179122251.0   | 0.0          |
    | 000001.SZ    | 1994-07-02 00:00:00 | 1994-08-22 00:00:00 | 21       | 431068638.0    | 286595634.0 | 0.0       | 0.0            | 0.0            | nan             | 0.0         | 0.0             | 286595634.0      | 0.0              | 286595634.0   | 0.0          |

[10/26] 分红送股 (实施)
  itf=dividend  table=cn_stock_dividend
  note: PK = (instrument, report_date); 仅实施版无 div_proc 三阶段
  SQL: SELECT * FROM cn_stock_dividend WHERE instrument='000001.SZ' LIMIT 5
  OK
    | date                | instrument   | report_date         | publish_date        | bonus_rate   | conversed_rate   | cash_before_tax   | cash_after_tax   | register_date       | ex_date             |
    |---------------------|--------------|---------------------|---------------------|--------------|------------------|-------------------|------------------|---------------------|---------------------|
    | 2008-10-31 00:00:00 | 000001.SZ    | 2008-06-30 00:00:00 | 2008-08-21 00:00:00 | 0.3          | nan              | 0.0335            | 0.00015          | 2008-10-30 00:00:00 | 2008-10-31 00:00:00 |
    | 2012-10-19 00:00:00 | 000001.SZ    | 2012-06-30 00:00:00 | 2012-08-16 00:00:00 | nan          | nan              | 0.1               | 0.09             | 2012-10-18 00:00:00 | 2012-10-19 00:00:00 |
    | 2013-06-20 00:00:00 | 000001.SZ    | 2012-12-31 00:00:00 | 2013-03-08 00:00:00 | 0.6          | nan              | 0.17              | 0.1315           | 2013-06-19 00:00:00 | 2013-06-20 00:00:00 |
    | 2014-06-12 00:00:00 | 000001.SZ    | 2013-12-31 00:00:00 | 2014-03-07 00:00:00 | nan          | 0.2              | 0.16              | 0.152            | 2014-06-11 00:00:00 | 2014-06-12 00:00:00 |

[11/26] 配股信息
  itf=cn_stock_allotment  table=cn_stock_allotment
  note: 更新频率日频
  SQL: SELECT * FROM cn_stock_allotment WHERE instrument='000001.SZ' LIMIT 5
  OK
    | date   | instrument   | publish_date   | allotment_price   | allotment_rate   | allotment_shares   | register_date   | exright_date   | allot_listdate   |
    |--------|--------------|----------------|-------------------|------------------|--------------------|-----------------|----------------|------------------|

[12/26] 融资融券明细 (个股)
  itf=margin_detail  table=cn_stock_margin_trading_detail
  note: financing_balance ↔ rzye, securities_lending_balance ↔ rqye
  SQL: SELECT date,instrument,financing_balance,securities_lending_balance,financing_purchase,financing_repayment,margin_trading_balance FROM cn_stock_margin_trading_detail WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | financing_balance   | securities_lending_balance   | financing_purchase   | financing_repayment   | margin_trading_balance   |
    |---------------------|--------------|---------------------|------------------------------|----------------------|-----------------------|--------------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 4580936570.0        | 5566860.0                    | 333627972.0          | 231734584.0           | 4586503430.0             |

[13/26] 融资融券市场统计
  itf=margin_market  table=cn_stock_margin_trading_market
  note: 全市场总融资融券; method=sum/mean
  SQL: SELECT * FROM cn_stock_margin_trading_market
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | method   | financing_balance   | financing_quantity   | financing_balance_ratio   | financing_purchase   | financing_purchase_quantity   | financing_repayment   | financing_repayment_quantity   | financing_net_purchase   | financing_net_purchase_quantity   | securities_lending_balance   | securities_lending_quantity   | securities_lending_sales   | securities_lending_sales_quantity   | securities_lending_repayment   | securities_lending_repayment_quantity   | securities_lending_net_sales   | securities_lending_net_sales_quantity   | margin_trading_balance   |
    |---------------------|----------|---------------------|----------------------|---------------------------|----------------------|-------------------------------|-----------------------|--------------------------------|--------------------------|-----------------------------------|------------------------------|-------------------------------|----------------------------|-------------------------------------|--------------------------------|-----------------------------------------|--------------------------------|-----------------------------------------|--------------------------|
    | 2024-12-31 00:00:00 | sum      | 1740224404220.0     | 138904462467.0       | 0.024312097710502648      | 108846357821.0       | 8566037449.0                  | 124619080070.0        | 9562855619.0                   | -15772722249.0           | -996818170.0                      | 5927380657.440001            | 479141239.0                   | 449084702.46999997         | 35571390.0                          | 398489841.21999997             | 33673813.0                              | 50594861.24999999              | 1897577.0                               | 1746151784877.44         |
    | 2024-12-31 00:00:00 | mean     | 474822484.0982265   | 37900262.61036835    | 6.6335873698506546e-06    | 29698869.801091406   | 2337254.4199181446            | 34002477.508867666    | 2609237.5495225103             | -4303607.7077762615      | -271983.1296043656                | 1617293.4945266033           | 130734.30804911323            | 122533.34310231922         | 9705.699863574351                   | 108728.46963710777             | 9187.943519781718                       | 13804.873465211458             | 517.7563437926331                       | 476439777.59275305       |

[14/26] 股东户数
  itf=cn_stock_shareholder  table=cn_stock_shareholder
  note: publish_date / end_date; 无 date 列
  SQL: SELECT * FROM cn_stock_shareholder WHERE instrument='000001.SZ' LIMIT 5
  OK
    | publish_date        | instrument   | end_date            | total_shareholder   | total_shareholder_chg   | a_shareholder   | a_shareholder_chg     | avg_share_per_account_total   | avg_share_per_account_total_chg   | avg_share_ratio_per_account_total   | avg_share_ratio_per_account_total_chg   | avg_share_per_account_float   | avg_share_per_account_float_chg   | avg_share_ratio_per_account_float   | avg_share_ratio_per_account_float_chg   |
    |---------------------|--------------|---------------------|---------------------|-------------------------|-----------------|-----------------------|-------------------------------|-----------------------------------|-------------------------------------|-----------------------------------------|-------------------------------|-----------------------------------|-------------------------------------|-----------------------------------------|
    | 2005-04-26 00:00:00 | 000001.SZ    | 2004-12-31 00:00:00 | 666196.0            | 0.0                     | 666196.0        | 0.0                   | 2920.7953049853195            | 0.0                               | 1.5010597481822165e-06              | 0.0                                     | 2115.536516280494             | 0.0                               | 1.5010597481822167e-06              | 0.0                                     |
    | 2005-04-26 00:00:00 | 000001.SZ    | 2005-03-31 00:00:00 | 658855.0            | -0.011019279611405697   | 658855.0        | -0.011019279611405697 | 2953.338972915133             | 0.011142057053524779              | 1.5177846415372124e-06              | 0.011142057053524779                    | 2139.1079448437063            | 0.011142057053524557              | 1.5177846415372124e-06              | 0.011142057053524779                    |
    | 2005-08-19 00:00:00 | 000001.SZ    | 2005-06-30 00:00:00 | 645701.0            | -0.019964939174780483   | 645701.0        | -0.019964939174780483 | 3013.503384693535             | 0.020371658089425315              | 1.5487044313079894e-06              | 0.020371658089425315                    | 2182.6851205124353            | 0.020371658089425315              | 1.5487044313079894e-06              | 0.020371658089425315                    |
    | 2005-10-29 00:00:00 | 000001.SZ    | 2005-09-30 00:00:00 | 630989.0            | -0.022784539593403097   | 630989.0        | -0.022784539593403097 | 3083.7655632665546            | 0.023315778880455884              | 1.5848136813795487e-06              | 0.023315778880455884                    | 2233.5761241479645            | 0.023315778880455884              | 1.5848136813795485e-06              | 0.023315778880455884                    |

[15/26] 股本数据 (日频)
  itf=shares (辅助 mcap)  table=cn_stock_shares
  note: total_shares / a_float_shares / free_float_shares / total_float_shares
  SQL: SELECT * FROM cn_stock_shares WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | total_shares   | a_float_shares   | free_float_shares   | total_float_shares   |
    |---------------------|--------------|----------------|------------------|---------------------|----------------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 19405918198.0  | 19405617528.0    | 8600976804.0        | 19405617528.0        |

[16/26] 股票状态 (ST/停牌/涨跌停)
  itf=stock_st  table=cn_stock_status
  note: ★ st_status TINYINT 0/1/2 直接 boolean
  SQL: SELECT date,instrument,st_status,is_risk_warning,suspended,price_limit_status,exdr FROM cn_stock_status WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | st_status   | is_risk_warning   | suspended   | price_limit_status   | exdr   |
    |---------------------|--------------|-------------|-------------------|-------------|----------------------|--------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 0           | 0                 | 0           | 2                    | 0      |

[17/26] 停复牌记录
  itf=suspend_d  table=cn_stock_suspend
  note: 含 suspend_period + suspend_reason
  SQL: SELECT * FROM cn_stock_suspend WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | suspend_period   | suspend_reason   |
    |--------|--------------|------------------|------------------|

[18/26] 证券简称变更
  itf=namechange  table=cn_stock_name_change
  note: (instrument, start_date, end_date, name); 无 date 列
  SQL: SELECT * FROM cn_stock_name_change WHERE instrument='000001.SZ'
  OK
    | instrument   | start_date          | end_date            | name     |
    |--------------|---------------------|---------------------|----------|
    | 000001.SZ    | 2005-01-04 00:00:00 | 2006-10-08 00:00:00 | 深发展A  |
    | 000001.SZ    | 2006-10-09 00:00:00 | 2007-06-19 00:00:00 | S深发展A |
    | 000001.SZ    | 2007-06-20 00:00:00 | 2012-08-01 00:00:00 | 深发展A  |
    | 000001.SZ    | 2012-08-02 00:00:00 | 2024-09-02 00:00:00 | 平安银行 |

[19/26] 龙虎榜
  itf=cn_stock_dragon_list  table=cn_stock_dragon_list
  note: 上榜事件, 非每股每日有
  SQL: SELECT * FROM cn_stock_dragon_list LIMIT 5
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | reason                                                                   | close   | price_change   | net_buy_amount   | buy_amount   | sell_amount   | deal_amount   | total_deal_amount   | net_buy_ratio   | deal_amount_ratio   | turn    | float_market_cap   | price_change_1d   | price_change_2d   | price_change_5d   |
    |---------------------|--------------|--------------------------------------------------------------------------|---------|----------------|------------------|--------------|---------------|---------------|---------------------|-----------------|---------------------|---------|--------------------|-------------------|-------------------|-------------------|
    | 2024-12-31 00:00:00 | 000592.SZ    | 日涨幅偏离值达到7%的前5只证券                                            | 2.87    | 9.9617         | 89172917.0       | 119600330.0  | 30427413.0    | 150027743.0   | 622541920.0         | 14.324001988493 | 24.099219374657     | 11.8092 | 5496315328.63      | 4.18118467        | 5.92334495        | 9.75609756        |
    | 2024-12-31 00:00:00 | 000669.SZ    | 连续三个交易日内，涨幅偏离值累计达到12%的ST证券、*ST证券和未完成股改证券 | 2.11    | 4.9751         | 4893834.6        | 19795863.0   | 14902028.4    | 34697891.4    | 120205269.0         | 4.071231353428  | 28.86553284116      | 4.154   | 1435662561.67      | -5.21327014       | -0.9478673        | 7.10900474        |
    | 2024-12-31 00:00:00 | 000670.SZ    | 日跌幅偏离值达到7%的前5只证券                                            | 8.32    | -9.9567        | 46841566.08      | 128475931.69 | 81634365.61   | 210110297.3   | 1035358342.0        | 4.524188793371  | 20.293485721488     | 16.7064 | 6007927843.84      | -9.375            | -16.22596154      | -16.70673077      |
    | 2024-12-31 00:00:00 | 000679.SZ    | 连续三个交易日内，涨幅偏离值累计达到20%的证券                            | 6.54    | 9.7315         | 3756960.3        | 95573699.63  | 91816739.33   | 187390438.96  | 782142505.0         | 0.480342172428  | 23.958605722368     | 21.0364 | 2330856000.0       | 7.03363914        | -3.51681957       | -13.76146789      |

[20/26] 股票不复权日行情 (OHLCV)
  itf=cn_stock_real_bar1d  table=cn_stock_real_bar1d
  note: 项目前复权口径需切换或自算
  SQL: SELECT date,instrument,adjust_factor,pre_close,open,close,high,low,volume,amount,change_ratio,turn,upper_limit,lower_limit FROM cn_stock_real_bar1d WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | adjust_factor      | pre_close   | open   | close   | high   | low   | volume    | amount        | change_ratio          | turn                 | upper_limit   | lower_limit   |
    |---------------------|--------------|--------------------|-------------|--------|---------|--------|-------|-----------|---------------|-----------------------|----------------------|---------------|---------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 127.78445003181974 | 11.95       | 11.93  | 11.7    | 11.99  | 11.7  | 147536733 | 1747242074.23 | -0.020920502092050212 | 0.007602784749679933 | 13.15         | 10.76         |

[21/26] 股票涨跌停价格
  itf=stk_limit  table=cn_stock_limit_price
  note: 完全对应 tushare stk_limit
  SQL: SELECT * FROM cn_stock_limit_price WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date                | instrument   | upper_limit   | lower_limit   |
    |---------------------|--------------|---------------|---------------|
    | 2024-12-31 00:00:00 | 000001.SZ    | 13.15         | 10.76         |

[22/26] 利润表 PIT (一般企业)
  itf=income_vip (pit raw)  table=cn_stock_financial_income_general_pit
  note: PIT 原生 (date=visible_date); change_type 修订追踪
  SQL: SELECT date,instrument,report_date,change_type,operating_revenue,net_profit,net_profit_to_parent_shareholders FROM cn_stock_financial_income_general_pit WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | change_type   | operating_revenue   | net_profit   | net_profit_to_parent_shareholders   |
    |--------|--------------|---------------|---------------|---------------------|--------------|-------------------------------------|

[23/26] 现金流量表 PIT (一般企业)
  itf=cashflow_vip (pit raw)  table=cn_stock_financial_cashflow_general_pit
  note: net_cffoa = 经营性现金流净额
  SQL: SELECT date,instrument,report_date,change_type,net_cffoa,net_cffia,net_cfffa,netinc_in_cce FROM cn_stock_financial_cashflow_general_pit WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | change_type   | net_cffoa   | net_cffia   | net_cfffa   | netinc_in_cce   |
    |--------|--------------|---------------|---------------|-------------|-------------|-------------|-----------------|

[24/26] 资产负债表 PIT (一般企业)
  itf=balance_vip (pit raw)  table=cn_stock_financial_balance_general_pit
  note: PIT 原生; total_equity_to_parent_shareholders 用于自算 PB
  SQL: SELECT date,instrument,report_date,change_type,total_assets,total_liabilities,total_owner_equity,total_equity_to_parent_shareholders FROM cn_stock_financial_balance_general_pit WHERE instrument='000001.SZ'
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | change_type   | total_assets   | total_liabilities   | total_owner_equity   | total_equity_to_parent_shareholders   |
    |--------|--------------|---------------|---------------|----------------|---------------------|----------------------|---------------------------------------|

[25/26] 财务衍生 TTM (利润+现金流)
  itf=ttm_shift  table=cn_stock_financial_ttm_shift
  note: ★ shift=0 取最新一期; ttm 直接给, 砍 ttm12 自算
  SQL: SELECT date,instrument,report_date,shift,operating_revenue_ttm,net_profit_ttm,net_profit_to_parent_shareholders_ttm,net_cffoa_ttm,net_cffia_ttm,net_cfffa_ttm FROM cn_stock_financial_ttm_shift WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   | operating_revenue_ttm   | net_profit_ttm   | net_profit_to_parent_shareholders_ttm   | net_cffoa_ttm   | net_cffia_ttm   | net_cfffa_ttm   |
    |--------|--------------|---------------|---------|-------------------------|------------------|-----------------------------------------|-----------------|-----------------|-----------------|

[26/26] 财务衍生 (财务附注 LF/MRQ/TTM)
  itf=notes_shift  table=cn_stock_financial_notes_shift
  note: 财务附注 LF/MRQ/TTM 三套衍生
  SQL: SELECT date,instrument,report_date,shift,nonrecurring_income_sum_lf,nonrecurring_income_sum_mrq,nonrecurring_income_sum_ttm FROM cn_stock_financial_notes_shift WHERE instrument='000001.SZ' AND shift=0
  filters: {"date": ["2024-12-31", "2024-12-31"]}
  OK
    | date   | instrument   | report_date   | shift   | nonrecurring_income_sum_lf   | nonrecurring_income_sum_mrq   | nonrecurring_income_sum_ttm   |
    |--------|--------------|---------------|---------|------------------------------|-------------------------------|-------------------------------|

============================================================
通过: 26/26
"""
