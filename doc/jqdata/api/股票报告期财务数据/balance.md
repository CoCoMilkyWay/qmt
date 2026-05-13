合并资产负债表、母公司资产负债表

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

from jqdatasdk import finance
finance.run_query(query(finance.STK_BALANCE_SHEET).filter(finance.STK_BALANCE_SHEET.code==code).limit(n))

描述

获取上市公司定期公告中公布的合并资产负债表（2007版）

获取上市公司定期公告中公布的母公司资产负债表（2007版）

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

合并资产负债表参数

query(finance.STK_BALANCE_SHEET)：表示从finance.STK_BALANCE_SHEET这张表中查询上市公司定期公告中公布的合并资产负债表信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.STK_BALANCE_SHEET：代表上市公司合并资产负债表信息，收录了上市公司定期公告中公布的合并资产负债表数据，表结构和字段信息如下：

filter(finance.STK_BALANCE_SHEET.code==code)：指定筛选条件，通过finance.STK_BALANCE_SHEET.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_BALANCE_SHEET.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日上市公司公布的合并资产负债表信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

母公司资产负债表参数

query(finance.STK_BALANCE_SHEET_PARENT)：表示从finance.STK_BALANCE_SHEET_PARENT这张表中查询上市公司定期公告中公布的母公司资产负债表信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.STK_BALANCE_SHEET_PARENT：代表上市公司母公司资产负债表信息，收录了上市公司定期公告中公布的母公司资产负债表数据，表结构和字段信息如下：

filter(finance.STK_BALANCE_SHEET_PARENT.code==code)：指定筛选条件，通过finance.STK_BALANCE_SHEET_PARENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_BALANCE_SHEET_PARENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日上市公司公布的母公司资产负债表信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段名称	中文名称	字段类型	含义

company_id	公司ID	int

company_name	公司名称	varchar(100)

code	股票代码	varchar(12)

a_code	A股代码	varchar(12)

b_code	B股代码	varchar(12)

h_code	H股代码	varchar(12)

pub_date	公告日期	date

end_date	截止日期	date

report_date	报告期	date

report_type	报告期类型	int	0本期，1上期

source_id	报表来源编码	int	如下 报表来源编码

source	报表来源	varchar(60)

cash_equivalents	货币资金	decimal(20,4)

trading_assets	交易性金融资产	decimal(20,4)

bill_receivable	应收票据	decimal(20,4)

account_receivable	应收账款	decimal(20,4)

advance_payment	预付款项	decimal(20,4)

other_receivable	其他应收款	decimal(20,4)

affiliated_company_receivable	应收关联公司款	decimal(20,4)

interest_receivable	应收利息	decimal(20,4)

dividend_receivable	应收股利	decimal(20,4)

inventories	存货	decimal(20,4)

expendable_biological_asset	消耗性生物资产	decimal(20,4)	消耗性生物资产，是指为出售而持有的、或在将来收获为农产品的生物资产，包括生长中的大田作物、蔬菜、用材林，以及存栏代售的牲畜等

non_current_asset_in_one_year	一年内到期的非流动资产	decimal(20,4)

total_current_assets	流动资产合计	decimal(20,4)

hold_for_sale_assets	可供出售金融资产	decimal(20,4)

hold_to_maturity_investments	持有至到期投资	decimal(20,4)

longterm_receivable_account	长期应收款	decimal(20,4)

longterm_equity_invest	长期股权投资	decimal(20,4)

investment_property	投资性房地产	decimal(20,4)

fixed_assets	固定资产	decimal(20,4)

constru_in_process	在建工程	decimal(20,4)

construction_materials	工程物资	decimal(20,4)

fixed_assets_liquidation	固定资产清理	decimal(20,4)

biological_assets	生产性生物资产	decimal(20,4)

oil_gas_assets	油气资产	decimal(20,4)

intangible_assets	无形资产	decimal(20,4)

development_expenditure	开发支出	decimal(20,4)

good_will	商誉	decimal(20,4)

long_deferred_expense	长期待摊费用	decimal(20,4)

deferred_tax_assets	递延所得税资产	decimal(20,4)

total_non_current_assets	非流动资产合计	decimal(20,4)

total_assets	资产总计	decimal(20,4)

shortterm_loan	短期借款	decimal(20,4)

trading_liability	交易性金融负债	decimal(20,4)

notes_payable	应付票据	decimal(20,4)

accounts_payable	应付账款	decimal(20,4)

advance_peceipts	预收款项	decimal(20,4)

salaries_payable	应付职工薪酬	decimal(20,4)

taxs_payable	应交税费	decimal(20,4)

interest_payable	应付利息	decimal(20,4)

dividend_payable	应付股利	decimal(20,4)

other_payable	其他应付款	decimal(20,4)

affiliated_company_payable	应付关联公司款	decimal(20,4)

non_current_liability_in_one_year	一年内到期的非流动负债	decimal(20,4)

