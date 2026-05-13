金融类上市公司现金流量表、母公司现金流量表

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

from jqdatasdk import finance
finance.run_query(query(finance.FINANCE_CASHFLOW_STATEMENT).filter(finance.FINANCE_CASHFLOW_STATEMENT.code==code).limit(n))

描述

获取金融类上市公司的合并现金流量表信息

获取金融类上市公司的母公司现金流量表信息

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

金融类公司合并现金流量表参数

query(finance.FINANCE_CASHFLOW_STATEMENT)：表示从finance.FINANCE_CASHFLOW_STATEMENT这张表中查询金融类上市公司合并现金流量的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_CASHFLOW_STATEMENT：代表金融类上市公司合并现金流量表，收录了金融类上市公司的合并现金流量，表结构和字段信息如下：

filter(finance.FINANCE_CASHFLOW_STATEMENT.code==code)：指定筛选条件，通过finance.FINANCE_CASHFLOW_STATEMENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_CASHFLOW_STATEMENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的合并现金流量信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

金融类上市公司母公司现金流量表参数

query(finance.FINANCE_CASHFLOW_STATEMENT_PARENT)：表示从finance.FINANCE_CASHFLOW_STATEMENT_PARENT这张表中查询金融类上市公司母公司现金流量的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_CASHFLOW_STATEMENT_PARENT：代表金融类上市公司母公司现金流量表，收录了金融类上市公司的母公司现金流量，表结构和字段信息如下：

filter(finance.FINANCE_CASHFLOW_STATEMENT_PARENT.code==code)：指定筛选条件，通过finance.FINANCE_CASHFLOW_STATEMENT_PARENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_CASHFLOW_STATEMENT_PARENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的母公司现金流量信息；多个筛选条件用英文逗号分隔。

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

source_id	报表来源编码	int	如下报表来源编码

source	报表来源	varchar(60)

operate_cash_flow	经营活动产生的现金流量	decimal(20,4)

net_loan_and_advance_decrease	客户贷款及垫款净减少额	decimal(20,4)

net_deposit_increase	客户存款和同业存放款项净增加额	decimal(20,4)

net_borrowing_from_central_bank	向中央银行借款净增加额	decimal(20,4)

net_deposit_in_cb_and_ib_de	存放中央银行和同业款项净减少额	decimal(20,4)

net_borrowing_from_finance_co	向其他金融机构拆入资金净增加额	decimal(20,4)

interest_and_commission_cashin	收取利息、手续费及佣金的现金	decimal(20,4)

trade_asset_increase	处置交易性金融资产净增加额	decimal(20,4)

net_increase_in_placements	拆入资金净增加额	decimal(20,4)

net_buyback	回购业务资金净增加额	decimal(20,4)

goods_sale_and_service_render_cash	销售商品、提供劳务收到的现金	decimal(20,4)

tax_levy_refund	收到的税费返还	decimal(20,4)

net_original_insurance_cash	收到原保险合同保费取得的现金	decimal(20,4)

insurance_cash_amount	收到再保业务现金净额	decimal(20,4)

net_insurer_deposit_investment	保户储金及投资款净增加额	decimal(20,4)

subtotal_operate_cash_inflow	经营活动现金流入小计	decimal(20,4)

net_loan_and_advance_increase	客户贷款及垫款净增加额	decimal(20,4)

saving_clients_decrease_amount	客户存放及同业存放款项净减少额	decimal(20,4)

net_deposit_in_cb_and_ib	存放中央银行和同业款项净增加额	decimal(20,4)

central_borrowing_decrease	向中央银行借款净减少额	decimal(20,4)

other_money_increase	向其他金融机构拆出资金净增加额	decimal(20,4)

purchase_trade_asset_increase	购入交易性金融资产净增加额	decimal(20,4)

repurchase_decrease	回购业务资金净减少额	decimal(20,4)

handling_charges_and_commission	支付利息、手续费及佣金的现金	decimal(20,4)

goods_and_services_cash_paid	购买商品、提供劳务支付的现金	decimal(20,4)

net_cash_re_insurance	支付再保业务现金净额	decimal(20,4)

reserve_investment_decrease	保户储金及投资款净减少额	decimal(20,4)

original_compensation_paid	支付原保险合同赔付款项的现金	decimal(20,4)

policy_dividend_cash_paid	支付保单红利的现金	decimal(20,4)

