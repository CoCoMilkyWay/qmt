可转债日行情

历史范围：2018/9/13至今；更新时间：每日19：00、22:00更新

from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_DAILY_PRICE).limit(n))

描述

获取可转债日行情

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

query函数的使用技巧

query函数的更多用法详见：query简易教程

可转债日行情

query(bond.CONBOND_DAILY_PRICE)：表示从bond.CONBOND_DAILY_PRICE这张表中查询可转债日行情，其中bond是库名，CONBOND_DAILY_PRICE是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.CONBOND_DAILY_PRICE：获取可转债日行情，表结构和字段信息如下：

filter(bond.CONBOND_DAILY_PRICE.code==code)：指定筛选条件，通过bond.CONBOND_DAILY_PRICE.code == '131801' 可以指定债券代码来获取可转债日行情；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述

date	date	交易日期（以YYYY-MM-DD表示）

code	str	债券代码

name	str	债券简称

exchange_code	str	证券市场编码（XSHG-上交所；XSHE-深交所）

pre_close	float	昨收价

open	float	开盘价，以人民币计

high	float	最高价，以人民币计

low	float	最低价，以人民币计

close	float	收盘价，以人民币计

volume	float	成交量（手），1手为10张债券

money	float	成交额，以人民币计

deal_number	int	成交笔数

change_pct	float	涨跌幅，单位：%

示例

from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_DAILY_PRICE))
print(df[:4])

id        date    code  name exchange_code  pre_close    open    high  \
0   1  2018-09-13  110030  格力转债          XSHG     101.94  101.92  101.98
1   2  2018-09-14  110030  格力转债          XSHG     101.90  101.89  101.91
2   3  2018-09-17  110030  格力转债          XSHG     101.90  101.91  101.93
3   4  2018-09-18  110030  格力转债          XSHG     101.92  101.91  101.94

low   close  volume      money  deal_number  change_pct
0  101.90  101.90    5146  5244323.0         90.0     -0.0392
1  101.88  101.90    4348  4430498.0         58.0      0.0000
2  101.89  101.92    2087  2126837.0         71.0      0.0196
3  101.89  101.91    1736  1769120.0         70.0     -0.0098

# 获取格力转债的日行情
from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_DAILY_PRICE).filter(bond.CONBOND_DAILY_PRICE.code=="110030"))
print(df[:5])

id        date    code  name exchange_code  pre_close    open    high  \
0  66071  2015-01-13  110030  N格力转          XSHG     100.00  138.00  142.80
1  66072  2015-01-14  110030  格力转债          XSHG     138.26  138.58  142.03
2  66073  2015-01-15  110030  格力转债          XSHG     139.81  139.20  141.20
3  66074  2015-01-16  110030  格力转债          XSHG     139.84  139.99  145.60
4  66075  2015-01-19  110030  格力转债          XSHG     143.35  141.04  141.05

low   close  volume        money  deal_number  change_pct
0  137.00  138.26  119222  165203875.0       1199.0     38.2600
1  137.11  139.81  274244  384512317.0       5109.0      1.1211
2  138.81  139.84   77315  108056549.0       2334.0      0.0215
3  139.99  143.35   88171  125987816.0       2276.0      2.5100
4  139.00  139.46   73077  102210898.0       1044.0     -2.7136
