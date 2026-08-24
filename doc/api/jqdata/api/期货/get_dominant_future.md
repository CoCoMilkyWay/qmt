期货主力合约

历史范围：2005年至今；更新时间：19:00更新下一交易日

get_dominant_future(underlying_symbol,date , end_date )

描述

获取主力合约对应的标的

参数

underlying_symbol: 期货合约品种，如 'AG'(白银)

date:指定日期参数，获取历史上该日期的主力期货合约 ,如未指定end_date时返回date当天的主力合约(字符串格式), 默认为当前时间; 该参数可指定分时秒，因期货夜盘的存在，date参数的分时秒指定为19:00:00(含)之后返回为下一交易日的主力

end_date:可选参数，如指定则返回date当天(忽略分时秒)到end_date这一个时间区间的主力合约序列(Series)

示例：

# 获取某一天的主力合约对应的期货合约代码，指定日期为'2018-05-06'
get_dominant_future('AU','2018-05-06')
>>> 'AU1812.XSGE'
