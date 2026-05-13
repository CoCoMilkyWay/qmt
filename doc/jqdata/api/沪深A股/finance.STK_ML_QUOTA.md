市场通成交与额度信息

历史范围：上市至今；更新时间：交易日20:30-06:30更新

from jqdatasdk import finance
finance.run_query(query(finance.STK_ML_QUOTA).filter(finance.STK_ML_QUOTA.day==day).limit(n))

描述

统计沪港通、深港通和港股通前十大交易活跃股的交易状况。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

query函数的使用技巧

query函数的更多用法详见：query简易教程

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

参数

query(finance.STK_ML_QUOTA)：表示从finance.STK_ML_QUOTA这张表中查询沪港通、深港通和港股通的成交与额度信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_ML_QUOTA：代表了市场通成交与额度信息表，记录了沪港通、深港通和港股通成交与额度的信息，包括买入、卖出等，表结构和字段信息如下:

filter(finance.STK_ML_QUOTA.day==day)：指定筛选条件，通过finance.STK_ML_QUOTA.day==day可以指定你想要查询的日期；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_ML_QUOTA.link_id==310001，表示筛选市场通编码为310001（沪股通）的数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段	名称	类型	备注/示例

day	交易日期	date

link_id	市场通编码	int

link_name	市场通名称	varchar(32)	包括以下四个名称： 沪股通，深股通，港股通(沪）,港股通(深）；
其中沪股通和深股通属于北向资金，
港股通（沪）和港股通（深）属于南向资金。

currency_id	货币编码	int

currency	货币名称	varchar(16)

buy_amount	买入成交额	decimal(20,4)	亿(自2024-08-18之后北向不再披露)

buy_volume	买入成交数	decimal(20,4)	笔(自2024-08-18之后北向不再披露)

sell_amount	卖出成交额	decimal(20,4)	亿(自2024-08-18之后北向不再披露)

sell_volume	卖出成交数	decimal(20,4)	笔(自2024-08-18之后北向不再披露)

sum_amount	累计成交额	decimal(20,4)	买入成交额+卖出成交额

sum_volume	累计成交数目	decimal(20,4)	买入成交量+卖出成交量

quota	总额度	decimal(20, 4)	亿（2016-08-16号起，沪港通和深港通不再设总额度限制）

quota_balance	总额度余额	decimal(20, 4)	亿（2016-08-16号起，沪港通和深港通不再设总额度限制）

quota_daily	每日额度	decimal(20, 4)	亿 (自2024-08-18之后北向不再披露)

quota_daily_balance	每日额度余额	decimal(20, 4)	亿 (自2024-08-18之后北向不再披露)

市场通编码

市场通编码	市场通名称

310001	沪股通

310002	深股通

310003	港股通（沪）

310004	港股通（深）

货币编码

货币编码	货币名称

110001	人民币

110003	港元

示例

q=query(finance.STK_ML_QUOTA).filter(finance.STK_ML_QUOTA.day>='2015-01-01').limit(4)
df=finance.run_query(q)
print(df)

id         day  link_id link_name  currency_id currency_name  buy_amount  \
0   1  2015-04-17   310003    港股通(沪)       110003            港元     81.2880
1   2  2015-04-17   310001       沪股通       110001           人民币     58.9429
2   3  2015-04-16   310003    港股通(沪)       110003            港元     56.1928
3   4  2015-04-16   310001       沪股通       110001           人民币     45.3180

buy_volume  sell_amount  sell_volume  sum_amount  sum_volume quota  \
0     92742.0      62.8078      66353.0    144.0958    159095.0  None
1    119118.0      42.9775      81620.0    101.9204    200738.0  None
2     64867.0      53.9511      52591.0    110.1439    117458.0  None
3     98189.0      29.5743      86428.0     74.8923    184617.0  None

quota_balance  quota_daily  quota_daily_balance
0          None        105.0              83.7100
1          None        130.0             111.9694
2          None        105.0              97.9000
3          None        130.0             110.5500
