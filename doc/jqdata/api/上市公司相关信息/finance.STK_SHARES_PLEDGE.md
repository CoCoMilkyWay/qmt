股东股份质押

历史范围：2005年至今；更新时间：交易日24:00更新

from jqdatasdk import finance
finance.run_query(query(finance.STK_SHARES_PLEDGE).filter(finance.STK_SHARES_PLEDGE.code==code).limit(n))

描述

获取上市公司股东股份的质押情况。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

query函数的使用技巧

query函数的更多用法详见：query简易教程

股东股份质押参数

query(finance.STK_SHARES_PLEDGE)：表示从finance.STK_SHARES_PLEDGE这张表中查询上市公司股东股份的质押情况，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用英文逗号进行分隔；query函数的更多用法详见：[query简易教程]

finance.STK_SHARES_PLEDGE：代表上市公司股东股份质押表，收录了上市公司股东股份的质押情况。表结构和字段信息如下：

filter(finance.STK_SHARES_PLEDGE.code==code)：指定筛选条件，通过finance.STK_SHARES_PLEDGE.code==code可以指定你想要查询的股票代码；除此之外，还可以对表中其他字段指定筛选条件，如finance.STK_SHARES_PLEDGE.pub_date>='2015-01-01'，表示筛选公告日期大于等于2015年1月1日之后的数据；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

表字段信息

字段名称	中文名称	字段类型	备注/示例

company_id	公司ID	int

company_name	公司名称	varchar(100)

code	股票代码	varchar(12)

pub_date	公告日期	date

pledgor_id	出质人ID	int

pledgor	出质人	varchar(100)	将资产质押出去的人成为出质人

pledgee	质权人	varchar(100)

pledge_item	质押事项	varchar(500)	质押原因，记录借款人、借款金额、币种等内容

pledge_nature_id	质押股份性质编码	int

pledge_nature	质押股份性质	varchar(120)

pledge_number	质押数量	int	股

pledge_total_ratio	占总股本比例	decimal(10,4)	%

start_date	质押起始日	date

end_date	质押终止日	date

unpledged_date	质押解除日	date

unpledged_number	质押解除数量	int

unpledged _detail	解除质押说明	varchar(1000)

is_buy_back	是否质押式回购交易	char(1)

示例

#指定查询对象为万科（000002.XSHE)的股东股份质押情况，返回条数为5条
q=query(finance.STK_SHARES_PLEDGE).filter(finance.STK_SHARES_PLEDGE.code=='000002.XSHE',finance.STK_SHARES_PLEDGE.pub_date>'2015-01-01').limit(5)
df=finance.run_query(q)
print(df)

id  company_id company_name         code    pub_date pledgor_id  \
0  82514   430000002   万科企业股份有限公司  000002.XSHE  2015-11-11       None
1  82515   430000002   万科企业股份有限公司  000002.XSHE  2016-07-14       None
2  82516   430000002   万科企业股份有限公司  000002.XSHE  2017-03-08       None
3  82517   430000002   万科企业股份有限公司  000002.XSHE  2017-03-08       None
4  82518   430000002   万科企业股份有限公司  000002.XSHE  2017-03-14       None

pledgor         pledgee  \
0  深圳市钜盛华股份有限公司  鹏华资产管理（深圳）有限公司
1  深圳市钜盛华股份有限公司    中国银河证券股份有限公司
2  深圳市钜盛华股份有限公司  鹏华资产管理（深圳）有限公司
3  深圳市钜盛华股份有限公司  鹏华资产管理（深圳）有限公司
4  深圳市钜盛华股份有限公司      平安证券股份有限公司

pledge_item  pledge_nature_id  \
0  本公司股东深圳市钜盛华股份有限公司将持有的公司728,000,000股无限售流通A股质押给鹏...               NaN
1  本公司股东深圳市钜盛华股份有限公司将持有的本公司37357300股股权质押给中国银河证券股份...          308007.0
2  本公司股东深圳市钜盛华股份有限公司将2015年10月21日质押给鹏华资产管理（深圳）有限公司...          308007.0
3  本公司股东深圳市钜盛华股份有限公司将2015年10月28日质押给鹏华资产管理（深圳）有限公司...          308007.0
4  本公司第一大股东深圳市钜盛华股份有限公司将持有的本公司182000000股流通A股股权质押给...          308007.0

pledge_nature  pledge_number  pledge_total_ratio  start_date    end_date  \
0          None    728000000.0                6.59  2015-10-15        None
1          流通A股     37357300.0                 NaN  2016-07-12        None
2          流通A股            NaN                 NaN  2015-10-21  2017-03-03
3          流通A股            NaN                 NaN  2015-10-28  2017-03-03
4          流通A股    182000000.0                 NaN  2017-03-09        None

unpledged_date  unpledged_number unpledged_detail is_buy_back
0           None               NaN             None        None
1           None               NaN             None           1
2     2017-03-03        91000000.0             None        None
3     2017-03-03        91000000.0             None        None
4           None               NaN             None           1
