市场通成交汇率

历史范围：上市至今；更新时间：交易日20:30-06:30更新

from jqdatasdk import finance finance.run_query(query(finance.STK_EXCHANGE_LINK_RATE).filter(finance.STK_EXCHANGE_LINK_RATE.day==day).limit(n))

描述

包含2014年11月起人民币和港币之间的参考汇率/结算汇兑比率信息。

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

query函数的使用技巧

query函数的更多用法详见：query简易教程

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

参数

query(finance.STK_EXCHANGE_LINK_RATE)：表示从finance.STK_EXCHANGE_LINK_RATE这张表中查询汇率信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_EXCHANGE_LINK_RATE：代表市场通汇率表，记录参考汇率/结算汇兑比率信息，包括买入参考/结算汇率、卖出参考/结算汇率等，表结构和字段信息如下:

filter(finance.STK_EXCHANGE_LINK_RATE.day==day)：指定筛选条件，通过finance.STK_EXCHANGE_LINK_RATE.day==day可以指定你想要查询的日期；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_EXCHANGE_LINK_RATE.link_id==310001，表示筛选市场通编码为310001（沪股通）的数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段	名称	类型	非空	交易所	备注/示例

day	日期	Date	Y	Y

link_id	市场通编码	int	Y

link_name	市场通名称	varchar(32)	Y	以“港股通(沪)”为代表

domestic_currency	本币	varchar(12)	Y	RMB

foreign_currency	外币	varchar(12)	Y	HKD

refer_bid_rate	买入参考汇率	decimal(10, 5)	Y	Y

refer_ask_rate	卖出参考汇率	decimal(10, 5)	Y	Y

settle_bid_rate	买入结算汇率	decimal(10, 5)	Y	Y

settle_ask_rate	卖出结算汇率	decimal(10, 5)	Y	Y

市场通编码

市场通编码	市场通名称

310001	沪股通

310002	深股通

310003	港股通（沪）

310004	港股通（深）

示例

q=query(finance.STK_EXCHANGE_LINK_RATE).filter(finance.STK_EXCHANGE_LINK_RATE.day>='2015-01-01').limit(4)
df=finance.run_query(q)
print(df)

id         day  link_id link_name domestic_currency foreign_currency  \
0  31  2015-01-05   310003    港股通(沪)               RMB              HKD
1  32  2015-01-06   310003    港股通(沪)               RMB              HKD
2  33  2015-01-07   310003    港股通(沪)               RMB              HKD
3  34  2015-01-08   310003    港股通(沪)               RMB              HKD

refer_bid_rate  refer_ask_rate  settle_bid_rate  settle_ask_rate
0          0.7774          0.8254          0.80317          0.80283
1          0.7785          0.8267          0.80307          0.80213
2          0.7777          0.8259          0.80197          0.80163
3          0.7773          0.8253          0.80116          0.80144
