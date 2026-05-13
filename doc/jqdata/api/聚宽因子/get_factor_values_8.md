技术类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取技术类因子值

注意

为保证数据的连续性，所有数据基于后复权计算

为了防止单次返回数据时间过长，每次调用 api 请求的因子值不能超过 200000 个

参数

securities:股票池，单只股票（字符串）或一个股票列表

factors: 因子名称，单个因子（字符串）或一个因子列表

start_date:开始日期，字符串或 datetime 对象，与 coun t参数二选一

end_date: 结束日期， 字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日），与 start_date 参数二选一

返回

一个 dict： key 是因子名称， value 是 pandas.dataframe。

dataframe 的 index 是日期， column 是股票代码， value 是因子值

因子 code	因子名称	计算方法

boll_down	下轨线（布林线）指标	(MA(CLOSE,M)-2*STD(CLOSE,M)) / 今日收盘价; M=20

boll_up	上轨线（布林线）指标	(MA(CLOSE,M)+2*STD(CLOSE,M)) / 今日收盘价; M=20

EMA5	5日指数移动均线	5日指数移动均线 / 今日收盘价

EMAC10	10日指数移动均线	10日指数移动均线 / 今日收盘价

EMAC12	12日指数移动均线	12日指数移动均线 / 今日收盘价

EMAC20	20日指数移动均线	20日指数移动均线 / 今日收盘价

EMAC26	26日指数移动均线	26日指数移动均线 / 今日收盘价

EMAC120	120日指数移动均线	120日指数移动均线 / 今日收盘价

MAC5	5日移动均线	5日移动均线 / 今日收盘价

MAC10	10日移动均线	10日移动均线 / 今日收盘价

MAC20	20日移动均线	20日移动均线 / 今日收盘价

MAC60	60日移动均线	60日移动均线 / 今日收盘价

MAC120	120日移动均线	120日移动均线 / 今日收盘价

MACDC	平滑异同移动平均线	MACD(SHORT=12, LONG=26, MID=9) / 今日收盘价

MFI14	资金流量指标	①求得典型价格（当日最高价，最低价和收盘价的均值）
②根据典型价格高低判定正负向资金流（资金流=典型价格*成交量）
③计算MR= 正向/负向 ④MFI=100-100/（1+MR）

price_no_fq	不复权价格	不复权价格

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['boll_down','boll_up'],
start_date='2022-01-01', end_date='2022-01-10')

print(factor_data['boll_up'])
