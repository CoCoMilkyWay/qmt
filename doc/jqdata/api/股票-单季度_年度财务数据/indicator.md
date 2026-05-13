财务指标数据

历史范围：2005年至今；更新时间：交易日24:00更新

get_fundamentals(query_object, date=None, statDate=None)

描述

查询indicator财务数据

按季度更新, 统计周期是一季度。可以使用get_fundamentals(query_object, date=None, statDate=None)查询

获取多日财务数据get_fundamentals_continuously

获取多个季度/年度的历史财务数据get_history_fundamentals

注意

date和statDate参数只能传入一个

传入date时, 查询指定日期date收盘后所能看到的最近(对市值表来说, 最近一天, 对其他表来说, 最近一个季度)的数据

statDate: 财报统计的季度或者年份。

季度: 格式是年 + 'q' + 季度序号, 例如: '2015q1', '2013q4'.
年份: 格式就是年份的数字, 例如:'2015', '2016'.

为了防止返回数据量过大, 我们每次最多返回10000行

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

indicator财务指标表

列名	列的含义	解释

code	股票代码	带后缀.XSHE/.XSHG

pubDate	日期	公司发布财报日期

statDate	日期	财报统计的季度的最后一天, 比如2015-03-31, 2015-06-30

eps	每股收益EPS(元)	每股收益(摊薄)＝净利润/期末股本；分子从单季利润表取值，分母取季度末报告期股本值。

adjusted_profit	扣除非经常损益后的净利润(元)	非经常性损益这一概念是证监会在1999年首次提出的，当时将其定义为：公司正常经营损益之外的一次性或偶发性损益。《问答第1号》则指出：非经常性损益是公司发生的与经营业务无直接关系的收支；以及虽与经营业务相关，但由于其性质、金额或发生频率等方面的原因，影响了真实公允地反映公司正常盈利能力的各项收入。

operating_profit	经营活动净收益(元)	营业总收入-营业总成本

value_change_profit	价值变动净收益(元)	公允价值变动净收益+投资净收益+汇兑净收益

roe	净资产收益率ROE(%)	归属于母公司股东的净利润*2/（期初归属于母公司股东的净资产+期末归属于母公司股东的净资产）

inc_return	净资产收益率(扣除非经常损益)(%)	扣除非经常损益后的净利润（不含少数股东损益）*2/（期初归属于母公司股东的净资产+期末归属于母公司股东的净资产）

roa	总资产净利率ROA(%)	净利润*2/（期初总资产+期末总资产）

net_profit_margin	销售净利率(%)	净利润/营业收入

gross_profit_margin	销售毛利率(%)	毛利/营业收入

expense_to_total_revenue	营业总成本/营业总收入(%)	营业总成本/营业总收入(%)

operation_profit_to_total_revenue	营业利润/营业总收入(%)	营业利润/营业总收入(%)

net_profit_to_total_revenue	净利润/营业总收入(%)	净利润/营业总收入(%)

operating_expense_to_total_revenue	营业费用/营业总收入(%)	营业费用/营业总收入(%)

ga_expense_to_total_revenue	管理费用/营业总收入(%)	管理费用/营业总收入(%)

financing_expense_to_total_revenue	财务费用/营业总收入(%)	财务费用/营业总收入(%)

operating_profit_to_profit	经营活动净收益/利润总额(%)	经营活动净收益/利润总额(%)

invesment_profit_to_profit	价值变动净收益/利润总额(%)	价值变动净收益/利润总额(%)

adjusted_profit_to_profit	扣除非经常损益后的净利润/归属于母公司所有者的净利润(%)	扣除非经常损益后的净利润/归属于母公司所有者的净利润(%)

goods_sale_and_service_to_revenue	销售商品提供劳务收到的现金/营业收入(%)	销售商品提供劳务收到的现金/营业收入(%)

ocf_to_revenue	经营活动产生的现金流量净额/营业收入(%)	经营活动产生的现金流量净额/营业收入(%)

ocf_to_operating_profit	经营活动产生的现金流量净额/经营活动净收益(%)	经营活动产生的现金流量净额/经营活动净收益(%)

inc_total_revenue_year_on_year	营业总收入同比增长率(%)	营业总收入同比增长率是企业在一定期间内取得的营业总收入与其上年同期营业总收入的增长的百分比，以反映企业在此期间内营业总收入的增长或下降等情况。

inc_total_revenue_annual	营业总收入环比增长率(%)	营业收入是指企业在从事销售商品，提供劳务和让渡资产使用权等日常经营业务过程中所形成的经济利益的总流入。环比增长率=（本期的某个指标的值-上一期这个指标的值）/上一期这个指标的值*100%。

inc_revenue_year_on_year	营业收入同比增长率(%)	营业收入,是指公司在从事销售商品、提供劳务和让渡资产使用权等日常经营业务过程中所形成的经济利益的总流入，而营业收入同比增长率，则是检验上市公司去年一年挣钱能力是否提高的标准，营业收入同比增长,说明公司在上一年度挣钱的能力加强了，营业收入同比下降，则说明公司的挣钱能力稍逊于往年。

inc_revenue_annual	营业收入环比增长率(%)	环比增长率=（本期的某个指标的值-上一期这个指标的值）/上一期这个指标的值*100%。

