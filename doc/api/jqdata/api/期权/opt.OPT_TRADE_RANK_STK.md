期权交易和持仓排名统计

历史范围：2019/12/2至今；更新频率：下一交易日盘前8:05更新

from jqdatasdk import opt
opt.run_query(query(opt.OPT_TRADE_RANK_STK).filter(opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol).limit(n))

描述

统计沪深ETF期权每日最活跃三个合约的交易排名和持仓量最大三个合约的持仓排名情况

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

期权日行情参数

query(opt.OPT_TRADE_RANK_STK)：表示从opt.OPT_TRADE_RANK_STK这张表中查询沪深ETF期权交易和持仓排名统计数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

filter(opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol)：指定筛选条件，通过opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol可以指定你想要查询的标的代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。：

filter(opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol)：指定筛选条件，通过opt.OPT_TRADE_RANK_STK.underlying_symbol==underlying_symbol可以指定你想要查询的标的代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述	示例

underlying_symbol	str	标的代码	510050.XSHG

underlying_name	str	标的简称	华夏上证50ETF

underlying_exchange	str	证券市场编码：XSHG-上海证券交易所；	XSHG

date	str	交易日期	2018-10-25

rank	int	排名	1

volume	int	数量(张）	184891

option_agency	str	期权经营机构	华泰证券

rank_type	str	排名统计类型601001：最活跃三个合约的认购交易排名；
601002：最活跃三个合约的认沽交易排名；
601003：持仓最大3个合约的认购持仓量排名；
601004：持仓最大3个合约的认沽持仓量排名	601001

示例：

#查询最活跃三个合约的认购交易排名（601001）
from jqdatasdk import *
q=query(opt.OPT_TRADE_RANK_STK).filter(opt.OPT_TRADE_RANK_STK.rank_type==601001).order_by(opt.OPT_TRADE_RANK_STK.date.desc()).limit(10)
df=opt.run_query(q)
print(df)

id underlying_symbol underlying_name underlying_exchange        date  \
0  41627       510050.XSHG           50ETF                XSHG  2021-03-18
1  41671       510300.XSHG    华泰柏瑞沪深300ETF                XSHG  2021-03-18
2  41628       510050.XSHG           50ETF                XSHG  2021-03-18
3  41629       510050.XSHG           50ETF                XSHG  2021-03-18
4  41630       510050.XSHG           50ETF                XSHG  2021-03-18
5  41631       510050.XSHG           50ETF                XSHG  2021-03-18
6  41647       159919.XSHE      嘉实沪深300ETF                XSHE  2021-03-18
7  41648       159919.XSHE      嘉实沪深300ETF                XSHE  2021-03-18
8  41649       159919.XSHE      嘉实沪深300ETF                XSHE  2021-03-18
9  41670       510300.XSHG    华泰柏瑞沪深300ETF                XSHG  2021-03-18

rank  volume option_agency rank_type
0     2  389167          广发证券    601001
1     3  171957          南华期货    601001
2     4  136430          中信证券    601001
3     5  115294          招商证券    601001
4     1  405170          华泰证券    601001
5     3  152924          南华期货    601001
6     2   44316          广发证券    601001
7     3   27527          中信证券    601001
8     5   21805          招商证券    601001
9     1  375776          华泰证券    601001
