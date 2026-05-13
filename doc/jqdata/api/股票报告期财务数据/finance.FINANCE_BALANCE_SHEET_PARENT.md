金融类上市公司资产负债表、母公司资产负债表

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

from jqdatasdk import finance
finance.run_query(query(finance.FINANCE_BALANCE_SHEET).filter(finance.FINANCE_BALANCE_SHEET.code==code).limit(n))

描述

获取金融类上市公司的合并资产负债表信息

获取金融类上市公司的母公司资产负债表信息

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

金融类公司合并资产负债参数

query(finance.FINANCE_BALANCE_SHEET)：表示从finance.FINANCE_BALANCE_SHEET这张表中查询金融类上市公司合并资产负债的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_BALANCE_SHEET：代表金融类上市公司合并资产负债表，收录了金融类上市公司的合并资产负债，表结构和字段信息如下：

filter(finance.FINANCE_BALANCE_SHEET.code==code)：指定筛选条件，通过finance.FINANCE_BALANCE_SHEET.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_BALANCE_SHEET.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的合并资产负债信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

金融类母公司资产负债表参数

query(finance.FINANCE_BALANCE_SHEET_PARENT)：表示从finance.FINANCE_BALANCE_SHEET_PARENT这张表中查询金融类上市公司母公司资产负债的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_BALANCE_SHEET_PARENT：代表金融类上市公司母公司资产负债表，收录了金融类上市公司的母公司资产负债，表结构和字段信息如下：

