期权合约资料

历史范围：2019/12/2至今；更新频率：盘后18:00更新

from jqdatasdk import opt
opt.run_query(query(opt.OPT_CONTRACT_INFO).filter(opt.OPT_CONTRACT_INFO.code==code).limit(n))

描述

支持ETF期权、股指期权及商品期权，提供最基础的合约信息

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

期权合约资料参数

query(opt.OPT_CONTRACT_INFO)：表示从opt.OPT_CONTRACT_INFO这张表中查询期权基本资料数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

opt.OPT_CONTRACT_INFO：收录了期权基本资料数据，表结构和字段信息如下：

filter(opt.OPT_CONTRACT_INFO.code==code)：指定筛选条件，通过opt.OPT_CONTRACT_INFO.code==code可以指定你想要查询的合约代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述	示例	备注

code	str	合约代码	10001313.XSHG；CU1901C46000.XSGE；SR903C4700.XZCE；M1707-C-2400.XDCE	注意合约代码使用大写字母

trading_code	int	合约交易代码	510050C1810M02800	合约调整会产生新的交易代码

name	str	合约简称	50ETF购10月2800豆粕购7月2400	合约调整会产生新的合约简称

contract_type	str	合约类型。CO-认购期权，PO-认沽期权	CO

exchange_code	str	证券市场编码，XSHG：上海证券交易所；XSHE:深圳证券交易所；XSGE：上海期货交易所；XZCE：郑州商品交易所；XDCE：大连商品交易所；CCFX:中国金融期货交易所；GFEX:广州期货交易所；XINE:上海国际能源期货交易所	XSHG

currency_id	str	货币代码CNY-人民币	CNY

underlying_symbol	str	标的代码	510050.XSHG

underlying_name	str	标的简称	华夏上证50ETF

underlying_exchange	str	标的交易市场	XSHG

underlying_type	str	标的品种类别。ETF-交易型开放式指数基金FUTURE-期货	ETF

exercise_price	float	行权价格	2.8	合约调整会产生新的行权价格

contract_unit	int	合约单位	10000	合约调整会产生新的合约单位

contract_status	str	合约状态：LIST-上市、DELIST-退市。SUSPEND-停牌	DELIST	新期权上市由交易所公布LIST：挂牌日期<=当前日期<=最后交易日DELIST：当前日期>最后交易日

list_date	str	挂牌日期	2018-09-25

list_reason	str	合约挂牌原因

list_price	decimal(20,4)	开盘参考价	合约挂牌当天交易所会公布

high_limit	decimal(20,4)	挂牌涨停价	合约挂牌当天交易所会公布

low_limit	decimal(20,4)	挂牌跌停价	合约上市当天交易所会公布

expire_date	str	到期日	2018/10/24

last_trade_date	str	最后交易日	2018/10/24

exercise_date	str	行权日	2018/10/24	50ETF，铜期权是欧式期权，行权日固定。白糖期权和豆粕期权是美式期权，到期日之前都可以行权，行权日不固定，可为空。

delivery_date	str	交收日期	2018/10/25

is_adjust	int	是否调整	原合约调整为新的合约会发生合约资料的变化1-是，0-否

delist_date	str	摘牌日期	2018/10/24

delist_reason	str	合约摘牌原因

示例：

#查询"10001313.XSHG "最新的期权基本资料数据。
from jqdatasdk import *
q=query(opt.OPT_CONTRACT_INFO).filter(opt.OPT_CONTRACT_INFO.code=='10001313.XSHG')
df=opt.run_query(q)
print(df)

id           code       trading_code            name contract_type  \
0  2435  10001313.XSHG  510050C1812A02500  50ETF购12月2450A            CO

exchange_code currency_id underlying_symbol underlying_name  \
0          XSHG         CNY       510050.XSHG           50ETF

underlying_exchange      ...      list_price  high_limit  low_limit  \
0                XSHG      ...          0.3523      0.6216      0.083

expire_date last_trade_date exercise_date  delivery_date  is_adjust  \
0  2018-12-26      2018-12-26    2018-12-26     2018-12-27          1

delist_date delist_reason
0         None          None
