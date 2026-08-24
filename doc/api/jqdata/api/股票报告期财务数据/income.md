合并利润表、母公司利润表

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

from jqdatasdk import finance
finance.run_query(query(finance.STK_INCOME_STATEMENT).filter(finance.STK_INCOME_STATEMENT.code==code).limit(n))

描述

获取上市公司定期公告中公布的合并利润表数据（2007版）

获取上市公司母公司利润的信息（2007版）

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

合并利润表参数

query(finance.STK_INCOME_STATEMENT)：表示从finance.STK_INCOME_STATEMENT这张表中查询上市公司定期公告中公布的合并利润表信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见:[query简易教程]

finance.STK_INCOME_STATEMENT：代表上市公司合并利润表，收录了上市公司定期公告中公布的合并利润表数据，表结构和字段信息如下：

filter(finance.STK_INCOME_STATEMENT_PARENT.code==code)：指定筛选条件，通过finance.STK_INCOME_STATEMENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_INCOME_STATEMENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日上市公司公布的合并利润表信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

母公司利润表参数

query(finance.STK_INCOME_STATEMENT_PARENT)：表示从finance.STK_INCOME_STATEMENT_PARENT这张表中查询上市公司母公司利润表的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

STK_INCOME_STATEMENT_PARENT：代表上市公司母公司利润表，收录了上市公司母公司的利润信息，表结构和字段信息如下：

filter(finance.STK_INCOME_STATEMENT_PARENT.code==code)：指定筛选条件，通过finance.STK_INCOME_STATEMENT_PARENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_INCOME_STATEMENT_PARENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日上市公司公布的母公司利润表信息；多个筛选条件用英文逗号分隔。

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

start_date	开始日期	date

end_date	截止日期	date

report_date	报告期	date

report_type	报告期类型	int	0：本期，1：上期

source_id	报表来源编码	int	如下 报表来源编码

source	报表来源	varchar(60)	选择时程序自动填入

total_operating_revenue	营业总收入	decimal(20,4)

operating_revenue	营业收入	decimal(20,4)

total_operating_cost	营业总成本	decimal(20,4)

operating_cost	营业成本	decimal(20,4)

operating_tax_surcharges	营业税金及附加	decimal(20,4)

sale_expense	销售费用	decimal(20,4)

administration_expense	管理费用	decimal(20,4)

exploration_expense	堪探费用	decimal(20,4)	勘探费用用于核算企业（石油天然气开采）核算的油气勘探过程中发生的地质调查、物理化学勘探各项支出和非成功探井等支出。

financial_expense	财务费用	decimal(20,4)

asset_impairment_loss	资产减值损失	decimal(20,4)

fair_value_variable_income	公允价值变动净收益	decimal(20,4)

investment_income	投资收益	decimal(20,4)

invest_income_associates	对联营企业和合营企业的投资收益	decimal(20,4)

exchange_income	汇兑收益	decimal(20,4)

other_items_influenced_income	影响营业利润的其他科目	decimal(20,4)

operating_profit	营业利润	decimal(20,4)

subsidy_income	补贴收入	decimal(20,4)

non_operating_revenue	营业外收入	decimal(20,4)

non_operating_expense	营业外支出	decimal(20,4)

disposal_loss_non_current_liability	非流动资产处置净损失	decimal(20,4)

other_items_influenced_profit	影响利润总额的其他科目	decimal(20,4)

total_profit	利润总额	decimal(20,4)

income_tax	所得税	decimal(20,4)

other_items_influenced_net_profit	影响净利润的其他科目	decimal(20,4)

net_profit	净利润	decimal(20,4)

np_parent_company_owners	归属于母公司所有者的净利润	decimal(20,4)

minority_profit	少数股东损益	decimal(20,4)

eps	每股收益	decimal(20,4)

basic_eps	基本每股收益	decimal(20,4)

diluted_eps	稀释每股收益	decimal(20,4)

other_composite_income	其他综合收益	decimal(20,4)

total_composite_income	综合收益总额	decimal(20,4)

ci_parent_company_owners	归属于母公司所有者的综合收益总额	decimal(20,4)

ci_minority_owners	归属于少数股东的综合收益总额	decimal(20,4)