staff_behalf_paid	支付给职工以及为职工支付的现金	decimal(20,4)

tax_payments	支付的各项税费	decimal(20,4)

subtotal_operate_cash_outflow	经营活动现金流出小计	decimal(20,4)

net_operate_cash_flow	经营活动现金流量净额	decimal(20,4)

invest_cash_flow	投资活动产生的现金流量	decimal(20,4)

invest_withdrawal_cash	收回投资收到的现金	decimal(20,4)

invest_proceeds	取得投资收益收到的现金	decimal(20,4)

gain_from_disposal	处置固定资产、无形资产和其他长期资产所收回的现金	decimal(20,4)

subtotal_invest_cash_inflow	投资活动现金流入小计	decimal(20,4)

invest_cash_paid	投资支付的现金	decimal(20,4)

impawned_loan_net_increase	质押贷款净增加额	decimal(20,4)

fix_intan_other_asset_acqui_cash	购建固定资产、无形资产和其他长期资产支付的现金	decimal(20,4)

subtotal_invest_cash_outflow	投资活动现金流出小计	decimal(20,4)

net_invest_cash_flow	投资活动现金流量净额	decimal(20,4)

finance_cash_flow	筹资活动产生的现金流量	decimal(20,4)

cash_from_invest	吸收投资收到的现金	decimal(20,4)

cash_from_bonds_issue	发行债券收到的现金	decimal(20,4)

cash_from_borrowing	取得借款收到的现金	decimal(20,4)

subtotal_finance_cash_inflow	筹资活动现金流入小计	decimal(20,4)

borrowing_repayment	偿还债务支付的现金	decimal(20,4)

dividend_interest_payment	分配股利、利润或偿付利息支付的现金	decimal(20,4)

subtotal_finance_cash_outflow	筹资活动现金流出小计	decimal(20,4)

net_finance_cash_flow	筹资活动产生的现金流量净额	decimal(20,4)

exchange_rate_change_effect	汇率变动对现金的影响	decimal(20,4)

other_reason_effect_cash	其他原因对现金的影响	decimal(20,4)

cash_equivalent_increase	现金及现金等价物净增加额	decimal(20,4)

cash_equivalents_at_beginning	期初现金及现金等价物余额	decimal(20,4)

cash_and_equivalents_at_end	期末现金及现金等价物余额	decimal(20,4)

net_profit_cashflow_adjustment	将净利润调节为经营活动现金流量	decimal(20,4)

net_profit	净利润	decimal(20,4)

assets_depreciation_reserves	资产减值准备	decimal(20,4)

fixed_assets_depreciation	固定资产折旧、油气资产折耗、生产性生物资产折旧	decimal(20,4)

intangible_assets_amortization	无形资产摊销	decimal(20,4)

defferred_expense_amortization	长期待摊费用摊销	decimal(20,4)

fix_intan_other_asset_dispo_loss	处置固定资产、无形资产和其他长期资产的损失	decimal(20,4)

fixed_asset_scrap_loss	固定资产报废损失	decimal(20,4)

fair_value_change_loss	公允价值变动损失	decimal(20,4)

financial_cost	财务费用	decimal(20,4)

invest_loss	投资损失	decimal(20,4)

deffered_tax_asset_decrease	递延所得税资产减少	decimal(20,4)

deffered_tax_liability_increase	递延所得税负债增加	decimal(20,4)

inventory_decrease	存货的减少	decimal(20,4)

operate_receivables_decrease	经营性应收项目的减少	decimal(20,4)

operate_payable_increase	经营性应付项目的增加	decimal(20,4)

others	其他	decimal(20,4)

net_operate_cash_flow2	经营活动产生的现金流量净额_间接法	decimal(20,4)

activities_not_relate_major	不涉及现金收支的重大投资和筹资活动	decimal(20,4)

debt_to_capital	债务转为资本	decimal(20,4)

cbs_expiring_in_one_year	一年内到期的可转换公司债券	decimal(20,4)

financial_lease_fixed_assets	融资租入固定资产	decimal(20,4)

change_info_cash	现金及现金等价物净变动情况	decimal(20,4)

cash_at_end	现金的期末余额	decimal(20,4)

cash_at_beginning	现金的期初余额	decimal(20,4)

equivalents_at_end	现金等价物的期末余额	decimal(20,4)

