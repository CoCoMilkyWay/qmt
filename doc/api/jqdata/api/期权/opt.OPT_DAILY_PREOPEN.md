期权每日盘前静态文件

历史范围：2019/12/2至今；更新频率：盘前9:05更新

from jqdatasdk import opt
opt.run_query(query(opt.OPT_DAILY_PREOPEN).filter(opt.OPT_DAILY_PREOPEN.code==code).limit(n))

描述

支持上证ETF期权、深交所期权

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

期权每日盘前静态文件参数

query(opt.OPT_DAILY_PREOPEN)：表示从opt.OPT_DAILY_PREOPEN这张表中查询期权每日盘前静态数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

opt.OPT_DAILY_PREOPEN：收录了期权每日盘前静态数据，表结构和字段信息如下

filter(opt.OPT_DAILY_PREOPEN.code==code)：指定筛选条件，通过opt.OPT_DAILY_PREOPEN.code==code可以指定你想要查询的合约代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述	示例	备注

date	str	交易日期	2018-11-09

code	str	合约代码	10001313.XSHG

trading_code	str	合约交易代码	510050C1812M02500	保留，每个交易日仍然存在一 一对应关系

name	str	合约简称	50ETF购12月2500	保留，每个交易日仍然存在一 一对应关系

exchange_code	str	证券市场编码XSHG:上海证券交易所	XSHG

underlying_symbol	str	标的代码	510050.XSHG

underlying_name	str	标的名称	50ETF

underlying_exchange	str	标的交易市场	XSHG

underlying_type	str	标的品种类别，STOCK：股票；ETF：交易型开放式指数基金；FUTURE：期货	F

exercise_type	str	期权履约方式:A 美式;E 欧式	E

contract_type	str	合约类型。CO-认购期权，PO-认沽期权	CO

contract_unit	int	合约单位	10000

exercise_price	float	行权价格	2.5

list_date	str	挂牌日期	2018/4/26

last_trade_date	str	最后交易日	2018/12/26

exercise_date	str	行权日	2018/12/26

delivery_date	str	交收日期	2018/12/27

expire_date	str	到期日	2018/12/26

contract_version	str	合约版本号	0

position	int	持仓量	13630

pre_close	float	前收盘价	0.1391

pre_settle	float	前结算价	0.1391

pre_close_underlying	float	标的证券前收盘	2.537

is_limit	str	涨跌幅限类型，“N”为有涨跌幅限制,深交所无此字段	N

high_limit	float	涨停价	0.3928

low_limit	float	跌停价	0.0001

margin_unit	float	单位保证金	4435.4

margin_ratio_1	float	保证金计算比例参数一	12

margin_ratio_2	float	保证金计算比例参数二	7

round_lot	int	整手数	1

limit_order_min	int	单笔限价申报下限,深交所无此字段	1

limit_order_max	int	单笔限价申报上限,深交所无此字段	30

market_order_min	int	单笔市价申报下限,深交所无此字段	1

marker_order_max	int	单笔市价申报上限,深交所无此字段	10

quote_change_min	float	最小报价变动(数值)	0.0001

contract_status	str	合约状态信息,深交所无此字段	0000E	该字段为8位字符串，左起每位表示特定的含义，无定义则填空格第1位：‘0’表示可开仓，‘1’表示限制卖出开仓（不.包括备兑开仓）和买入开仓。第2位：‘0’表示未连续停牌，‘1’表示连续停牌。（预留，暂填0）第3位：‘0’表示未临近到期日，‘1’表示距离到期日不足5个交易日。第4位：‘0’表示近期未做调整，‘1’表示最近5个交易日内合约发生过调整。第5位：‘A’表示当日新挂牌的合约，‘E’表示存续的合约

示例：

#查询"10001313.XSHG "最新的期权每日盘前静态数据。
from jqdatasdk import *
q=query(opt.OPT_DAILY_PREOPEN.code,
opt.OPT_DAILY_PREOPEN.trading_code,
opt.OPT_DAILY_PREOPEN.name,
opt.OPT_DAILY_PREOPEN.exercise_date,).filter(opt.OPT_DAILY_PREOPEN.code=='10001313.XSHG').order_by(opt.OPT_DAILY_PREOPEN.date.desc()).limit(10)
df=opt.run_query(q)
print(df)

code       trading_code              name exercise_date
0   10001313.XSHG  510050C1812A02500    50ETF购12月2450A    2018-12-26
1   10001313.XSHG  510050C1812A02500    50ETF购12月2450A    2018-12-26
2   10001313.XSHG  510050C1812A02500    50ETF购12月2450A    2018-12-26
3   10001313.XSHG  510050C1812A02500  XD50ETF购12月2450A    2018-12-26
4   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
5   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
6   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
7   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
8   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
9   10001313.XSHG  510050C1812M02500     50ETF购12月2500    2018-12-26
