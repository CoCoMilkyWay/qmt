指定日期的期货列表数据

get_future_contracts(underlying_symbol, date)

描述

获取某期货品种在指定日期下的可交易合约标的列表

参数

underlying_symbol: 期货合约品种，如 'AU'(白银)

date：指定日期，默认为None，不指定时返回当前日期下可交易的合约标的列表

返回结果

某一期货品种在指定日期下的可交易合约标的列表

示例：

# 获取某期货品种在指定日期下的可交易合约标的列表
get_future_contracts('AU','2017-01-05')

# 输出
['AU1701.XSGE', 'AU1702.XSGE', 'AU1703.XSGE', 'AU1704.XSGE', 'AU1706.XSGE', 'AU1708.XSGE', 'AU1710.XSGE', 'AU1712.XSGE']
