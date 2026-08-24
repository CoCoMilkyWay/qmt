可转债基本资料

历史范围：上市至今；更新时间：每日19：00、22:00更新

from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_BASIC_INFO).limit(n))

描述

获取可转债基本资料

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

可转债基本资料参数

query(bond.CONBOND_BASIC_INFO)：表示从bond.CONBOND_BASIC_INFO这张表中查询可转债基本资料，其中bond是库名，REPO_DAILY_PRICE是表名。bond库中的表都可以使用run_query方法调用,在查询表数据时还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2，多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

bond.CONBOND_BASIC_INFO：获取可转债基本资料，表结构和字段信息如下：

filter(bond.CONBOND_BASIC_INFO.code==code)：指定筛选条件，通过bond.CONBOND_BASIC_INFO.code == '131801' 可以指定债券代码来获取可转债基本资料；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

名称	类型	描述

code	str	债券代码（不带后缀）

short_name	str	债券简称

full_name	str	债券全称

list_status_id	int	上市状态编码，见下表上市状态编码对照表

list_status	str	上市状态

issuer	str	发行人

company_code	str	发行人股票代码（带后缀）

issue_start_date	date	发行起始日

issue_end_date	date	发行终止日

plan_raise_fund	decimal(20,4)	计划发行总量（万元）

actual_raise_fund	decimal(20,4)	实际发行总量（万元）

issue_par	int	发行面值

issue_price	decimal(10,3)	发行价格

is_guarantee	int	是否有担保(1-是，0-否）

fund_raising_purposes	varchar(200)	募资用途说明

list_date  list_declare_date	date	上市公告日期

convert_price_reason	varchar(300)	初始转股价确定方式

convert_price	decimal(10,3)	初始转股价格

convert_start_date	start_date	转股开始日期

convert_end_date	end_date	转股终止日期

convert_code	varchar(10)	转股代码（不带后缀）

coupon	decimal(10,3)	初始票面利率

exchange_code	int	交易市场编码，见下表交易市场编码

exchange	str	交易市场

currency_id	str	货币代码。CNY-人民币

coupon_type_id	int	计息方式编码，见下表计息方式编码

coupon_type	str	计息方式

coupon_frequency	int	付息频率，单位：月/次。按年付息是12月/次；半年付息是6月/次

payment_type_id	int	兑付方式编码，见下表兑付方式编码表

payment_type	str	兑付方式

par	float	债券面值(元)

repayment_period	int	偿还期限(月）

bond_type_id	int	债券分类编码，见下表债券分类编码

bond_type	str	债券分类

bond_form_id	int	债券形式编码，见下表债券形式编码表

bond_form	str	债券形式

list_date	date	上市日期

delist_Date	date	退市日期

interest_begin_date	date	起息日

maturity_date	date	到期日

interest_date	str	付息日

last_cash_date	date	最终兑付日

cash_comment	str	兑付说明

上市状态编码

上市状态编码	上市状态

301001	正常上市

301006	终止上市

301099	其他

交易市场编码对照表

交易市场编码	交易市场

705001	上交所

705002	深交所主板

705003	深交所中小板

705004	深交所创业板

705005	上交所综合业务平台

705006	深交所综合协议交易平台

705007	银行间债券市场

705008	商业银行柜台市场

705009	港交所创业板

705010	新加坡证券交易所

705011	拟上市

705012	产权交易市场

705013	美国NASDAQ证券交易所

705014	港交所主板

705015	股份报价系统

705016	代办转让

705017	上交所CDR

705018	深交所存托凭证

705099	其他

计息方式编码

计息方式编码	计息方式

701001	利随本清

701002	固定利率附息

701003	递进利率

701004	浮动利率

701005	贴现

701006	未公布

701007	无利率

701008	累进利率

兑付方式编码

兑付方式编码	兑付方式

702001	到期一次付息

702002	按年付息

702003	按半年付息

702004	按季付息

702005	按月付息

702006	未公布

702099	其他

债券分类编码

债券分类编码	债券分类

703001	短期融资券

703002	质押式回购

703003	私募债

703004	企业债

703005	次级债

703006	一般金融债

703007	中期票据

703008	资产支持证券

703009	小微企业扶持债

703010	地方政府债

703011	公司债

703012	可交换私募债

703013	可转债

703014	集合债券

703015	国际机构债券

703016	政府支持机构债券

703017	集合票据

703018	外国主权政府人民币债券

703019	央行票据

703020	政策性金融债

703021	国债

703022	非银行金融债

703023	可分离可转债

703024	国库定期存款

703025	可交换债

703026	特种金融债

债券形式编码

债券形式编码	债券形式

704001	记账式

704002	实物式

704003	储蓄电子式

704004	凭证式

704005	未公布

示例：

# 获得鞍钢转债的基本资料
from jqdatasdk import *
df=bond.run_query(query(bond.CONBOND_BASIC_INFO).filter(bond.CONBOND_BASIC_INFO.company_code=='000898.XSHE'))
print(df)

# 输出
id    code short_name           full_name  list_status_id list_status  \
0   3  125898       鞍钢转债  鞍钢新轧钢股份有限公司可转换公司债券          301006        终止上市

issuer company_code issue_start_date issue_end_date     ...       \
0  鞍钢股份有限公司  000898.XSHE       2000-03-14     2000-03-22     ...

bond_type  bond_form_id  bond_form   list_date  delist_Date  \
0        可转债        704001        记账式  2000-04-17   2005-03-14

interest_begin_date maturity_date interest_date  last_cash_date cash_comment
0          2000-03-14    2005-03-14   存续期内每年3月14日      2005-03-14         None

[1 rows x 44 columns]