inc_operation_profit_year_on_year	营业利润同比增长率(%)	同比增长率就是指公司当年期的净利润和上月同期、上年同期的净利润比较。（当期的利润-上月（上年）当期的利润）/上月（上年）当期的利润=利润同比增长率。

inc_operation_profit_annual	营业利润环比增长率(%)	环比增长率=（本期的某个指标的值-上一期这个指标的值）/上一期这个指标的值*100%。

inc_net_profit_year_on_year	净利润同比增长率(%)	（当期的净利润-上月（上年）当期的净利润）/上月（上年）当期的净利润绝对值=净利润同比增长率。

inc_net_profit_annual	净利润环比增长率(%)	环比增长率=（本期的某个指标的值-上一期这个指标的值）/上一期这个指标的值*100%。

inc_net_profit_to_shareholders_year_on_year	归属母公司股东的净利润同比增长率(%)	归属于母公司股东净利润是指全部归属于母公司股东的净利润，包括母公司实现的净利润和下属子公司实现的净利润；同比增长率，一般是指和去年同期相比较的增长率。同比增长 和上一时期、上一年度或历史相比的增长（幅度）。

inc_net_profit_to_shareholders_annual	归属母公司股东的净利润环比增长率(%)	环比增长率=（本期的某个指标的值-上一期这个指标的值）/上一期这个指标的值*100%。

示例

获取某一天所有的财务指标数据**

# 获取“2021-01-05”所有的财务指标数据
df=get_fundamentals(query(indicator),date="2021-01-05")
print(df[:4])

id         code         day     pubDate    statDate     eps  \
0  45000708  000001.XSHE  2021-01-05  2020-10-22  2020-09-30  0.4493
1  45590101  000002.XSHE  2021-01-05  2020-10-30  2020-09-30  0.6331
2  45590317  000004.XSHE  2021-01-05  2020-10-30  2020-09-30  0.3324
3  45406174  000005.XSHE  2021-01-05  2020-10-29  2020-09-30  0.0043

adjusted_profit  operating_profit  value_change_profit   roe  \
0     8.730000e+09      1.041200e+10         8.570000e+08  2.46
1     6.775110e+09      1.379713e+10         1.785868e+09  3.69
2     5.469821e+07      6.525505e+07                  NaN  3.86
3    -1.787463e+07     -1.747028e+07         4.047380e+07  0.28

...                    inc_total_revenue_year_on_year  \
0                  ...                                            8.8400
1                  ...                                           12.4700
2                  ...                                        17929.4004
3                  ...                                          -41.6200

inc_total_revenue_annual  inc_revenue_year_on_year  inc_revenue_annual  \
0                     -5.36                    8.8400               -5.36
1                     -3.48                   12.4700               -3.48
2                    209.31                17929.4004              209.31
3                     -3.07                  -41.6200               -3.07

inc_operation_profit_year_on_year  inc_operation_profit_annual  \
0                               5.51                      69.1000
1                              10.53                     -28.2900
2                             745.37                    6337.2998
3                             138.08                     155.5600

inc_net_profit_year_on_year  inc_net_profit_annual  \
0                         6.11                69.9800
1                        22.97               -29.3600
2                       721.89              3654.3401
3                       143.59               151.5400

inc_net_profit_to_shareholders_year_on_year  \
0                                         6.11
1                                        14.94
2                                       783.92
3                                       132.08

inc_net_profit_to_shareholders_annual
0                                69.9800
1                               -34.6700
2                              4519.6099
3                               140.9000

[4 rows x 36 columns]

获取一只股票某个字段数据**

q = query(
indicator
).filter(
indicator.code == '000004.XSHE'
)
df = get_fundamentals(q, '2015-10-15')
# 打印出每股收益EPS(元)
print(df['eps'][0])

>>>0.0365

获取多只股票在某一日期的财务数据

df = get_fundamentals(query(
indicator
).filter(
# 这里不能使用 in 操作, 要使用in_()函数
indicator.code.in_(['000001.XSHE', '600000.XSHG'])
), date='2015-10-15')
print(df)

id         code         day     pubDate    statDate     eps  \
0      178  000001.XSHE  2015-10-15  2015-08-14  2015-06-30  0.4162
1  2952806  600000.XSHG  2015-10-15  2015-08-20  2015-06-30  0.6813

adjusted_profit  operating_profit  value_change_profit   roe  \
0     5.954000e+09      6.254000e+09         1.566000e+09  4.14
1     1.239500e+10      1.553200e+10         8.140000e+08  4.45

...                    inc_total_revenue_year_on_year  \
0                  ...                                             39.02
1                  ...                                             19.94

inc_total_revenue_annual  inc_revenue_year_on_year  inc_revenue_annual  \
0                     25.32                     39.02               25.32
1                     14.30                     19.94               14.30

inc_operation_profit_year_on_year  inc_operation_profit_annual  \
0                              17.90                         5.01
1                               4.92                        12.52

inc_net_profit_year_on_year  inc_net_profit_annual  \
0                        18.69                   5.81
1                         6.29                  13.33

inc_net_profit_to_shareholders_year_on_year  \
0                                        18.69
1                                         6.39

inc_net_profit_to_shareholders_annual
0                                   5.81
1                                  13.53

[2 rows x 36 columns]