interest_income	利息收入	decimal(20,4)

premiums_earned	已赚保费	decimal(20,4)

commission_income	手续费及佣金收入	decimal(20,4)

interest_expense	利息支出	decimal(20,4)

commission_expense	手续费及佣金支出	decimal(20,4)

refunded_premiums	退保金	decimal(20,4)

net_pay_insurance_claims	赔付支出净额	decimal(20,4)

withdraw_insurance_contract_reserve	提取保险合同准备金净额	decimal(20,4)

policy_dividend_payout	保单红利支出	decimal(20,4)

reinsurance_cost	分保费用	decimal(20,4)

non_current_asset_disposed	非流动资产处置利得	decimal(20,4)

other_earnings	其他收益	decimal(20,4)

asset_deal_income	资产处置收益	decimal(20,4)

sust_operate_net_profit	持续经营净利润	decimal(20,4)

discon_operate_net_profit	终止经营净利润	decimal(20,4)

credit_impairment_loss	信用减值损失	decimal(20,4)

net_open_hedge_income	净敞口套期收益	decimal(20,4)

interest_cost_fin	财务费用-利息费用	decimal(20,4)

interest_income_fin	财务费用-利息收入	decimal(20,4)

rd_expenses	研发费用	decimal(20,4)

报表来源编码

编码	名称

321001	招募说明书

321002	上市公告书

321003	定期报告

321004	预披露公告

321005	换股报告书

321099	其他

示例

获取上市公司合并利润表数据

#查询贵州茅台2015年之后公告的合并利润表数据，取出合并利润表中本期的营业总收入，归属于母公司的净利润
from jqdatasdk import finance
q=query(finance.STK_INCOME_STATEMENT.company_name,
finance.STK_INCOME_STATEMENT.code,
finance.STK_INCOME_STATEMENT.pub_date,
finance.STK_INCOME_STATEMENT.start_date,
finance.STK_INCOME_STATEMENT.end_date,
finance.STK_INCOME_STATEMENT.total_operating_revenue,
finance.STK_INCOME_STATEMENT.np_parent_company_owners).filter(finance.STK_INCOME_STATEMENT.code=='600519.XSHG',finance.STK_INCOME_STATEMENT.pub_date>='2015-01-01',finance.STK_INCOME_STATEMENT.report_type==0).limit(8)
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

total_operating_revenue  np_parent_company_owners
0             3.221721e+10              1.534980e+10
1             8.760368e+09              4.364902e+09
2             1.618565e+10              7.888232e+09
3             2.373432e+10              1.142464e+10
4             3.344686e+10              1.550309e+10
5             1.025087e+10              4.889272e+09
6             1.873762e+10              8.802637e+09
7             2.753274e+10              1.246558e+10

获取上市公司母公司利润的信息

#查询贵州茅台2015年之后公告的母公司利润表数据，取出母公司利润表中本期的营业总收入，归属于母公司所有者的净利润
from jqdatasdk import finance
q=query(finance.STK_INCOME_STATEMENT_PARENT.company_name,
finance.STK_INCOME_STATEMENT_PARENT.code,
finance.STK_INCOME_STATEMENT_PARENT.pub_date,
finance.STK_INCOME_STATEMENT_PARENT.start_date,
finance.STK_INCOME_STATEMENT_PARENT.end_date,
finance.STK_INCOME_STATEMENT_PARENT.total_operating_revenue,
finance.STK_INCOME_STATEMENT_PARENT.np_parent_company_owners).filter(finance.STK_INCOME_STATEMENT_PARENT.code=='600519.XSHG',finance.STK_INCOME_STATEMENT_PARENT.pub_date>='2015-01-01',finance.STK_INCOME_STATEMENT_PARENT.report_type==0).limit(8)
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

total_operating_revenue  np_parent_company_owners
0             6.878165e+09              1.028603e+10
1             1.886084e+09             -5.773331e+07
2             3.571872e+09             -1.556184e+08
3             5.411957e+09              9.476542e+09
4             8.843334e+09              9.611173e+09
5             1.507658e+09              8.850591e+09
6             3.608903e+09              8.733012e+09
7             5.430884e+09              8.002128e+09
