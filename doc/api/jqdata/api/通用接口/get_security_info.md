获取单支标的信息

get_security_info(code,date=None)

描述

code: 证券代码

使用方式:接口+.属性，具体属性说明如下图

注意

使用get_all_securities方法，即可获取完整的证券交易列表代码

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

parent	分级基金的母基金代码

示例

获取中文名称

# 获取000001.XSHE某一日期的中文名称
display_name = get_security_info('000001.XSHE',date='2017-08-31’).display_name
print(display_name)
>>>平安银行

#获取000001.XSHE的上市时间
start_date = get_security_info('000001.XSHE').start_date
print(start_date)
>>>1991-04-03

#获取150008.XSHE的母基金代码
parent = get_security_info('150008.XSHE').parent
print(parent)
