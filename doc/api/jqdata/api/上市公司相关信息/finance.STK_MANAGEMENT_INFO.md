公司管理人员任职情况

历史范围：2005年至今；更新时间：交易日24:00更新

from jqdatasdk import finance
finance.run_query(query(finance.STK_MANAGEMENT_INFO).filter(finance.STK_MANAGEMENT_INFO.code==code).order_by(finance.STK_MANAGEMENT_INFO.pub_date).limit(n)

描述

记录上市公司管理人员的任职情况。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

公司管理人员参数

query(finance.STK_MANAGEMENT_INFO)：表示从finance.STK_MANAGEMENT_INFO这张表中查询上市公司管理人员任职情况，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_SHAREHOLDER_TOP10：代表了公司管理人员任职情况表，收录了上市公司管理人员的任职情况，表结构和字段信息如下：

filter(finance.STK_MANAGEMENT_INFO.code==code)：指定筛选条件，通过finance.STK_MANAGEMENT_INFO.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_MANAGEMENT_INFO.pub_date>='2015-01-01'，表示筛选公告日期大于等于2015年1月1日之后的数据；多个筛选条件用英文逗号分隔。

order_by(finance.STK_MANAGEMENT_INFO.pub_date): 将返回结果按公告日期排序

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段名称	中文名称	字段类型	备注/示例

company_id	公司ID	int

company_name	公司名称	varchar(100)

code	股票代码	varchar(12)

pub_date	公告日期	date

person_id	个人ID	int

name	姓名	varchar(40)

title_class_id	职务类别编码	int

title_class	职务类别	varchar(60)

title	职务名称	varchar(60)

start_date	任职日期	date

leave_date	离职日期	date

leave_reason	离职原因	varchar(255)

on_job	是否在职	char(1)	0-否，1-是

gender	性别	char(1)	F-女；M-男

birth_year	出生年份	varchar(8)

highest_degree_id	最高学历编码	int

highest_degree	最高学历	varchar(60)

title_level_id	职级编码	int

titile_level	职级	varchar(120)	职级代表工作的难易程度、责任轻重以及所需的资格条件相同或充分相似的职系的集合。如初级、中级、高级。

profession_certificate	专业技术资格	varchar(120)

profession_certificate	专业技术资格	varchar(120)

nationality	国籍	varchar(60)

security_career_start_year	从事证券业开始年份	varchar(8)

resume	个人简历	varchar(3000)

示例

q=query(finance.STK_MANAGEMENT_INFO).filter(finance.STK_MANAGEMENT_INFO.code=='000001.XSHE').order_by(finance.STK_MANAGEMENT_INFO.pub_date).limit(5)
df=finance.run_query(q)
print(df)

# 输出
id  company_id company_name         code    pub_date  person_id  \
0  138262   430000001   平安银行股份有限公司  000001.XSHE  2014-10-10  201309346
1  138263   430000001   平安银行股份有限公司  000001.XSHE  2014-10-10  201309346
2  138264   430000001   平安银行股份有限公司  000001.XSHE  2014-10-10  201309346
3  138265   430000001   平安银行股份有限公司  000001.XSHE  2014-10-10  201313341
4  138266   430000001   平安银行股份有限公司  000001.XSHE  2014-10-10  201313342

name  title_class_id title_class title  \
0      肖遂宁          314001       董事会成员    董事
1      肖遂宁          314003        高管成员    行长
2      肖遂宁          314001       董事会成员   董事长
3  罗伯特·巴内姆          314001       董事会成员  独立董事
4      孙昌基          314001       董事会成员  独立董事

...                         highest_degree_id  \
0                        ...                                  316004.0
1                        ...                                  316004.0
2                        ...                                  316004.0
3                        ...                                  316002.0
4                        ...                                       NaN

highest_degree title_level_id title_level profession_certificate_id  \
0          大专及其他       317003.0          高级                      None
1          大专及其他       317003.0          高级                      None
2          大专及其他       317003.0          高级                      None
3          硕士研究生            NaN        None                      None
4           None       317003.0          高级                      None

profession_certificate  nationality_id nationality  \
0                   None            None        None
1                   None            None        None
2                   None            None        None
3                   None            None        None
4                   None            None        None

security_career_start_year  \
0                        None
1                        None
2                        None
3                        None
4                        None

resume
0  肖遂宁先生：出生于1948年2月，高级经济师。曾任深圳发展银行总行行长、董事长，平安银行股份...
1  肖遂宁先生：出生于1948年2月，高级经济师。曾任深圳发展银行总行行长、董事长，平安银行股份...
2  肖遂宁先生：出生于1948年2月，高级经济师。曾任深圳发展银行总行行长、董事长，平安银行股份...
3  罗伯特巴内姆先生（RobertT.Barnum）自1997年至今在美国加州的PokerFla...
4  孙昌基，男，研究员级高级工程师。1942年8月20日出生于上海，1966年9月毕业于清华大学...

[5 rows x 26 columns]
