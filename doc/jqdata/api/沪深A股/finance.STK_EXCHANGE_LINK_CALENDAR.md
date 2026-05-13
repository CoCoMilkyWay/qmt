市场通交易日历

历史范围：上市至今；更新时间：交易日20:30-06:30更新

from jqdatasdk import finance

finance.run_query(query(finance.STK_EXCHANGE_LINK_CALENDAR).filter(finance.STK_EXCHANGE_LINK_CALENDAR.day==day).limit(n))

描述

记录沪港通、深港通和港股通每天是否开市。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

参数

query(finance.STK_EXCHANGE_LINK_CALENDAR)：表示从finance.STK_EXCHANGE_LINK_CALENDAR这张表中查询市场沪港通、深港通和港股通交易日历的信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_EXCHANGE_LINK_CALENDAR：代表市场通交易日历表，记录沪港通、深港通和港股通每天是否开市，包括交易日期，交易日类型等，表结构和字段信息如下：

filter(finance.STK_EXCHANGE_LINK_CALENDAR.day==day)：指定筛选条件，通过finance.STK_EXCHANGE_LINK_CALENDAR.day==day可以指定你想要查询的日期；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_EXCHANGE_LINK_CALENDAR.type_id==312001，表示筛选交易日类型为正常交易日的数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段	名称	类型	非空	备注/示例

day	交易日期	date	Y

link_id	市场通编码	int	Y

link_name	市场通名称	varchar(32)	Y	包括以下四个名称：
沪股通， 深股通
港股通(沪)， 港股通(深)

type_id	交易日类型编码	int	Y	如下 交易日类型编码

type	交易日类型	varchar(32)	Y

附注：港股通（沪）和港股通（深）的交易日在深港通开展后是一致的。

交易日类型编码

交易日类型编码	交易日类型

312001	正常交易日

312002	周末

312003	全天休市

312004	上午休市

312005	下午休市

市场通编码

市场通编码	市场通名称

310001	沪股通

310002	深股通

310003	港股通（沪）

310004	港股通（深）

示例

q=query(finance.STK_EXCHANGE_LINK_CALENDAR).filter(finance.STK_EXCHANGE_LINK_CALENDAR.day>='2015-01-04').limit(4)
df=finance.run_query(q)
print(df)

id         day  link_id link_name  type_id   type
0  1244833  2015-01-04   310001       沪股通   312003     休市
1      148  2015-01-04   310003    港股通(沪)   312003     休市
2  1244834  2015-01-05   310001       沪股通   312001  正常交易日
3      149  2015-01-05   310003    港股通(沪)   312001  正常交易日
