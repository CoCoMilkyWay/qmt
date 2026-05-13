获取股票当日盘前交易信息

历史范围：当天；更新时间：盘前交易日 09:15 后可获取

get_preopen_infos(security, fields=("paused", "factor", "high_limit", "low_limit"))

描述

获取股票当日盘前交易信息, 盘前交易日 09:15 后可获取到数据

返回

DataFrame : index 为标的列表, columns 为字段名

注意

在盘前更新中，聚宽单季度财务估值表于08:30更新当日最新的总股本与流通股本数据。

参数

参数	备注

security	股票代码或者股票代码的 list

fields	请求字段, 默认为全部: paused(停牌标志), factor(后复权因子), high_limit(涨停价), low_limit(跌停价)

代码示例

# 获取单季度某张表中的字段信息
stocks = get_index_stocks('000300.XSHG')
get_preopen_infos(stocks, fields=("paused", "factor", "high_limit", "low_limit"))

paused      factor  high_limit  low_limit
000001.XSHE     0.0  135.995625       11.01       9.01
000002.XSHE     0.0  168.875961        7.57       6.19
000063.XSHE     0.0   18.066363       28.11      23.00
000100.XSHE     0.0    3.723823        4.04       3.30
000157.XSHE     0.0   87.370424        7.12       5.82