total_current_liability	流动负债合计	decimal(20,4)

longterm_loan	长期借款	decimal(20,4)

bonds_payable	应付债券	decimal(20,4)

longterm_account_payable	长期应付款	decimal(20,4)

specific_account_payable	专项应付款	decimal(20,4)

estimate_liability	预计负债	decimal(20,4)

deferred_tax_liability	递延所得税负债	decimal(20,4)

total_non_current_liability	非流动负债合计	decimal(20,4)

total_liability	负债合计	decimal(20,4)

paidin_capital	实收资本（或股本）	decimal(20,4)

capital_reserve_fund	资本公积	decimal(20,4)

specific_reserves	专项储备	decimal(20,4)

surplus_reserve_fund	盈余公积	decimal(20,4)

treasury_stock	库存股	decimal(20,4)

retained_profit	未分配利润	decimal(20,4)

equities_parent_company_owners	归属于母公司所有者权益	decimal(20,4)

minority_interests	少数股东权益	decimal(20,4)

foreign_currency_report_conv_diff	外币报表折算价差	decimal(20,4)

irregular_item_adjustment	非正常经营项目收益调整	decimal(20,4)

total_owner_equities	所有者权益（或股东权益）合计	decimal(20,4)

total_sheet_owner_equities	负债和所有者权益（或股东权益）合计	decimal(20,4)

other_comprehensive_income	其他综合收益	decimal(20,4)

deferred_earning	递延收益-非流动负债	decimal(20,4)

settlement_provi	结算备付金	decimal(20,4)

lend_capital	拆出资金	decimal(20,4)

loan_and_advance_current_assets	发放贷款及垫款-流动资产	decimal(20,4)

derivative_financial_asset	衍生金融资产	decimal(20,4)

insurance_receivables	应收保费	decimal(20,4)

reinsurance_receivables	应收分保账款	decimal(20,4)

reinsurance_contract_

reserves_receivable	应收分保合同准备金	decimal(20,4)

bought_sellback_assets	买入返售金融资产	decimal(20,4)

hold_sale_asset	划分为持有待售的资产	decimal(20,4)

loan_and_advance_noncurrent_assets	发放贷款及垫款-非流动资产	decimal(20,4)

borrowing_from_centralbank	向中央银行借款	decimal(20,4)

deposit_in_interbank	吸收存款及同业存放	decimal(20,4)

borrowing_capital	拆入资金	decimal(20,4)

derivative_financial_liability	衍生金融负债	decimal(20,4)

sold_buyback_secu_proceeds	卖出回购金融资产款	decimal(20,4)

commission_payable	应付手续费及佣金	decimal(20,4)

reinsurance_payables	应付分保账款	decimal(20,4)

insurance_contract_reserves	保险合同准备金	decimal(20,4)

proxy_secu_proceeds	代理买卖证券款	decimal(20,4)

receivings_from_vicariously

_sold_securities	代理承销证券款	decimal(20,4)

hold_sale_liability	划分为持有待售的负债	decimal(20,4)

estimate_liability_current	预计负债-流动负债	decimal(20,4)

deferred_earning_current	递延收益-流动负债	decimal(20,4)

preferred_shares_noncurrent	优先股-非流动负债	decimal(20,4)

pepertual_liability_noncurrent	永续债-非流动负债	decimal(20,4)

longterm_salaries_payable	长期应付职工薪酬	decimal(20,4)

other_equity_tools	其他权益工具	decimal(20,4)

preferred_shares_equity	其中：优先股-所有者权益	decimal(20,4)

pepertual_liability_equity	永续债-所有者权益	decimal(20,4)

other_current_assets	其他流动资产	decimal(20,4)

other_non_current_assets	其他非流动资产	decimal(20,4)

other_current_liability	其他流动负债	decimal(20,4)

other_non_current_liability	其他非流动负债	decimal(20,4)

ordinary_risk_reserve_fund	一般风险准备	decimal(20,4)

contract_assets	合同资产	decimal(20,4)

bond_invest	债权投资	decimal(20,4)

other_bond_invest	其他债权投资	decimal(20,4)

other_equity_tools_invest	其他权益工具投资	decimal(20,4)

other_non_current_financial_assets	其他非流动金融资产	decimal(20,4)

contract_liability	合同负债	decimal(20,4)

receivable_fin	应收款项融资	decimal(20,4)

usufruct_assets	使用权资产	decimal(20,4)

bill_and_account_payable	应付票据及应付账款	decimal(20,4)

bill_and_account_receivable	应收票据及应收账款	decimal(20,4)

lease_liability	租赁负债	decimal(20,4)

报表来源编码

编码	名称

321001	招募说明书

321002	上市公告书

321003	定期报告

321004	预披露公告

321005	换股报告书

321099	其他

示例

获取上市公司合并资产负债表数据

