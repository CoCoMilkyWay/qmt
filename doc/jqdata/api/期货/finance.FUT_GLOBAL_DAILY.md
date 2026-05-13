外盘日行情数据

历史范围：2005年至今；更新时间：盘前09:00更新

from jqdatasdk import finance
df=finance.run_query(query(finance.FUT_GLOBAL_DAILY).filter(finance.FUT_GLOBAL_DAILY.day==date).limit(n))

描述：记录主要外盘商品期货的日行情数据，包含开盘价、收盘价、最高价、最低价、成交量等

参数：

query(finance.FUT_GLOBAL_DAILY)：表示从finance.FUT_GLOBAL_DAILY这张表中查询外盘期货日行情数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：query简易教程

finance.FUT_GLOBAL_DAILY：收录了外盘期货日行情数据，表结构和字段信息如下：

字段设计

字段	名称	类型	非空	含义

code	期货代码	varchar(64)	Y	代码列表详见下方期货代码名称对照表

name	期货名称	varchar(64)

day	日期	date	Y

open	开盘价	decimal(20,6)

close	收盘价	decimal(20,6)

low	最低价	decimal(20,6)

high	最高价	decimal(20,6)

volume	成交量	decimal(20,6)

change_pct	涨跌幅（%）	decimal(20,4)	（当日收盘价-前收价）/前收价

amplitude	振幅（%）	decimal(20,6)	（当日最高点的价格－当日最低点的价格）/前收价

pre_close	前收价	decimal(20,6)

期货代码名称对照表

代码	AHD	BO	C	CAD	CL	ES	GC	HG	LHC	NG	NID

名称	LME铝3个月	CBOT-黄豆油	CBOT-玉米	LME铜3个月	NYMEX原油	标普期货	COMEX黄金	COMEX铜	CME-瘦肉猪	NYMEX天然气	LME镍3个月

代码	OIL	PBD	S	SI	SM	SND	W	XAG	XAU	XPD	XPT	ZSD

名称	布伦特原油	LME铅3个月	CBOT-黄豆	COMEX白银	CBOT-黄豆粉	LME锡3个月	CBOT-小麦	伦敦银	伦敦金	伦敦钯金	伦敦铂金	LME锌3个月

filter(finance.FUT_GLOBAL_DAILY.day==date)：指定筛选条件，通过finance.FUT_GLOBAL_DAILY.day==date可以指定你想要查询的行情日期；除此之外，还可以对表中其他字段指定筛选条件，如finance.FUT_GLOBAL_DAILY.code==code，表示指定标的代码查询日行情；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

返回结果：

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意：

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

示例：

df=finance.run_query(query(finance.FUT_GLOBAL_DAILY).filter(finance.FUT_GLOBAL_DAILY.day=='2019-06-27'))[:5]
df
id code      name         day      open     close       low      high  \
0  93431  AHD   LME铝3个月  2019-06-27  1815.000  1797.000  1788.000  1825.000
1  92994   BO  CBOT-黄豆油  2019-06-27    27.860    27.791    27.650    28.040
2  93032   BP    IMM-英镑  2019-06-27     1.274     1.271     1.271     1.277
3  93127    C   CBOT-玉米  2019-06-27   442.750   440.001   438.500   445.500
4  93070  CAD   LME铜3个月  2019-06-27  5986.500  5998.500  5960.000  6016.000

volume  change_pct  amplitude  pre_close
0  11737.0     -0.9372   2.039691   1814.000
1    563.0     -0.1437   1.401315     27.831
2  65982.0     -0.1571   0.471328      1.273
3   1143.0     -0.6772   1.580132    443.001
4  12717.0      0.0834   0.934346   5993.500
