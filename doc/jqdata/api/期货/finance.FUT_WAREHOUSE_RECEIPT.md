获取期货仓单数据

历史范围：2005年至今；更新时间：盘后20:00更新

from jqdatasdk import finance
finance.run_query(query(finance.FUT_WAREHOUSE_RECEIPT).filter(finance.FUT_WAREHOUSE_RECEIPT.underlying_code==underlying_code).limit(n))

描述

期货仓单是指由期货交易所指定交割仓库，按照期货交易所指定的程序，签发的符合合约规定质量的实物提货凭证。记录了交易所所有期货实物的库存情况以及变更情况。

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

新增 run_offset_query方法，支持分批从数据库提取超5000条的数据，上限20万条

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

参数

期货仓单数据参数

query(finance.FUT_WAREHOUSE_RECEIPT)：表示从finance.FUT_WAREHOUSE_RECEIPT这张表中查询期货仓单数据，还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：[query简易教程]

finance.FUT_MEMBER_POSITION_RANK：收录了期货仓单数据，表结构和字段信息如下：

filter(finance.FUT_WAREHOUSE_RECEIPT.underlying_code==underlying_code)：指定筛选条件，通过finance.FUT_WAREHOUSE_RECEIPT.underlying_code==underlying_code可以指定你想要查询的品种；除此之外，还可以对表中其他字段指定筛选条件；多个筛选条件用英文逗号分隔。

limit(n)：限制返回的数据条数，n指定返回条数。

字段名称	中文名称	字段类型	注释

day	日期	date

exchange	交易所编码	varchar(10)	英文编码

exchange_name	交易所名称	varchar(30)	上海期货交易所
大连商品交易所
郑州商商品交易所
中国金融期货交易所

underlying_code	品种编码	varchar(10)

product_name	品种名称	varchar(20)

warehouse_name	仓库名称	varchar(20)	上期所：将地区和仓库数据合并成一条，仓库名称=“地区”+“仓库”。
大商所：仓库名称存在多个不同的名字的，取第一个字体加粗的仓库名称。
郑商所：不区分品牌，对每个仓库取仓库小计值
该字段可能因不可抗力只按照品种维度维护，仓库名称维护为【其他】

warehouse_receipt_number	今日期货仓单	int

unit	单位	varchar(10)

warehouse_receipt_number_increase	比昨日增减	int

交易所编号

交易所名称	编码

上海期货交易所	XSGE

大连商品交易所	XDCE

郑州商商品交易所	XZCE

示例：

#查询品种为铜（'CU')的2018年12月05日的仓单数据
q=query(finance.FUT_WAREHOUSE_RECEIPT.day,
finance.FUT_WAREHOUSE_RECEIPT.underlying_code,
finance.FUT_WAREHOUSE_RECEIPT.warehouse_name,
finance.FUT_WAREHOUSE_RECEIPT.warehouse_receipt_number).filter(finance.FUT_WAREHOUSE_RECEIPT.underlying_code=='CU',finance.FUT_WAREHOUSE_RECEIPT.day=='2018-12-05').order_by(finance.FUT_WAREHOUSE_RECEIPT.warehouse_receipt_number.desc())
df=finance.run_query(q)
print(df[:8])

day underlying_code warehouse_name  warehouse_receipt_number
0  2018-12-05              CU        江苏-中储无锡                     10517
1  2018-12-05              CU       广东-八三O黄浦                      9141
2  2018-12-05              CU        江苏-常州融达                      6787
3  2018-12-05              CU        上海-中储吴淞                      5238
4  2018-12-05              CU        广东-南储仓储                      5025
5  2018-12-05              CU      浙江-国储837处                      4205
6  2018-12-05              CU        上海-上港物流                      3524
7  2018-12-05              CU      上海-中海华东宝山                      2804