equivalents_at_beginning	现金等价物的期初余额	decimal(20,4)

other_influence2	其他原因对现金的影响2	decimal(20,4)

cash_equivalent_increase2	现金及现金等价物净增加额2	decimal(20,4)

investment_property_depreciation	投资性房地产的折旧及摊销	decimal(20,4)

net_dec_finance_out	融出资金净减少额	decimal(20,4)

net_cash_received_from_proxy_secu	代理买卖证券收到的现金净额	decimal(20,4)

net_inc_finance_out	融出资金净增加额	decimal(20,4)

net_cash_paid_to_proxy_secu	代理买卖证券支付的现金净额	decimal(20,4)

net_dec_in_placements	拆入资金净减少额	decimal(20,4)

credit_impairment_loss	信用减值损失(现金流量表补充科目)	decimal(20,4)

报表来源编码

编码	名称

321001	招募说明书

321002	上市公告书

321003	定期报告

321004	预披露公告

321005	换股报告书

321099	其他

示例

获取金融类上市公司合并现金流量表数据

#查询中国平安2015年之后公告的合并现金流量表数据，指定只取出本期数据经营活动现金流量净额，投资活动现金流量净额，以及筹资活动现金流量净额
from jqdatasdk import *
q=query(finance.FINANCE_CASHFLOW_STATEMENT.company_name,
finance.FINANCE_CASHFLOW_STATEMENT.code,
finance.FINANCE_CASHFLOW_STATEMENT.pub_date,
finance.FINANCE_CASHFLOW_STATEMENT.start_date,
finance.FINANCE_CASHFLOW_STATEMENT.end_date,
finance.FINANCE_CASHFLOW_STATEMENT.net_operate_cash_flow,
finance.FINANCE_CASHFLOW_STATEMENT.net_invest_cash_flow,
finance.FINANCE_CASHFLOW_STATEMENT.net_finance_cash_flow).filter(finance.FINANCE_CASHFLOW_STATEMENT.code=='601318.XSHG',finance.FINANCE_CASHFLOW_STATEMENT.pub_date>='2015-01-01',finance.FINANCE_CASHFLOW_STATEMENT.report_type==0).limit(8)
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

net_operate_cash_flow  net_invest_cash_flow  net_finance_cash_flow
0           1.702600e+11         -2.368890e+11           8.536800e+10
1           6.114900e+10         -4.478700e+10           1.996200e+10
2           2.478960e+11         -1.355150e+11           1.059350e+11
3           1.710670e+11         -1.442380e+11           1.675280e+11
4           1.356180e+11         -2.737320e+11           2.049760e+11
5           1.192720e+11         -1.241580e+11           5.367100e+10
6           6.599800e+10         -2.663960e+11           1.714720e+11
7          -1.702500e+10         -1.942610e+11           1.291400e+11

获取金融类上市公司母公司合并现金流量表的信息

#查询中国平安2015年之后公告的母公司现金流量表数据，指定只取出本期经营活动现金流量净额，投资活动现金流量净额，以及筹资活动现金流量净额
from jqdatasdk import *
q=query(finance.FINANCE_CASHFLOW_STATEMENT_PARENT.company_name,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.code,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.pub_date,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.start_date,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.end_date,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.net_operate_cash_flow,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.net_invest_cash_flow,
finance.FINANCE_CASHFLOW_STATEMENT_PARENT.net_finance_cash_flow).filter(finance.FINANCE_CASHFLOW_STATEMENT_PARENT.code=='601318.XSHG',finance.FINANCE_CASHFLOW_STATEMENT_PARENT.pub_date>='2015-01-01',finance.FINANCE_CASHFLOW_STATEMENT_PARENT.report_type==0).limit(8)
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

net_operate_cash_flow  net_invest_cash_flow  net_finance_cash_flow
0            -88000000.0         -1.433300e+10           2.354300e+10
1           -202000000.0         -7.180000e+08           6.910000e+08
2            -88000000.0         -8.063000e+09           3.010000e+09
3            -25000000.0         -1.116800e+10          -7.130000e+09
4           -203000000.0         -1.099000e+10          -5.711000e+09
5           -533000000.0          4.560000e+08          -3.000000e+08
6           -236000000.0          3.237000e+09          -1.620000e+09
7           -418000000.0          1.089500e+10          -1.039000e+10
