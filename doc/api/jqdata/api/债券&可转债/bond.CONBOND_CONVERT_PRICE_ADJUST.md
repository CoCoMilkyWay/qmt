可转债转股价格调整

历史范围：上市至今；更新时间：每日19：00、22:00更新

from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_CONVERT_PRICE_ADJUST))

描述

获取可转债转股价格调整

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

可转债转股价格调整参数

query(bond.CONBOND_CONVERT_PRICE_ADJUST)：表示从bond.CONBOND_CONVERT_PRICE_ADJUST这张表中查询可转债转股价格调整，其中bond是库名，CONBOND_CONVERT_PRICE_ADJUST是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.CONBOND_CONVERT_PRICE_ADJUST：获取可转债转股价格调整，表结构和字段信息如下：

filter(bond.CONBOND_CONVERT_PRICE_ADJUST.code==code)：指定筛选条件，通过bond.CONBOND_CONVERT_PRICE_ADJUST.code == '131801' 可以指定债券代码来获取可转债基本资料；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述

code	str	债券代码（不带后缀）

name	str	债券名称

pub_date	date	公告日期

adjust_date	date	调整生效日期

new_convert_price	float	调整后转股价格

adjust_reason	str	调整原因

#获得公告日期2019-11-07可转债的价格调整信息
from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_CONVERT_PRICE_ADJUST).filter(bond.CONBOND_CONVERT_PRICE_ADJUST.pub_date=='2019-11-07'))
print(df)

# 输出
id    code                            name    pub_date adjust_date  \
0   1  117135  2019年海亮集团有限公司非公开发行可交换公司债券(第二期)  2019-11-07  2019-11-12

new_convert_price adjust_reason
0              10.08         修正转股价
