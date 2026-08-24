金融类上市公司合并利润表、母公司利润表

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

from jqdatasdk import finance
finance.run_query(query(finance.FINANCE_INCOME_STATEMENT).filter(finance.FINANCE_INCOME_STATEMENT.code==code).limit(n))

描述

获取金融类上市公司的合并利润表信息

获取金融类上市公司的母公司利润表信息

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

金融类公司合并利润表参数

query(finance.FINANCE_INCOME_STATEMENT)：表示从finance.FINANCE_INCOME_STATEMENT这张表中查询金融类上市公司合并利润表的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_INCOME_STATEMENT：代表金融类上市公司合并利润表，收录了金融类上市公司的合并利润表，表结构和字段信息如下：

filter(finance.FINANCE_INCOME_STATEMENT.code==code)：指定筛选条件，通过finance.FINANCE_INCOME_STATEMENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_INCOME_STATEMENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的合并利润表信息；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

母公司资产负债表参数

query(finance.FINANCE_INCOME_STATEMENT_PARENT)：表示从finance.FINANCE_INCOME_STATEMENT_PARENT这张表中查询金融类上市公司母公司利润表的字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FINANCE_INCOME_STATEMENT_PARENT：代表金融类上市公司母公司利润表，收录了金融类上市公司的母公司利润表，表结构和字段信息如下：

filter(finance.FINANCE_INCOME_STATEMENT_PARENT.code==code)：指定筛选条件，通过finance.FINANCE_INCOME_STATEMENT_PARENT.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FINANCE_INCOME_STATEMENT_PARENT.pub_date>='2015-01-01'，表示公告日期大于2015年1月1日金融类上市公司公布的母公司利润表信息；多个筛选条件用英文逗号分隔。

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

operating_revenue	营业收入	decimal(20,4)

interest_net_revenue	利息净收入	decimal(20,4)

interest_income	利息收入	decimal(20,4)

interest_expense	利息支出	decimal(20,4)

commission_net_income	手续费及佣金净收入	decimal(20,4)

commission_income	手续费及佣金收入	decimal(20,4)

commission_expense	手续费及佣金支出	decimal(20,4)

agent_security_income	代理买卖证券业务净收入	decimal(20,4)

sell_security_income	证券承销业务净收入	decimal(20,4)

manage_income	委托客户管理资产业务净收入	decimal(20,4)

premiums_earned	已赚保费	decimal(20,4)

assurance_income	保险业务收入	decimal(20,4)

premiums_income	分保费收入	decimal(20,4)

premiums_expense	分出保费	decimal(20,4)

prepare_money	提取未到期责任准备金	decimal(20,4)

investment_income	投资收益	decimal(20,4)

invest_income_associates	对联营企业和合营企业的投资收益	decimal(20,4)

fair_value_variable_income	公允价值变动收益	decimal(20,4)

exchange_income	汇兑收益	decimal(20,4)

other_income	其他业务收入	decimal(20,4)

operation_expense	营业支出	decimal(20,4)

refunded_premiums	退保金	decimal(20,4)

compensate_loss	赔付支出	decimal(20,4)

compensation_back	摊回赔付支出	decimal(20,4)

insurance_reserve	提取保险责任准备金	decimal(20,4)

insurance_reserve_back	摊回保险责任准备金	decimal(20,4)

policy_dividend_payout	保单红利支出	decimal(20,4)

reinsurance_cost	分保费用	decimal(20,4)

operating_tax_surcharges	营业税金及附加	decimal(20,4)

commission_expense2	手续费及佣金支出(保险专用)	decimal(20,4)

operation_manage_fee	业务及管理费	decimal(20,4)

separate_fee	摊回分保费用	decimal(20,4)

asset_impairment_loss	资产减值损失	decimal(20,4)

other_cost	其他业务成本	decimal(20,4)

operating_profit	营业利润	decimal(20,4)

subsidy_income	补贴收入	decimal(20,4)

non_operating_revenue	营业外收入	decimal(20,4)

non_operating_expense	营业外支出	decimal(20,4)

other_items_influenced_profit	影响利润总额的其他科目	decimal(20,4)

total_profit	利润总额	decimal(20,4)

income_tax_expense	所得税费用	decimal(20,4)

other_influence_net_profit	影响净利润的其他科目	decimal(20,4)

net_profit	净利润	decimal(20,4)

np_parent_company_owners	归属于母公司股东的净利润	decimal(20,4)

minority_profit	少数股东损益	decimal(20,4)

eps	每股收益	decimal(20,4)

basic_eps	基本每股收益	decimal(20,4)

diluted_eps	稀释每股收益	decimal(20,4)

other_composite_income	其他综合收益	decimal(20,4)

total_composite_income	综合收益总额	decimal(20,4)

ci_parent_company_owners	归属于母公司的综合收益	decimal(20,4)

ci_minority_owners	归属于少数股东的综合收益	decimal(20,4)

other_earnings	其他收益	decimal(20,4)

asset_deal_income	资产处置收益	decimal(20,4)

sust_operate_net_profit	持续经营净利润	decimal(20,4)

discon_operate_net_profit	终止经营净利润	decimal(20,4)

credit_impairment_loss	信用减值损失	decimal(20,4)

报表来源编码

编码	名称

321001	招募说明书

321002	上市公告书

321003	定期报告

321004	预披露公告

321005	换股报告书

321099	其他

示例

获取金融类上市公司利润表数据

