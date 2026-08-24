审计报告

历史范围：2005年至今；更新时间：交易日24:00更新

from jqdatasdk import finance
finance.run_query(query(finance.STK_AUDIT_OPINION).filter(finance.STK_AUDIT_OPINION.code==code).limit(n))

描述

获取上市公司定期报告及审计报告中出具的审计意见

参数

query(finance.STK_AUDIT_OPINION)：表示从finance.STK_AUDIT_OPINION这张表中查询上市公司审计意见的所有字段信息，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：sqlalchemy.orm.query.Query对象

finance.STK_AUDIT_OPINION：收录了上市公司定期报告及审计报告中出具的审计意见，表结构和字段信息如下：

filter(finance.STK_AUDIT_OPINION.code==code)：指定筛选条件，通过finance.STK_AUDIT_OPINION.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_AUDIT_OPINION.pub_date>='2015-01-01'，表示公告日期在2015年1月1日之后发布的数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

字段	名称	类型	注释

pub_date	公告日期	DATE

end_date	报告日期	DATE

report_type	审计报告类型	TINYINT(4)	0(财务报表审计报告), 1(内部控制审计报告)

accounting_firm	会计师事务所	VARCHAR(100)

accountant	会计师	VARCHAR(100)

opinion_type_id	审计意见类型id	INTEGER(11)

opinion_type	审计意见类型	VARCHAR(20)

审计意见类型编码	审计意见类型

1	无保留

2	无保留带解释性说明

3	保留意见

4	拒绝/无法表示意见

5	否定意见

6	未经审计

7	保留带解释性说明

10	经审计（不确定具体意见类型）

11	无保留带持续经营重大不确定性

示例

#查询贵州茅台2015年之后公布的审计意见信息，限定返回条数为10条
from jqdatasdk import finance
q=query(finance.STK_AUDIT_OPINION).filter(finance.STK_AUDIT_OPINION.code=='600519.XSHG',finance.STK_AUDIT_OPINION.pub_date>='2015-01-01').limit(10)
df=finance.run_query(q)
print(df)

id         code    pub_date    end_date  report_type   accounting_firm  \
0  91458  600519.XSHG  2015-04-21  2014-12-31            0  立信会计师事务所(特殊普通合伙)
1  91459  600519.XSHG  2015-04-21  2015-03-31            0              None
2  91460  600519.XSHG  2015-08-28  2015-06-30            0              None
3  91461  600519.XSHG  2015-10-23  2015-09-30            0              None
4  91462  600519.XSHG  2016-03-24  2015-12-31            0  立信会计师事务所(特殊普通合伙)
5  91463  600519.XSHG  2016-04-21  2016-03-31            0              None
6  91464  600519.XSHG  2016-08-27  2016-06-30            0              None
7  91465  600519.XSHG  2016-10-29  2016-09-30            0              None
8  91466  600519.XSHG  2017-04-15  2016-12-31            0  立信会计师事务所(特殊普通合伙)
9  91467  600519.XSHG  2017-04-15  2016-12-31            1  立信会计师事务所(特殊普通合伙)

accountant  opinion_type_id opinion_type
0      杨雄、江山                1          无保留
1       None                6         未经审计
2       None                6         未经审计
3       None                6         未经审计
4     江山、王晓明                1          无保留
5       None                6         未经审计
6       None                6         未经审计
7       None                6         未经审计
8     江山、王晓明                1          无保留
9     江山、王晓明                1          无保留