filter(finance.FINANCE_BALANCE_SHEET_PARENT.code==code：指定筛选条件，通过finance.FINANCE_BALANCE_SHEET_PARENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_BALANCE_SHEET_PARENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的母公司资产负债信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段名称	中文名称	字段类型	含义

company_id	公司ID	int

company_name	公司名称	varchar(100)

code	公司主证券代码	varchar(12)

a_code	A股代码	varchar(12)

b_code	B股代码	varchar(12)

h_code	H股代码	varchar(12)

pub_date	公告日期	date

start_date	开始日期	date

end_date	截止日期	date

report_date	报告期	date

report_type	报告期类型	int	0本期，1上期

source_id	报表来源编码	int	如下报表编码表

source	报表来源	varchar(60)

deposit_in_ib	存放同业款项	decimal(20,4)

cash_equivalents	货币资金	decimal(20,4)

deposit_client	客户资金存款	decimal(20,4)

cash_in_cb	现金及存放中央银行款项	decimal(20,4)

settlement_provi	结算备付金	decimal(20,4)

settlement_provi_client	客户备付金	decimal(20,4)

metal	贵金属	decimal(20,4)

lend_capital	拆出资金	decimal(20,4)

fairvalue_fianancial_asset	以公允价值计量且其变动计入当期损益的金融资产	decimal(20,4)

other_grow_asset	衍生金融资产	decimal(20,4)

bought_sellback_assets	买入返售金融资产	decimal(20,4)

interest_receivable	应收利息	decimal(20,4)

insurance_receivables	应收保费	decimal(20,4)

recover_receivable	应收代位追偿款	decimal(20,4)

separate_receivable	应收分保帐款	decimal(20,4)

not_time_fund	应收分保未到期责任准备金	decimal(20,4)

not_decide_fund	应收分保未决赔款准备金	decimal(20,4)

response_fund	应收分保寿险责任准备金	decimal(20,4)

health_fund	应收分保长期健康险责任准备金	decimal(20,4)

margin_loan	保户质押贷款	decimal(20,4)

deposit_period	定期存款	decimal(20,4)

loan_and_advance	发放贷款及垫款	decimal(20,4)

margin_out	存出保证金	decimal(20,4)

agent_asset	代理业务资产	decimal(20,4)

investment_reveiable	应收款项类投资	decimal(20,4)

advance_payment	预付款项	decimal(20,4)

hold_for_sale_assets	可供出售金融资产	decimal(20,4)

hold_to_maturity_investments	持有至到期投资	decimal(20,4)

longterm_equity_invest	长期股权投资	decimal(20,4)

finance_out	融出资金	decimal(20,4)

capital_margin_out	存出资本保证金	decimal(20,4)

investment_property	投资性房地产	decimal(20,4)

inventories	存货	decimal(20,4)

fixed_assets	固定资产	decimal(20,4)

constru_in_process	在建工程	decimal(20,4)

intangible_assets	无形资产	decimal(20,4)

trade_fee	交易席位费	decimal(20,4)

long_deferred_expense	长期待摊费用	decimal(20,4)

fixed_assets_liquidation	固定资产清理	decimal(20,4)

independent_account_asset	独立帐户资产	decimal(20,4)

deferred_tax_assets	递延所得税资产	decimal(20,4)

other_asset	其他资产	decimal(20,4)

total_assets	资产总计	decimal(20,4)

borrowing_from_centralbank	向中央银行借款	decimal(20,4)

deposit_in_ib_and_other	同业及其他金融机构存放款项	decimal(20,4)

shortterm_loan	短期借款	decimal(20,4)

loan_pledge	其中：质押借款	decimal(20,4)

borrowing_capital	拆入资金	decimal(20,4)

fairvalue_financial_liability	以公允价值计量且其变动计入当期损益的金融负债	decimal(20,4)

derivative_financial_liability	衍生金融负债	decimal(20,4)

sold_buyback_secu_proceeds	卖出回购金融资产款	decimal(20,4)

deposit_absorb	吸收存款	decimal(20,4)

proxy_secu_proceeds	代理买卖证券款	decimal(20,4)

proxy_sell_proceeds	代理承销证券款	decimal(20,4)

accounts_payable	应付账款	decimal(20,4)

notes_payable	应付票据	decimal(20,4)

advance_peceipts	预收款项	decimal(20,4)

insurance_receive_early	预收保费	decimal(20,4)

commission_payable	应付手续费及佣金	decimal(20,4)

insurance_payable	应付分保帐款	decimal(20,4)

salaries_payable	应付职工薪酬	decimal(20,4)

taxs_payable	应交税费	decimal(20,4)

interest_payable	应付利息	decimal(20,4)

proxy_liability	代理业务负债	decimal(20,4)

estimate_liability	预计负债	decimal(20,4)

compensation_payable	应付赔付款	decimal(20,4)

interest_insurance_payable	应付保单红利	decimal(20,4)

investment_money	保户储金及投资款	decimal(20,4)

not_time_reserve	未到期责任准备金	decimal(20,4)

not_decide_reserve	未决赔款准备金	decimal(20,4)

live_reserve	寿险责任准备金	decimal(20,4)

longterm_reserve	长期健康险责任准备金	decimal(20,4)

longterm_loan	长期借款	decimal(20,4)

bonds_payable	应付债券	decimal(20,4)

independent_account	独立帐户负债	decimal(20,4)

deferred_tax_liability	递延所得税负债	decimal(20,4)

other_liability	其他负债	decimal(20,4)

total_liability	负债合计	decimal(20,4)

paidin_capital	实收资本(或股本)	decimal(20,4)

capital_reserve_fund	资本公积	decimal(20,4)

treasury_stock	减：库存股	decimal(20,4)

surplus_reserve_fund	盈余公积	decimal(20,4)

equities_parent_company_owners	归属于母公司所有者权益	decimal(20,4)

retained_profit	未分配利润	decimal(20,4)

minority_interests	少数股东权益	decimal(20,4)

currency_mis	外币报表折算差额	decimal(20,4)

total_owner_equities	所有者权益合计	decimal(20,4)

total_liability_equity	负债和所有者权益总计	decimal(20,4)

perferred_share_liability	优先股-负债	decimal(20,4)

account_receivable	应收账款	decimal(20,4)

other_equity_tools	其他权益工具	decimal(20,4)

perferred_share_equity	优先股-权益	decimal(20,4)

pep_debt_equity	永续债-权益	decimal(20,4)

other_comprehensive_income	其他综合收益	decimal(20,4)

good_will	商誉	decimal(20,4)

shortterm_loan_payable	应付短期融资款	decimal(20,4)

accounts_payable	应付账款	decimal(20,4)

contract_assets	合同资产	decimal(20,4)

hold_sale_asset	持有待售资产	decimal(20,4)

bond_invest	债权投资	decimal(20,4)

other_bond_invest	其他债权投资	decimal(20,4)

other_equity_tools_invest	其他权益工具投资	decimal(20,4)

contract_liability	合同负债	decimal(20,4)

usufruct_assets	使用权资产	decimal(20,4)

liease_liability	租赁负债	decimal(20,4)

ordinary_risk_reserve_fund	一般风险准备	decimal(20,4)

other_operate_cash_paid	支付其他与经营活动有关的现金(元)	decimal(20, 4)

subtotal_operate_cash_outflow	经营活动现金流出小计(元)	decimal(20, 4)

net_operate_cash_flow	经营活动现金流量净额(元)	decimal(20, 4)

invest_cash_flow	投资活动产生的现金流量(元)	decimal(20, 4)

invest_withdrawal_cash	收回投资收到的现金(元)	decimal(20, 4)

invest_proceeds	取得投资收益收到的现金(元)	decimal(20, 4)

other_cash_from_invest_act	收到其他与投资活动有关的现金(元)	decimal(20, 4)

gain_from_disposal	处置固定资产、无形资产和其他长期资产所收回的现金(元)	decimal(20, 4)

subtotal_invest_cash_inflow	投资活动现金流入小计(元)	decimal(20, 4)

long_deferred_expense	长期待摊费用(元)	decimal(20, 4)

报表来源编码

编码	名称

321001	招募说明书

321002	上市公告书

321003	定期报告

321004	预披露公告

321005	换股报告书

321099	其他

示例

获取金融类上市公司合并资产负债表数据

#查询中国平安2015年之后公告的合并资产负债表数据，取出本期的货币资金，总资产和总负债
from jqdatasdk import finance
q=query(finance.FINANCE_BALANCE_SHEET.company_name,
finance.FINANCE_BALANCE_SHEET.code,
finance.FINANCE_BALANCE_SHEET.pub_date,
finance.FINANCE_BALANCE_SHEET.start_date,
finance.FINANCE_BALANCE_SHEET.end_date,
finance.FINANCE_BALANCE_SHEET.cash_equivalents,
finance.FINANCE_BALANCE_SHEET.total_assets,
finance.FINANCE_BALANCE_SHEET.total_liability
).filter(finance.FINANCE_BALANCE_SHEET.code=='601318.XSHG',finance.FINANCE_BALANCE_SHEET.pub_date>='2015-01-01',finance.FINANCE_BALANCE_SHEET.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

company_name         code    pub_date  start_date    end_date  \
0  中国平安保险(集团)股份有限公司  601318.XSHG  2015-03-20  2014-01-01  2014-12-31
1  中国平安保险(集团)股份有限公司  601318.XSHG  2015-04-30  2015-01-01  2015-03-31
2  中国平安保险(集团)股份有限公司  601318.XSHG  2015-08-21  2015-01-01  2015-06-30
3  中国平安保险(集团)股份有限公司  601318.XSHG  2015-10-28  2015-01-01  2015-09-30
4  中国平安保险(集团)股份有限公司  601318.XSHG  2016-03-16  2015-01-01  2015-12-31
5  中国平安保险(集团)股份有限公司  601318.XSHG  2016-04-27  2016-01-01  2016-03-31
6  中国平安保险(集团)股份有限公司  601318.XSHG  2016-08-18  2016-01-01  2016-06-30
7  中国平安保险(集团)股份有限公司  601318.XSHG  2016-10-28  2016-01-01  2016-09-30

cash_equivalents  total_assets  total_liability
0      4.427070e+11  4.005911e+12     3.652095e+12
1      4.131880e+11  4.215240e+12     3.833842e+12
2      4.510800e+11  4.632287e+12     4.227789e+12
3      4.654240e+11  4.667113e+12     4.262293e+12
4      4.750570e+11  4.765159e+12     4.351588e+12
5      5.668130e+11  5.006993e+12     4.566653e+12
6      5.210790e+11  5.219782e+12     4.757190e+12
7      5.230110e+11  5.296564e+12     4.815950e+12

获取金融类上市公司母公司资产负债表的信息

#查询中国平安2015年之后公告的母公司资产负债表数据，取出本期的货币资金，总资产和总负债
from jqdatasdk import finance
q=query(finance.FINANCE_BALANCE_SHEET_PARENT.company_name,
finance.FINANCE_BALANCE_SHEET_PARENT.code,
finance.FINANCE_BALANCE_SHEET_PARENT.pub_date,
finance.FINANCE_BALANCE_SHEET_PARENT.start_date,
finance.FINANCE_BALANCE_SHEET_PARENT.end_date,
finance.FINANCE_BALANCE_SHEET_PARENT.cash_equivalents,
finance.FINANCE_BALANCE_SHEET_PARENT.total_assets,
finance.FINANCE_BALANCE_SHEET_PARENT.total_liability
).filter(finance.FINANCE_BALANCE_SHEET_PARENT.code=='601318.XSHG',finance.FINANCE_BALANCE_SHEET_PARENT.pub_date>='2015-01-01',finance.FINANCE_BALANCE_SHEET_PARENT.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

company_name         code    pub_date  start_date    end_date  \
0  中国平安保险(集团)股份有限公司  601318.XSHG  2015-03-20  2014-01-01  2014-12-31
1  中国平安保险(集团)股份有限公司  601318.XSHG  2015-04-30  2015-01-01  2015-03-31
2  中国平安保险(集团)股份有限公司  601318.XSHG  2015-08-21  2015-01-01  2015-06-30
3  中国平安保险(集团)股份有限公司  601318.XSHG  2015-10-28  2015-01-01  2015-09-30
4  中国平安保险(集团)股份有限公司  601318.XSHG  2016-03-16  2015-01-01  2015-12-31
5  中国平安保险(集团)股份有限公司  601318.XSHG  2016-04-27  2016-01-01  2016-03-31
6  中国平安保险(集团)股份有限公司  601318.XSHG  2016-08-18  2016-01-01  2016-06-30
7  中国平安保险(集团)股份有限公司  601318.XSHG  2016-10-28  2016-01-01  2016-09-30

cash_equivalents  total_assets  total_liability
0      2.621400e+10  1.970230e+11     1.733100e+10
1      2.589900e+10  1.980580e+11     8.930000e+09
2      2.080900e+10  2.096090e+11     1.610500e+10
3      8.815000e+09  2.004830e+11     9.538000e+09
4      1.017900e+10  2.033480e+11     1.080500e+10
5      9.906000e+09  2.031290e+11     1.034900e+10
6      1.023400e+10  2.155980e+11     1.557700e+10
7      1.045300e+10  2.069940e+11     1.058500e+10
