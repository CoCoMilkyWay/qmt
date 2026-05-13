获取指定范围交易日

get_trade_days(start_date=None, end_date=None, count=None)

描述

获取指定日期范围内的所有交易日, 返回 [numpy.ndarray], 包含指定的 start_date 和 end_date, 默认返回至 datatime.date.today() 的所有交易日

参数

字段	名称	备注	类型

start_date	开始日期	str/[datetime.date]/[datetime.datetime] 对象

end_date	结束日期	默认为 datetime.date.today()	str/[datetime.date]/[datetime.datetime] 对象

count	交易日数量	必须大于 0. 当指定了count时，start_date与end_date 只能二选一。

表示取end_date(含)往前的 count 个交易日，或start_date(含)往后count个交易日

代码示例

#获取“2018-02-10”至”2018-03-01“的交易日
get_trade_days(start_date="2018-02-10",end_date="2018-03-01")

# 输出为数组
array([datetime.date(2018, 2, 12), datetime.date(2018, 2, 13),
datetime.date(2018, 2, 14), datetime.date(2018, 2, 22),
datetime.date(2018, 2, 23), datetime.date(2018, 2, 26),
datetime.date(2018, 2, 27), datetime.date(2018, 2, 28),
datetime.date(2018, 3, 1)], dtype=object)

#获取“2018-02-10”至”2018-03-01“的交易日
get_trade_days(start_date="2018-03-10",count=10)

# 输出为数组
array([datetime.date(2018, 3, 12), datetime.date(2018, 3, 13),
datetime.date(2018, 3, 14), datetime.date(2018, 3, 15),
datetime.date(2018, 3, 16), datetime.date(2018, 3, 19),
datetime.date(2018, 3, 20), datetime.date(2018, 3, 21),
datetime.date(2018, 3, 22), datetime.date(2018, 3, 23)],
dtype=object)

获取所有交易日

get_all_trade_days()

描述

获取所有交易日, 不需要传入参数, 返回一个包含所有交易日的 [numpy.ndarray], 每个元素为一个 [datetime.date] 类型.

get_all_trade_days()

# 输出
array([datetime.date(2005, 1, 4), datetime.date(2005, 1, 5),
datetime.date(2005, 1, 6), ..., datetime.date(2023, 3, 7),
datetime.date(2023, 3, 8), datetime.date(2023, 3, 9)], dtype=object)
