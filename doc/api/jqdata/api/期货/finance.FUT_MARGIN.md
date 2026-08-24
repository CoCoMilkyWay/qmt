获取期货保证金(结算参数)

历史范围：2013-01-01 至今；更新时间：17:00 更新

from jqdatasdk import finance
finance.run_query(query(finance.FUT_MARGIN).filter(finance.FUT_MARGIN.day==date).limit(n))

描述

获取期货保证金(结算参数)

参数

query(finance.FUT_MARGIN)：表示从finance.FUT_MARGIN这张表中查询期货保证金数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：sqlalchemy.orm.query.Query对象

finance.FUT_MARGIN：收录了期货保证金数据，表结构和字段信息如下：

filter(finance.FUT_MARGIN.code==code)：指定筛选条件，通过finance.FUT_MARGIN.code==code可以指定你想要查询的标的；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称/li>

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

字段名称	中文名称	字段类型	注释

day	日期	date

code	标的代码	varchar(12)

exchange	交易所编码	varchar(10)	英文编码

exchange_name	交易所名称	varchar(30)

specul_buy_margin_rate	投机买保证金率	DECIMAL(19, 4)	上期所,大商所,广期所为交易保证金(投机),中金所为多头保证金, 郑商所为交易保证金

specul_sell_margin_rate	投机卖保证金率	DECIMAL(19, 4)	上期所/能源中心,大商所,广期所为交易保证金(投机),中金所为空头保证金, 郑商所为交易保证金

hedg_buy_margin_rate	套保买保证金率	DECIMAL(19, 4)	上期所/能源中心,大商所,广期所为交易保证金(套保)

hedg_sell_margin_rate	套保卖保证金率	DECIMAL(19, 4)	上期所/能源中心,大商所,广期所为交易保证金(套保)

示例

#查询SI2312的2022年12月22日的保证金数据
from jqdatasdk import finance
df = finance.run_query(query(finance.FUT_MARGIN).filter(finance.FUT_MARGIN.day == '2022-12-22',
finance.FUT_MARGIN.code == "SI2312.GFEX"
))
print(df)

id        date  ... hedg_buy_margin_rate hedg_sell_margin_rate
0  637002  2022-12-22  ...                 10.0                  10.0

[1 rows x 9 columns]
