聚宽特色指数成分股

历史范围：2010年至今；更新时间：每天8:00检查更新

get_index_stocks(index_symbol, date=None)

描述

获取聚宽特色系列指数成分股列表

参数

index_symbol: 指数代码

date: 查询日期, 一个字符串(格式类似'2015-10-15')或者[datetime.date]/[datetime.datetime]对象, 可以是None,

返回结果

返回聚宽特色系列指数代码的list

示例

JQ= get_index_stocks('JQ0001.SPI')
print(JQ[:5])

['000001.XSHE', '000002.XSHE', '000004.XSHE', '000006.XSHE', '000007.XSHE']
