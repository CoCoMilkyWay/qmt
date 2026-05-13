期权行权交收信息

历史范围：2019/12/2至今；更新频率：每日10:45更新

from jqdatasdk import opt
opt.run_query(query(opt.OPT_EXERCISE_INFO).filter(opt.OPT_EXERCISE_INFO.underlying_symbol==underlying_symbol).limit(n))

描述

统计上证50ETF期权在各个行权日的交收情况，一定程度上也代表了用户对当前市场的风险偏好

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

期权行权交收信息参数

query(opt.OPT_EXERCISE_INFO)：表示从opt.OPT_EXERCISE_INFO这张表中查询期权行权交收信息数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

filter(opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol)：收录了期权行权交收信息数据，表结构和字段信息如下：

filter(opt.OPT_EXERCISE_INFO.underlying_symbol==underlying_symbol：指定筛选条件，通过opt.OPT_EXERCISE_INFO.underlying_symbol==underlying_symbol可以指定你想要查询的标的代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述	示例

underlying_symbol	str	标的代码	510050.XSHG

underlying_name	str	标的名称

exercise_date	str	行权日	2018-10-24

constract_type	str	合约类型，CO-认购期权，PO-认沽期权	CO

exercise_number	int	行权数量	12520

示例：

#查询华夏上证50ETF("510050.XSHG")最新的期权行权交收信息数据。
q=query(opt.OPT_EXERCISE_INFO).filter(opt.OPT_EXERCISE_INFO.underlying_symbol=='510050.XSHG').order_by(opt.OPT_EXERCISE_INFO.exercise_date.desc()).limit(10)
df=opt.run_query(q)
print(df)

id underlying_symbol underlying_name exercise_date contract_type  exercise_number
0   86       510050.XSHG           50ETF    2018-11-28            PO            14419
1   85       510050.XSHG           50ETF    2018-11-28            CO            17330
2   84       510050.XSHG           50ETF    2018-10-24            PO            21933
3   83       510050.XSHG           50ETF    2018-10-24            CO            12520
4   82       510050.XSHG           50ETF    2018-09-26            PO             9550
5   81       510050.XSHG           50ETF    2018-09-26            CO            20286
6   80       510050.XSHG           50ETF    2018-08-22            PO            10228
7   79       510050.XSHG           50ETF    2018-08-22            CO            10208
8   78       510050.XSHG           50ETF    2018-07-25            PO             5754
9   77       510050.XSHG           50ETF    2018-07-25            CO            19632
