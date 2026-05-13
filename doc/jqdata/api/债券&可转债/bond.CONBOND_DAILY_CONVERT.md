可转债转每日转股统计

历史范围：上市至今；更新时间：下一交易日 8:30、12：30更新

from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_DAILY_CONVERT).limit(n))

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

可转债转每日转股统计参数

query(bond.CONBOND_DAILY_CONVERT)：表示从bond.CONBOND_DAILY_CONVERT这张表中查询可转债转每日转股统计，其中bond是库名，CONBOND_DAILY_CONVERT是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.CONBOND_DAILY_CONVERT：获取可转债转每日转股统计，表结构和字段信息如下：

filter(bond.CONBOND_DAILY_CONVERT.code==code)：指定筛选条件，通过bond.CONBOND_DAILY_CONVERT.code == '131801' 可以指定债券代码来获取可转债转每日转股统计；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述

date	date	交易日期（以YYYY-MM-DD表示）

code	str	债券代码（不带后缀）

name	str	债券简称

exchange_code	str	证券市场编码（XSHG-上海证券交易所；XSHE-深圳证券交易所）

issue_number	int	发行总量（单位：张）

convert_price	float	转股价格

daily_convert_number	int	当日转股数量（深交所披露为债券转换量 单位：张，上交所披露为股票转换量 单位 :股）

acc_convert_number	int	累计转股数量（深交所披露为债券转换量 单位：张，上交所披露为股票转换量 单位 :股）

acc_convert_ratio	float	累计转股比例（单位：% ， 因上交所只披露转股股数，因此计算剩余转股张数时公式应为 : 发行总量 *(1 -累计转股比例)  ）

convert_premium	float	转股溢价，从2018-09-13开始计算
（每张可转债转股后可以获得的收益，单位：元。转股溢价=可转债收盘价-（100/转股价格）*正股收盘价）

convert_premium_rate	float	转股溢价率

示例

#获得招行转债的每日转股统计数据
df=bond.run_query(query(bond.CONBOND_DAILY_CONVERT).filter(bond.CONBOND_DAILY_CONVERT.code=="110036"))
print(df[:5])

id        date    code  name exchange_code  issue_number  convert_price  \
0  2990  2005-05-10  110036  招行转债          XSHG      65000000           9.34
1  2994  2005-05-11  110036  招行转债          XSHG      65000000           9.34
2  3000  2005-05-13  110036  招行转债          XSHG      65000000           9.34
3  3012  2005-05-18  110036  招行转债          XSHG      65000000           9.34
4  3017  2005-05-23  110036  招行转债          XSHG      65000000           9.34

daily_convert_number  acc_convert_number  acc_convert_ratio  \
0                1498.0              1498.0                0.0
1                2996.0              4494.0                0.0
2                 214.0              4708.0                0.0
3                 428.0              5136.0                0.0
4                 321.0              5457.0                0.0

convert_premium convert_premium_rate
0           5.8107                 None
1           6.9325                 None
2           9.4599                 None
3           9.4957                 None
4          12.3748                 None
