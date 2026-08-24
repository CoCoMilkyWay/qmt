债券付息事件

历史范围：上市至今；更新时间：每日19：00、22:00更新

from jqdatasdk import *
df=bond.run_query(query(bond.BOND_INTEREST_PAYMENT ).limit(n))

描述

获取债券付息事件

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

债券付息事件参数

query(bond.BOND_INTEREST_PAYMENT )：表示从bond.BOND_INTEREST_PAYMENT 这张表中查询债券付息事件数据，其中bond是库名，BOND_INTEREST_PAYMENT 是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.BOND_INTEREST_PAYMENT ：获取债券付息事件，表结构和字段信息如下：

filter(bond.BOND_INTEREST_PAYMENT .code==code)：指定筛选条件，通过bond.BOND_INTEREST_PAYMENT .code == '131801' 可以指定债券代码来获取债券付息事件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述

code	str	债券代码(不加后缀）

name	str	债券简称

pub_date	date	公告日期

exchange_code	int	证券市场编码(新增字段)

exchange	str	证券市场(新增字段)

event_type	str	事件类型

interest_start_date	date	年度计息起始日

coupon	float	票面利率（%）

interest_end_date	date	年度计息终止日

autual_interest	float	实际付息利率（%）

interest_per_unit	float	每手付息数（单位：元，每1000元付息金额）

register_date	date	债权登记日

dividend_date	date	除息日

interest_pay_start_date	date	付息起始日（债务人实际付息开始日期）

interest_pay_end_date	date	付息终止日（债务人实际付息截止日期）

payment_date	date	兑付日（债券到期兑付）

payment_per_unit	float	每百元面值的到期兑付资金（元）

tax_rate	float	代扣所得税率（%）

tax_channel	str	扣税渠道

示例：

from jqdatasdk import *
df=bond.run_query(query(bond.BOND_INTEREST_PAYMENT ).filter(bond.BOND_INTEREST_PAYMENT.pub_date=='2019-11-13'))
print(df[:5])
id       code        name    pub_date  exchange_code exchange event_type  \
0  7684  101574010  15京粮MTN001  2019-11-13          12004  银行间债券市场         付息
1  8635    1555048     15福建债48  2019-11-13          12004  银行间债券市场         付息
2  8668    1555047     15福建债47  2019-11-13          12004  银行间债券市场         付息
3  8684    1555046     15福建债46  2019-11-13          12004  银行间债券市场         付息
4  8726    1555044     15福建债44  2019-11-13          12004  银行间债券市场         付息

interest_start_date  coupon interest_end_date  autual_interest  \
0          2018-12-14    4.38        2019-12-13             4.38
1          2019-06-04    3.30        2019-12-03             1.65
2          2018-12-04    3.32        2019-12-03             3.32
3          2018-12-04    3.15        2019-12-03             3.15
4          2019-06-04    3.30        2019-12-03             1.65

interest_per_unit register_date dividend_date interest_pay_start_date  \
0               43.8    2019-12-13          None              2019-12-16
1               16.5    2019-12-03          None              2019-12-04
2               33.2    2019-12-03          None              2019-12-04
3               31.5    2019-12-03          None              2019-12-04
4               16.5    2019-12-03          None              2019-12-04

interest_pay_end_date payment_date  payment_per_unit  tax_rate tax_channel
0                  None         None               NaN       NaN        None
1                  None         None               NaN       NaN        None
2                  None         None               NaN       NaN        None
3                  None         None               NaN       NaN        None
4                  None         None               NaN       NaN        None
