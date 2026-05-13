查询多日财务数据

get_fundamentals_continuously(query_object, end_date=None,count=1，panel=True)

描述

查询财务数据，详细的数据字段描述请该目录对应文档查看

返回结果

返回一个 [pandas.Panel]，如果您的环境pandas大于0.25 ，将强制返回dataframe。

注意

为了防止返回数据量过大, 我们每次最多返回10000行

新增 get_table_info(table) 方法，支持查询数据表中的字段信息

query函数的使用技巧

query函数的更多用法详见：query简易教程

参数

query_object：一个sqlalchemy.orm.query.Query对象，可以通过全局的 query 函数获取 Query 对象：[query简易教程]

end_date：查询日期, 一个字符串(格式类似'2015-10-15')或者[datetime.date]/[datetime.datetime]对象, 可以是None, 如果传入的 date 不是交易日, 则使用这个日期之前的最近的一个交易日

count：获取 end_date 前 count 个日期的数据

示例

q = query(valuation.turnover_ratio,
valuation.market_cap,
indicator.eps
).filter(valuation.code.in_(['000001.XSHE', '600000.XSHG']))
df = get_fundamentals_continuously(q, end_date='2017-12-25', count=5,panel=False)[:5]
print(df)

# 输出
day         code  turnover_ratio  market_cap   eps
0  2017-12-19  000001.XSHE          1.4174   2280.2307  0.38
1  2017-12-20  000001.XSHE          0.6539   2276.7964  0.38
2  2017-12-21  000001.XSHE          0.8779   2324.8738  0.38
3  2017-12-22  000001.XSHE          0.4391   2321.4397  0.38
4  2017-12-25  000001.XSHE          0.9372   2275.0796  0.38
