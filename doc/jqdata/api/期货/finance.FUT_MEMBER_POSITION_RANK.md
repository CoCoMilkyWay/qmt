期货龙虎榜数据(会员持仓)

历史范围：2005年至今；更新时间：盘后19:00更新

from jqdatasdk import finance
finance.run_query(query(finance.FUT_MEMBER_POSITION_RANK).filter(finance.FUT_MEMBER_POSITION_RANK.code==code).limit(n))

描述

记录各个期货交易所对不同商品下的期货合约，记录该交易所会员持仓排名前20的信息。（每天更新）

获取期货交易所会员持仓（龙虎榜）

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

期货龙虎榜参数

query(finance.FUT_MEMBER_POSITION_RANK)：表示从finance.FUT_MEMBER_POSITION_RANK这张表中查询期货龙虎榜数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FUT_MEMBER_POSITION_RANK：收录了期货龙虎榜数据，表结构和字段信息如下：

filter(finance.FUT_MEMBER_POSITION_RANK.code==code)：指定筛选条件，通过finance.FUT_MEMBER_POSITION_RANK.code==code可以指定你想要查询的合约编码；除此之外，还可以对表中其他字段指定筛选条件，如finance.FUT_MEMBER_POSITION_RANK.rank_type_ID=="501001"，表示按成交量排名查询龙虎榜数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

字段名称	中文名称	字段类型	含义

day	交易日	date

code	合约编码	varchar(12)	同一商品根据交割日的不同对应不同的期货合约，比如：'CU1807.XSGE'

exchange	交易所编码	varchar(10)	英文编码：XSGE：上海期货交易所， XDCE：大连商品交易所，XZCE：郑州商品交易所， CCFX：中国金融期货交易所，XINE:上海能源中心, GFEX : 广期所

exchange_name	交易所名称	varchar(30)

underlying_code	标的编码	varchar(10)

underlying_name	标的名称	varchar(50)

rank_type_ID	排名类别编码	int	501001-成交量排名, 501002-持买单量排名， 501003-持卖单量排名

rank_type	排名类别	varchar(50)	包含:成交量排名，持买单量排名，持卖单量排名

rank	排名	int

member_name	会员简称	varchar(50)

indicator	统计指标	int	统计指标根据排名类别确定，分别代表：成交量，持买单量，持卖单量。单位：手

indicator_increase	统计指标比上交易日增减	int	单位：手

ps：一手：是指交易单位,也就是一张合约所代表的单位数量的标的物，一般根据品种有所不同,比如：大豆一手是10吨，铜一手是5吨，国债期货交易单位是钱，一手几万元。

交易所编号

交易所名称	编码

上海期货交易所	XSGE

大连商品交易所	XDCE

郑州商品交易所	XZCE

中国金融期货交易所	CCFX

排名类别编号

交易所名称	编码

成交量排名	501001

持买单量排名	501002

持卖单量排名	501003

示例：

#查询A1905.XDCE 2019-04-24交易日的持仓量龙虎榜数据
q=query(finance.FUT_MEMBER_POSITION_RANK.day,
finance.FUT_MEMBER_POSITION_RANK.code,
finance.FUT_MEMBER_POSITION_RANK.rank_type,
finance.FUT_MEMBER_POSITION_RANK.rank,
finance.FUT_MEMBER_POSITION_RANK.member_name,
finance.FUT_MEMBER_POSITION_RANK.indicator
).filter(finance.FUT_MEMBER_POSITION_RANK.code=='A1905.XDCE',
finance.FUT_MEMBER_POSITION_RANK.rank_type_ID==501002,
finance.FUT_MEMBER_POSITION_RANK.day == '2019-04-24'
).order_by( finance.FUT_MEMBER_POSITION_RANK.rank ).limit(8)
df=finance.run_query(q)
print(df)

day        code rank_type  rank member_name  indicator
0  2019-04-24  A1905.XDCE    持买单量排名     1        国投安信       1330
1  2019-04-24  A1905.XDCE    持买单量排名     2        广州金控       1026
2  2019-04-24  A1905.XDCE    持买单量排名     3        东证期货        986
3  2019-04-24  A1905.XDCE    持买单量排名     4        中信期货        844
4  2019-04-24  A1905.XDCE    持买单量排名     5        华泰期货        654
5  2019-04-24  A1905.XDCE    持买单量排名     6        东方财富        483
6  2019-04-24  A1905.XDCE    持买单量排名     7        摩根大通        462
7  2019-04-24  A1905.XDCE    持买单量排名     8        广州期货        442
