风险类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取风险类因子值

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

Variance20	20日年化收益方差	20日年化收益方差

Skewness20	个股收益的20日偏度	个股收益的20日偏度

Kurtosis20	个股收益的20日峰度	个股收益的20日峰度

sharpe_ratio_20	20日夏普比率	（Rp - Rf）/Sigma p 其中，Rp是个股的年化收益率，Rf是无风险利率（在这里设置为0.04），Sigma p是个股的收益波动率（标准差）

Variance60	60日年化收益方差	60日年化收益方差

Skewness60	个股收益的60日偏度	个股收益的60日偏度

Kurtosis60	个股收益的60日峰度	个股收益的60日峰度

sharpe_ratio_60	60日夏普比率	（Rp - Rf）/Sigma p 其中，Rp是个股的年化收益率，Rf是无风险利率（在这里设置为0.04），Sigma p是个股的收益波动率（标准差）

Variance120	120日年化收益方差	120日年化收益方差

Skewness120	个股收益的120日偏度	个股收益的120日偏度

Kurtosis120	个股收益的120日峰度	个股收益的120日峰度

sharpe_ratio_120	120日夏普比率	（Rp - Rf）/Sigma p 其中，Rp是个股的年化收益率，Rf是无风险利率（在这里设置为0.04），Sigma p是个股的收益波动率（标准差）

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['Variance20','Skewness20','Kurtosis20'],start_date='2022-01-01', end_date='2022-01-10')

# 查看因子值
print(factor_data['Kurtosis20'])
