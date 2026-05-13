每股类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取每股类因子值

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

total_operating_revenue_per_share_ttm	每股营业总收入TTM	营业总收入（TTM）除以总股本

cash_and_equivalents_per_share	每股现金及现金等价物余额	每股现金及现金等价物余额

surplus_reserve_fund_per_share	每股盈余公积金	每股盈余公积金

retained_profit_per_share	每股未分配利润	每股未分配利润

operating_revenue_per_share_ttm	每股营业收入TTM	营业收入（TTM）除以总股本

net_asset_per_share	每股净资产	(归属母公司所有者权益合计-其他权益工具)除以总股本

total_operating_revenue_per_share	每股营业总收入	每股营业总收入

retained_earnings_per_share	每股留存收益	每股留存收益

operating_revenue_per_share	每股营业收入	每股营业收入

net_operate_cash_flow_per_share	每股经营活动产生的现金流量净额	每股经营活动产生的现金流量净额

operating_profit_per_share_ttm	每股营业利润TTM	营业利润（TTM）除以总股本

eps_ttm	每股收益TTM	过去12个月归属母公司所有者的净利润（TTM）除以总股本

cashflow_per_share_ttm	每股现金流量净额，根据当时日期来获取最近变更日的总股本	现金流量净额（TTM）除以总股本

operating_profit_per_share	每股营业利润	每股营业利润

capital_reserve_fund_per_share	每股资本公积金	每股资本公积金

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['total_operating_revenue_per_share_ttm','cash_and_equivalents_per_share','surplus_reserve_fund_per_share'],
start_date='2022-01-01', end_date='2022-01-10')
# 查看因子值
print(factor_data['cash_and_equivalents_per_share'])
