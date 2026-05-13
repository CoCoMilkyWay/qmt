获取数据交易列表

get_all_securities(types=[], date=None)

描述

获取平台支持的所有股票、基金、指数、期货、期权、可转债信息

参数

属性	名称	字段类型	备注

types	类型	用list的形式过滤securities的类型,	list元素可选: 'stock', 'fund', 'index', 'futures','conbond', 'etf','lof',
'fja', 'fjb','open_fund', 'bond_fund',
'stock_fund', 'QDII_fund', 'money_market_fund', 'mixture_fund'
types为空时返回所有股票

date	日期	日期字符串或者
[datetime.datetime]/[datetime.date] 对象	用于获取某日期还在上市的股票信息.
默认值为 None, 表示获取所有日期的股票信息

返回结果

属性	名称	备注

display_name	中文名称

name	缩写简称

start_date	上市日期	[datetime.date] 类型

end_date	退市日期	[datetime.date] 类型,

如果没有退市则为2200-01-01)

type	类型
stock(股票),index(交易所指数),
options(期权) , conbond(可转债),futures（期货)

etf(ETF基金)，fja（分级A），fjb（分级B），fjm（分级母基金），
mmf（场内交易的货币基金）open_fund（开放式基金）,’bond_fund（债券基金）, stock_fund（股票型基金）
QDII_fund（QDII 基金）, money_market_fund（场外交易的货币基金）,mixture_fund（混合型基金）,

示例

#获得所有股票列表
df=get_all_securities()
print(df[:2])

display_name  name start_date   end_date   type
000001.XSHE         平安银行  PAYH 1991-04-03 2200-01-01  stock
000002.XSHE          万科A   WKA 1991-01-29 2200-01-01  stock

#获得2020年10月10日还在上市的所有股票列表
df=get_all_securities(date='2020-10-10')
print(df[:5])
display_name  name start_date   end_date   type
000001.XSHE         平安银行  PAYH 1991-04-03 2200-01-01  stock
000002.XSHE          万科A   WKA 1991-01-29 2200-01-01  stock
000004.XSHE         国华网安  GHWA 1990-12-01 2200-01-01  stock
000005.XSHE         世纪星源  SJXY 1990-12-10 2200-01-01  stock
000006.XSHE         深振业A  SZYA 1992-04-27 2200-01-01  stock

#将所有股票列表转换成数组
stocks = list(get_all_securities(['stock']).index)
stocks[:5]
>>>['000001.XSHE', '000002.XSHE', '000004.XSHE']