#查询中国平安2015年之后公告的合并利润表数据,指定只取出本期数据
from jqdatasdk import finance
q=query(finance.FINANCE_INCOME_STATEMENT).filter(finance.FINANCE_INCOME_STATEMENT.code=='601318.XSHG',finance.FINANCE_INCOME_STATEMENT.pub_date>='2015-01-01',finance.FINANCE_INCOME_STATEMENT.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

id  company_id      company_name         code  a_code b_code h_code  \
0  246   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
1  248   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
2  250   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
3  252   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
4  254   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
5  256   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
6  258   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
7  260   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None

pub_date  start_date    end_date         ...            net_profit  \
0  2015-03-20  2014-01-01  2014-12-31         ...          4.793000e+10
1  2015-04-30  2015-01-01  2015-03-31         ...          2.243600e+10
2  2015-08-21  2015-01-01  2015-06-30         ...          3.991100e+10
3  2015-10-28  2015-01-01  2015-09-30         ...          5.640500e+10
4  2016-03-16  2015-01-01  2015-12-31         ...          6.517800e+10
5  2016-04-27  2016-01-01  2016-03-31         ...          2.338900e+10
6  2016-08-18  2016-01-01  2016-06-30         ...          4.630800e+10
7  2016-10-28  2016-01-01  2016-09-30         ...          6.481300e+10

np_parent_company_owners  minority_profit   eps  basic_eps  diluted_eps  \
0              3.927900e+10     8.651000e+09  None       4.93         4.68
1              1.996400e+10     2.472000e+09  None       2.19         2.19
2              3.464900e+10     5.262000e+09  None       1.90         1.90
3              4.827600e+10     8.129000e+09  None       2.64         2.64
4              5.420300e+10     1.097500e+10  None       2.98         2.98
5              2.070000e+10     2.689000e+09  None       1.16         1.16
6              4.077600e+10     5.532000e+09  None       2.28         2.28
7              5.650800e+10     8.305000e+09  None       3.17         3.16

other_composite_income  total_composite_income  ci_parent_company_owners  \
0            3.077400e+10            7.870400e+10              6.959000e+10
1           -3.572000e+09            1.886400e+10              1.633600e+10
2            7.100000e+07            3.998200e+10              3.450800e+10
3           -1.316100e+10            4.324400e+10              3.488100e+10
4            7.520000e+08            6.593000e+10              5.456500e+10
5           -1.124600e+10            1.214300e+10              9.509000e+09
6           -9.129000e+09            3.717900e+10              3.167900e+10
7           -5.917000e+09            5.889600e+10              5.050300e+10

ci_minority_owners
0        9.114000e+09
1        2.528000e+09
2        5.474000e+09
3        8.363000e+09
4        1.136500e+10
5        2.634000e+09
6        5.500000e+09
7        8.393000e+09

[8 rows x 66 columns]

获取金融类上市公司母公司利润表的信息

#查询中国平安2015年之后公告的母公司利润表数据,指定只取出本期数据
from jqdatasdk import finance
q=query(finance.FINANCE_INCOME_STATEMENT_PARENT).filter(finance.FINANCE_INCOME_STATEMENT_PARENT.code=='601318.XSHG',finance.FINANCE_INCOME_STATEMENT_PARENT.pub_date>='2015-01-01',finance.FINANCE_INCOME_STATEMENT_PARENT.report_type==0).limit(8)
df=finance.run_query(q)
print(df)

id  company_id      company_name         code  a_code b_code h_code  \
0  214   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
1  216   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
2  218   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
3  220   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
4  222   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
5  224   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
6  226   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None
7  228   300002221  中国平安保险(集团)股份有限公司  601318.XSHG  601318   None   None

pub_date  start_date    end_date        ...            net_profit  \
0  2015-03-20  2014-01-01  2014-12-31        ...          7.214000e+09
1  2015-04-30  2015-01-01  2015-03-31        ...          3.160000e+08
2  2015-08-21  2015-01-01  2015-06-30        ...          8.390000e+09
3  2015-10-28  2015-01-01  2015-09-30        ...          8.969000e+09
4  2016-03-16  2015-01-01  2015-12-31        ...          1.028000e+10
5  2016-04-27  2016-01-01  2016-03-31        ...          1.810000e+08
6  2016-08-18  2016-01-01  2016-06-30        ...          1.385000e+10
7  2016-10-28  2016-01-01  2016-09-30        ...          1.374700e+10

np_parent_company_owners  minority_profit   eps  basic_eps diluted_eps  \
0              7.214000e+09             None  None       None        None
1              3.160000e+08             None  None       None        None
2              8.390000e+09             None  None       None        None
3              8.969000e+09             None  None       None        None
4              1.028000e+10             None  None       None        None
5              1.810000e+08             None  None       None        None
6              1.385000e+10             None  None       None        None
7              1.374700e+10             None  None       None        None

other_composite_income total_composite_income ci_parent_company_owners  \
0            235000000.0           7.449000e+09                      NaN
1            -47000000.0           2.690000e+08                      NaN
2             85000000.0           8.475000e+09                      NaN
3            191000000.0           9.160000e+09                      NaN
4            436000000.0           1.071600e+10                      NaN
5            -38000000.0           1.430000e+08                      NaN
6            -48000000.0           1.380200e+10             1.380200e+10
7              7000000.0           1.375400e+10             1.375400e+10

ci_minority_owners
0               None
1               None
2               None
3               None
4               None
5               None
6               None
7               None

[8 rows x 66 columns]
