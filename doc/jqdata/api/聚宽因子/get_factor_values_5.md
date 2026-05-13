情绪类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取情绪类因子值

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

VROC12	12日量变动速率指标	成交量减N日前的成交量，再除以N日前的成交量，放大100倍，得到VROC值 ，n=12

TVMA6	6日成交金额的移动平均值	6日成交金额的移动平均值

VEMA10	成交量的10日指数移动平均

VR	成交量比率（Volume Ratio）	VR=（AVS+1/2CVS）/（BVS+1/2CVS）

VOL5	5日平均换手率	5日换手率的均值,单位为%

BR	意愿指标	BR=N日内（当日最高价－昨日收盘价）之和 / N日内（昨日收盘价－当日最低价）之和×100 n设定为26

VEMA12	12日成交量的移动平均值

TVMA20	20日成交金额的移动平均值	20日成交金额的移动平均值

DAVOL5	5日平均换手率与120日平均换手率	5日平均换手率 / 120日平均换手率

VDIFF	计算VMACD因子的中间变量	EMA(VOLUME，SHORT)-EMA(VOLUME，LONG) short设置为12，long设置为26，M设置为9

WVAD	威廉变异离散量	(收盘价－开盘价)/(最高价－最低价)×成交量，再做加和，使用过去6个交易日的数据

MAWVAD	因子WVAD的6日均值

VSTD10	10日成交量标准差	10日成交量标准差

ATR14	14日均幅指标	真实振幅的14日移动平均

VOL10	10日平均换手率	10日换手率的均值,单位为%

DAVOL10	10日平均换手率与120日平均换手率之比	10日平均换手率 / 120日平均换手率

VDEA	计算VMACD因子的中间变量	EMA(VDIFF，M)    short设置为12，long设置为26，M设置为9

VSTD20	20日成交量标准差	20日成交量标准差

ATR6	6日均幅指标	真实振幅的6日移动平均

VOL20	20日平均换手率	20日换手率的均值,单位为%

DAVOL20	20日平均换手率与120日平均换手率之比	20日平均换手率 / 120日平均换手率

VMACD	成交量指数平滑异同移动平均线	快的指数移动平均线（EMA12）减去慢的指数移动平均线（EMA26）得到快线DIFF, 由DIFF的M日移动平均得到DEA，由DIFF-DEA的值得到MACD

AR	人气指标	AR=N日内（当日最高价—当日开市价）之和 / N日内（当日开市价—当日最低价）之和 * 100，n设定为26

VOL60	60日平均换手率	60日换手率的均值,单位为%

turnover_volatility	换手率相对波动率	取20个交易日个股换手率的标准差

VOL120	120日平均换手率	120日换手率的均值,单位为%

VROC6	6日量变动速率指标	成交量减N日前的成交量，再除以N日前的成交量，放大100倍，得到VROC值 ，n=6

TVSTD20	20日成交金额的标准差	20日成交额的标准差

ARBR	ARBR	因子 AR 与因子 BR 的差

money_flow_20	20日资金流量	用收盘价、最高价及最低价的均值乘以当日成交量即可得到该交易日的资金流量

VEMA5	成交量的5日指数移动平均

VOL240	240日平均换手率	240日换手率的均值,单位为%

VEMA26	成交量的26日指数移动平均

VOSC	成交量震荡	'VEMA12'和'VEMA26'两者的差值，再求差值与'VEMA12'的比，最后将比值放大100倍，得到VOSC值

TVSTD6	6日成交金额的标准差	6日成交额的标准差

PSY	心理线指标	12日内上涨的天数/n *100

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['VROC12','TVMA6','VEMA10'],
start_date='2022-01-01', end_date='2022-01-10')

# 查看因子值
print(factor_data['VEMA10'])
