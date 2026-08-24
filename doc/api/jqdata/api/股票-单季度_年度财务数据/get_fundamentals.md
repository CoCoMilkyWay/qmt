查询财务数据(附综合案例)

get_fundamentals(query_object, date=None, statDate=None)

描述

查询财务数据，详细的数据字段描述请该目录对应文档查看

注意

date和statDate参数只能传入一个,传入date时, 查询指定日期date收盘后所能看到的最近(对市值表来说, 最近一天, 对其他表来说, 最近一个季度)的数据, 我们会查找上市公司在这个日期之前(包括此日期)发布的数据, 不会有未来函数.(查找上市公司在这个日期(date)之前(包括此日期)发布的(pubDate)的最新一期(statDate) 数据,如果同一天披露的话 , 会取最新的一期 )

当 date 和 statDate 都不传入时, 相当于使用 date 参数, date 的默认值下面会描述.

由于公司发布财报不及时, 一般是看不到当季度或年份的财务报表的, 回测中使用这个数据可能会有未来函数, 请注意规避.

由于估值表每天更新, 当按季度或者年份查询时, 返回季度或者年份最后一天的数据

由于“资产负债数据”这个表是存量性质的， 查询年度数据是返回第四季度的数据。

银行业、券商、保险专项数据只有年报数据，需传入statDate参数，当传入 date 参数 或 statDate 传入季度时返回空，请自行避免未来函数。

参数

query_object: 一个sqlalchemy.orm.query.Query对象，可以通过全局的 query 函数获取 Query 对象，[query简易教程]

date: 查询日期, 一个字符串(格式类似'2015-10-15')或者[datetime.date]/[datetime.datetime]对象, 可以是None, 使用默认日期. 如果传入的 date 不是交易日, 则使用这个日期之前的最近的一个交易日

statDate: 财报统计的季度或者年份, 一个字符串, 有两种格式

1. 季度: 格式是: 年 + 'q' + 季度序号, 例如: '2015q1', '2013q4'.

2. 年份: 格式就是年份的数字, 例如: '2015', '2016'.

注意

返回一个 [pandas.DataFrame], 每一行对应数据库返回的每一行(可能是几个表的联合查询结果的一行), 列索引是你查询的所有字段

为了防止返回数据量过大, 我们每次最多返回10000行

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

示例：

获取多只股票在某一天的指标数据

# 获取多只股票在某一日期的市值, 利润
df = get_fundamentals(query(
valuation, income
).filter(
# 这里不能使用 in 操作, 要使用in_()函数
valuation.code.in_(['000001.XSHE', '600000.XSHG'])
), date='2022-01-20')
print(df)
id         code  pe_ratio  turnover_ratio  pb_ratio  ps_ratio  \
0  76774258  000001.XSHE    9.2554          1.5620    1.0332    1.9855
1  76775726  600000.XSHG    4.6862          0.2822    0.4698    1.3514

pcf_ratio  capitalization  market_cap  circulating_cap  \
0   -18.3944     1940591.875   3363.0457       1940575.50
1     4.5834     2935216.750   2582.9907       2935216.75

circulating_market_cap         day  pe_ratio_lyr      id_1       code_1  \
0               3363.0173  2022-01-20        9.2554  35059772  000001.XSHE
1               2582.9907  2022-01-20        4.4286  35917288  600000.XSHG

day_1     pubDate    statDate  total_operating_revenue  \
0  2022-01-20  2021-10-21  2021-09-30             4.251000e+10
1  2022-01-20  2021-10-30  2021-09-30             4.611900e+10

operating_revenue  interest_income  premiums_earned  commission_income  \
0       4.251000e+10     3.020000e+10              NaN       8.041000e+09
1       4.611900e+10     7.476500e+10              NaN       9.733000e+09

total_operating_cost  operating_cost  interest_expense  commission_expense  \
0          2.809600e+10             NaN               NaN                 NaN
1          3.341300e+10             NaN      4.103800e+10        2.679000e+09

refunded_premiums  net_pay_insurance_claims  \
0                NaN                       0.0
1                NaN                       NaN

withdraw_insurance_contract_reserve  policy_dividend_payout  \
0                                  0.0                     NaN
1                                  NaN                     NaN

reinsurance_cost  operating_tax_surcharges  sale_expense  \
0               0.0               409000000.0           NaN
1               NaN               477000000.0           NaN

administration_expense  financial_expense  asset_impairment_loss  \
0            1.221000e+10                NaN           1.177000e+09
1            1.139500e+10                NaN                    NaN

