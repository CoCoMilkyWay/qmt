获取多个季度/年度的历史财务数据

get_history_fundamentals(security, fields, watch_date=None, stat_date=None, count=1, interval='1q', stat_by_year=False)

描述

获取多个季度/年度的三大财务报表和财务指标数据. 可指定单季度数据, 也可以指定年度数据。可以指定观察日期, 也可以指定最后一个报告期的结束日期

参数

security：股票代码或者股票代码列表。

fields：要查询的财务数据的列表, 季度数据和年度数据可选择的列不同。

示例：
[balance.cash_equivalents, cash_flow.net_deposit_increase, income.total_operating_revenue]

watch_date：观察日期, 如果指定, 将返回 watch_date 日期前(包含该日期)发布的报表数据

stat_date：统计日期, 可以是 '2019'/'2019q1'/'2018q4' 格式, 如果指定, 将返回 stat_date 对应报告期及之前的历史报告期的报表数据

watch_date 和 stat_date 只能指定一个, 而且必须指定一个

如果没有 stat_date 指定报告期的数据, 则该数据会缺失一行.

count：查询历史的多个报告期时, 指定的报告期数量. 如果股票历史报告期的数量小于 count, 则该股票返回的数据行数将小于 count

interval：查询多个报告期数据时, 指定报告期间隔, 可选值: '1q'/'1y', 表示间隔一季度或者一年, 举例说明:

stat_date='2019q1', interval='1q', count=4, 将返回 2018q2,2018q3,2018q4,2019q1 的数据

stat_date='2019q1', interval='1y', count=4, 将返回 2016q1,2017q1,2018q1,2019q1 的数据

stat_by_year=True, stat_date='2018', interval='1y', count=4 将返回 2015/2016/2017/2018 年度的年报数据

stat_by_year：bool, 是否返回年度数据. 默认返回的按季度统计的数据(比如income表中只有单个季度的利润).

如果是True：

interval必须是 '1y'

如果指定了 stat_date 的话, stat_date 必须是一个代表年份整数、字符串, 表明统计的年份，比如2019, "2019"。但不能是"20191q"这种格式。

fields 可以选择 balance/income/cash_flow/indicator表中的列

如果是False：

fields只能选择balance/income/cash_flow/indicator 表中的列

注意

pandas.DataFrame, 数据库查询结果. 数据格式同 get_fundamentals. 每个股票每个报告期(一季度或者一年)的数据占用一行.

不支持valuation市值表

推荐用户对结果使用pandas的groupby方法来进行分组分析数据

为了防止返回数据量过大, 我们每次最多返回10000行

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

示例

from jqdatasdk import *
security = ['000001.XSHE', '600000.XSHG']
df = get_history_fundamentals(security, fields=[balance.cash_equivalents,
cash_flow.net_deposit_increase, income.total_operating_revenue],
watch_date=None, stat_date='2019q1', count=5, interval='1q', stat_by_year=False)
print(df)
print(df.groupby('code').mean())
