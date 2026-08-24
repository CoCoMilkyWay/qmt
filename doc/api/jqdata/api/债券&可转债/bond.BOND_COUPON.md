债券票面利率

历史范围：上市至今；更新时间：每日19：00、22:00更新

from jqdatasdk import *
df=bond.run_query(query(bond.BOND_COUPON).limit(n))

描述

获取债券基本信息数据

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

债券票面利率参数

query(bond.BOND_COUPON)：表示从bond.BOND_COUPON这张表中查询债券票面利率信息数据，其中bond是库名，BOND_COUPON是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.BOND_BASIC_INFO：获取债券票面利率数据，表结构和字段信息如下：

filter(bond.BOND_COUPON.code==code)：指定筛选条件，通过bond.BOND_COUPON.code == '131801' 可以指定债券代码来获取债券票面利率信息数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

| 名称                | 类型  | 描述                                                       |

名称	类型	描述

code	str	债券代码（不加后缀）

short_name	str	债券简称

pub_date	date	信息发布日期

coupon_type_id	int	计息方式编码，见下表计息方式编码

exchange_code	int	证券市场编码(新增字段)

exchange	str	证券市场(新增字段)

coupon_type	str	计息方式

coupon	float(5)	票面年利率(%)

coupon_start_date	date	票面利率起始适用日期

coupon_end_date	date	票面利率终止适用日期

reference_rate	float	浮息债参考利率(%)

reference_rate_comment	str	浮息债参考利率说明

margin_rate	float	浮息债利差(%)-(等于票面利率减参考利率）

coupon_upper_limit	float	利率上限

coupon_lower_limit	float	利率下限

编码对照表

计息方式编码	计息方式

701001	利随本清

701002	固定利率附息

701003	递进利率

701004	浮动利率

701005	贴现

701006	未公布

701007	无利率

示例：

# 获取15建元2A3的债券票面利率
from jqdatasdk import *
df=bond.run_query(query(bond.BOND_COUPON).filter(bond.BOND_COUPON.code=='1589355'))
print(df[:4])

id     code short_name    pub_date  exchange_code exchange  \
0  1954  1589355    15建元2A3  2015-12-23         705007  银行间债券市场
1  1953  1589355    15建元2A3  2016-01-19         705007  银行间债券市场
2  1952  1589355    15建元2A3  2016-02-19         705007  银行间债券市场
3  1951  1589355    15建元2A3  2016-03-18         705007  银行间债券市场

coupon_type_id coupon_type  coupon coupon_start_date coupon_end_date  \
0          701004        浮动利率     4.6        2015-12-24      2016-01-25
1          701004        浮动利率     4.6        2016-01-26      2016-02-25
2          701004        浮动利率     4.6        2016-02-26      2016-03-25
3          701004        浮动利率     4.6        2016-03-26      2016-04-25

reference_rate                             reference_rate_comment  \
0             4.9  票面利率：“优先档资产支持证券的票面利率”根据簿记建档结果确定。“受托机构”于“交割日”后次...
1             4.9  票面利率：“优先档资产支持证券的票面利率”根据簿记建档结果确定。“受托机构”于“交割日”后次...
2             4.9  票面利率：“优先档资产支持证券的票面利率”根据簿记建档结果确定。“受托机构”于“交割日”后次...
3             4.9  票面利率：“优先档资产支持证券的票面利率”根据簿记建档结果确定。“受托机构”于“交割日”后次...

margin_rate coupon_upper_limit coupon_lower_limit
0         -0.3               None               None
1         -0.3               None               None
2         -0.3               None               None
3         -0.3               None               None