fair_value_variable_income  investment_income  invest_income_associates  \
0                 287000000.0       3.404000e+09                       NaN
1                -296000000.0       4.473000e+09                48000000.0

exchange_income  operating_profit  non_operating_revenue  \
0      488000000.0      1.441400e+10             58000000.0
1      172000000.0      1.270600e+10             14000000.0

non_operating_expense  disposal_loss_non_current_liability  total_profit  \
0             26000000.0                                  NaN  1.444600e+10
1             94000000.0                                  NaN  1.262600e+10

income_tax_expense    net_profit  np_parent_company_owners  \
0        2.894000e+09  1.155200e+10              1.155200e+10
1        7.010000e+08  1.192500e+10              1.169800e+10

minority_profit  basic_eps  diluted_eps  other_composite_income  \
0              NaN       0.60         0.60             113000000.0
1      227000000.0       0.35         0.32            -333000000.0

total_composite_income  ci_parent_company_owners  ci_minority_owners
0            1.166500e+10              1.166500e+10                 NaN
1            1.159200e+10              1.136400e+10         228000000.0

根据条件，筛选数据

# 选出所有的总市值大于1000亿元, 市盈率小于10, 基本每股收益大于0.8的股票
df = get_fundamentals(query(
valuation.code, valuation.market_cap, valuation.pe_ratio, income.basic_eps ,income.total_operating_revenue
).filter(

valuation.market_cap > 1000,
valuation.pe_ratio < 10,
income.basic_eps  > 0.8
).order_by(
# 按市值降序排列
valuation.market_cap.desc()
))
print(df)

code  market_cap  pe_ratio  basic_eps  total_operating_revenue
0  601166.XSHG   4908.9414    7.3679       0.92             5.176600e+10
1  600585.XSHG   2754.5774    7.8412       1.63             4.997668e+10

使用or_函数,筛选数据

# 使用 or_ 函数: 查询总市值大于1000亿元 或者 市盈率小于10的股票 或者 基本每股收益大于0.5
from sqlalchemy.sql.expression import or_
df=get_fundamentals(query(
valuation.code,valuation.pe_ratio,valuation.market_cap,income.basic_eps ,income.total_operating_revenue
).filter(
or_(
valuation.market_cap > 1000,
valuation.pe_ratio < 10,
income.basic_eps > 0.5
)
).order_by(
# 按市值降序排列
valuation.market_cap.desc()))
print(df[:5])

code  pe_ratio  market_cap  basic_eps  total_operating_revenue
0  600519.XSHG   56.0766  24998.2109       8.94             2.394051e+10
1  601398.XSHG    6.6798  19317.2188       0.22             2.172370e+11
2  601939.XSHG    7.4127  18325.8047       0.27             1.823260e+11
3  601318.XSHG   10.1966  14591.2891       2.27             3.012450e+11
4  600036.XSHG   13.9336  13563.2334       0.77             6.905200e+10

查询季报, 放到数组中

# 查询平安银行2014年四个季度的季报, 放到数组中
q = query(
income.statDate,
income.code,
income.basic_eps,
balance.cash_equivalents,
cash_flow.goods_sale_and_service_render_cash
).filter(
income.code == '000001.XSHE',
)

rets = [get_fundamentals(q, statDate='2018q'+str(i)) for i in range(1, 5)]
print(rets)

[     statDate         code  basic_eps  cash_equivalents  \
0  2018-03-31  000001.XSHE       0.33      2.819490e+11

goods_sale_and_service_render_cash
0                                 NaN  ,      statDate         code  basic_eps  cash_equivalents  \
0  2018-06-30  000001.XSHE        0.4      3.074000e+11

goods_sale_and_service_render_cash
0                                 NaN  ,      statDate         code  basic_eps  cash_equivalents  \
0  2018-09-30  000001.XSHE       0.41      2.876480e+11

goods_sale_and_service_render_cash
0                                 NaN  ,      statDate         code  basic_eps  cash_equivalents  \
0  2018-12-31  000001.XSHE       0.25      2.785280e+11

goods_sale_and_service_render_cash
0                                 NaN  ]

查询年报

# 查询平安银行2018年的年报
q = query(
income.statDate,
income.code,
income.basic_eps,
cash_flow.goods_sale_and_service_render_cash
).filter(
income.code == '000001.XSHE',
)

ret = get_fundamentals(q, statDate='2018')
print(ret)

statDate         code  basic_eps  goods_sale_and_service_render_cash
0  2018-12-31  000001.XSHE       1.39                                 NaN