#查询贵州茅台2015年之后公告的合并资产负债表数据，取出本期的货币资金，总资产和总负债
from jqdatasdk import *
q=query(finance.STK_BALANCE_SHEET.company_name,
finance.STK_BALANCE_SHEET.code,
finance.STK_BALANCE_SHEET.pub_date,
finance.STK_BALANCE_SHEET.start_date,
finance.STK_BALANCE_SHEET.end_date,
finance.STK_BALANCE_SHEET.cash_equivalents,
finance.STK_BALANCE_SHEET.total_assets,
finance.STK_BALANCE_SHEET.total_liability
).filter(finance.STK_BALANCE_SHEET.code=='600519.XSHG',finance.STK_BALANCE_SHEET.pub_date>='2015-01-01',finance.STK_BALANCE_SHEET.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

company_name         code    pub_date  start_date    end_date  \
0  贵州茅台酒股份有限公司  600519.XSHG  2015-04-21  2014-01-01  2014-12-31
1  贵州茅台酒股份有限公司  600519.XSHG  2015-04-21  2015-01-01  2015-03-31
2  贵州茅台酒股份有限公司  600519.XSHG  2015-08-28  2015-01-01  2015-06-30
3  贵州茅台酒股份有限公司  600519.XSHG  2015-10-23  2015-01-01  2015-09-30
4  贵州茅台酒股份有限公司  600519.XSHG  2016-03-24  2015-01-01  2015-12-31
5  贵州茅台酒股份有限公司  600519.XSHG  2016-04-21  2016-01-01  2016-03-31
6  贵州茅台酒股份有限公司  600519.XSHG  2016-08-27  2016-01-01  2016-06-30
7  贵州茅台酒股份有限公司  600519.XSHG  2016-10-29  2016-01-01  2016-09-30

cash_equivalents  total_assets  total_liability
0      2.771072e+10  6.587317e+10     1.056161e+10
1      2.842068e+10  6.876902e+10     8.838873e+09
2      3.023650e+10  7.233774e+10     8.675962e+09
3      3.053612e+10  7.755903e+10     1.564019e+10
4      3.680075e+10  8.630146e+10     2.006729e+10
5      4.377574e+10  9.069045e+10     1.974919e+10
6      4.752806e+10  9.554650e+10     2.819334e+10
7      6.199974e+10  1.051460e+11     3.386253e+10

获取上市公司母公司资产负债表的信息

#查询贵州茅台2015年之后公告的母公司资产负债表数据，取出本期的货币资金，总资产和总负债
from jqdatasdk import *
q=query(finance.STK_BALANCE_SHEET_PARENT.company_name,
finance.STK_BALANCE_SHEET_PARENT.code,
finance.STK_BALANCE_SHEET_PARENT.pub_date,
finance.STK_BALANCE_SHEET_PARENT.start_date,
finance.STK_BALANCE_SHEET_PARENT.end_date,
finance.STK_BALANCE_SHEET_PARENT.cash_equivalents,
finance.STK_BALANCE_SHEET_PARENT.total_assets,
finance.STK_BALANCE_SHEET_PARENT.total_liability
).filter(finance.STK_BALANCE_SHEET_PARENT.code=='600519.XSHG',finance.STK_BALANCE_SHEET_PARENT.pub_date>='2015-01-01',finance.STK_BALANCE_SHEET_PARENT.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

company_name         code    pub_date  start_date    end_date  \
0  贵州茅台酒股份有限公司  600519.XSHG  2015-04-21  2014-01-01  2014-12-31
1  贵州茅台酒股份有限公司  600519.XSHG  2015-04-21  2015-01-01  2015-03-31
2  贵州茅台酒股份有限公司  600519.XSHG  2015-08-28  2015-01-01  2015-06-30
3  贵州茅台酒股份有限公司  600519.XSHG  2015-10-23  2015-01-01  2015-09-30
4  贵州茅台酒股份有限公司  600519.XSHG  2016-03-24  2015-01-01  2015-12-31
5  贵州茅台酒股份有限公司  600519.XSHG  2016-04-21  2016-01-01  2016-03-31
6  贵州茅台酒股份有限公司  600519.XSHG  2016-08-27  2016-01-01  2016-06-30
7  贵州茅台酒股份有限公司  600519.XSHG  2016-10-29  2016-01-01  2016-09-30

cash_equivalents  total_assets  total_liability
0      1.070530e+10  4.662489e+10     1.693767e+10
1      1.165218e+10  4.903731e+10     1.940782e+10
2      1.239635e+10  5.079041e+10     2.125881e+10
3      8.754779e+09  5.396424e+10     1.979558e+10
4      1.505296e+10  5.512518e+10     2.082189e+10
5      1.574722e+10  6.608276e+10     2.292887e+10
6      1.898219e+10  6.186466e+10     2.658035e+10
7      1.540346e+10  5.697338e+10     2.241995e+10
