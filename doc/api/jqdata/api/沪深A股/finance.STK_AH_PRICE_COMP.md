市场通AH股价对比

历史范围：上市至今；更新时间：交易日20:30-06:30更新

from jqdatasdk import finance

finance.run_query(query(finance.STK_AH_PRICE_COMP).filter(finance.STK_AH_PRICE_COMP.a_code==a_code).order_by(finance.STK_AH_PRICE_COMP.day).limit(n)

描述

记录同时在A股和H股上市的股票的价格比对。

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

query(finance.STK_AH_PRICE_COMP)：表示从finance.STK_AH_PRICE_COMP这张表中查询同时在A股和H股上市的股票的价格对比，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_AH_PRICE_COMP：代表A股和H股的价格对比表，收录了同时在A股和H股上市的股票的价格对比情况，包括收盘价，涨跌幅等，表结构和字段信息如下：

filter(finance.STK_AH_PRICE_COMP.a_code==a_code)：指定筛选条件，通过finance.STK_AH_PRICE_COMP.a_code==a_code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_AH_PRICE_COMP.day>='2015-01-01'，表示筛选日期大于等于2015年1月1日之后的数据；多个筛选条件用英文逗号分隔。

order_by(finance.STK_AH_PRICE_COMP.day)： 将返回结果按日期排序

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段	名称	类型	非空	备注/示例

day	日期	date	Y

name	股票简称	varchar(32)	Y

a_code	a股代码	varchar(12)	Y	'000002.XSHE'

h_code	h股代码	varchar(12)	Y

a_price	a股收盘价	decimal(10,4)	人民币

h_price	h股收盘价	decimal(10,4)	港币

a_quote_change	a股涨跌幅	decimal(10,4)	%

h_quote_change	h股涨跌幅	decimal(10,4)	%

h_a_comp	比价(H/A)	decimal(10,4)	A股人民币价格/(H股港币价格*港币兑人民币的汇率)

示例

q=query(finance.STK_AH_PRICE_COMP).filter(finance.STK_AH_PRICE_COMP.a_code=='000002.XSHE').order_by(finance.STK_AH_PRICE_COMP.day).limit(4)
df=finance.run_query(q)
print(df)

id         day name       a_code h_code  a_price  h_price  \
0  584546  2014-06-25  万科A  000002.XSHE  02202     8.13    13.28
1  584547  2014-06-26  万科A  000002.XSHE  02202     8.26    14.18
2  584548  2014-06-27  万科A  000002.XSHE  02202     8.32    13.92
3  584549  2014-06-30  万科A  000002.XSHE  02202     8.27    13.76

a_quote_change  h_quote_change  h_a_comp
0          0.4944            0.00    0.7624
1          1.5990            6.78    0.7251
2          0.7264           -1.83    0.7453
3         -0.6010           -1.15    0.7498
