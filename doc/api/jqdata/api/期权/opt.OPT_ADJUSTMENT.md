期权合约调整记录

历史范围：上市至今；更新频率：盘后18:00更新

from jqdatasdk  import opt
opt.run_query(query(opt.OPT_ADJUSTMENT).filter(opt.OPT_ADJUSTMENT.code==code).limit(n))

描述

支持ETF期权

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

query(opt.OPT_ADJUSTMENT)：表示从opt.OPT_ADJUSTMENT这张表中查询期权合约调整数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

opt.OPT_ADJUSTMENT：收录了期权合约调整数据，表结构和字段信息如下

filter(opt.OPT_ADJUSTMENT.code==code)：指定筛选条件，通过opt.OPT_ADJUSTMENT.code==code可以指定你想要查询的合约代码；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述	示例	备注

code	str	合约代码	10001313.XSHG;

adj_date	date	调整日期

contract_type	str	合约类型。CO-认购期权，PO-认沽期权	CO

ex_trading_code	int	原交易代码	10001465Nan	合约调整会产生新的交易代码

ex_name	str	原合约简称	50ETF购10月2800;豆粕购7月2400	合约调整会产生新的合约简称

ex_exercise_price	float	原行权价

ex_contract_unit	int	原合约单位

new_trading_code	str	新交易代码

new_name	str	新合约简称

new_exercise_price	float	新行权价

new_contract_unit	int	新合约单位

adj_reason	str	调整原因

expire_date	str	到期日	2018/10/24

last_trade_date	str	最后交易日	2018/10/24

exercise_date	str	行权日	2018/10/24	50ETF期权是欧式期权，行权日固定

delivery_date	str	交收日期	2018/10/25

position	int	合约持仓

示例：

#查询"10001313.XSHG "最新的期权合约调整记录。
from jqdatasdk import opt
q=query(opt.OPT_ADJUSTMENT).filter(opt.OPT_ADJUSTMENT.code=='10001313.XSHG')
df=opt.run_query(q)
print(df)

id           code    adj_date contract_type    ex_trading_code  \
0  70  10001313.XSHG  2018-12-03            CO  510050C1812M02500

ex_name  ex_exercise_price  ex_contract_unit   new_trading_code  \
0  50ETF购12月2500                2.5             10000  510050C1812A02500

new_name  new_exercise_price  new_contract_unit adj_reason  \
0  50ETF购12月2450A                2.45              10202         除息

expire_date last_trade_date exercise_date delivery_date  position
0  2018-12-26      2018-12-26    2018-12-26    2018-12-27    107928
