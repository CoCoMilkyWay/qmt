获取期货结算价

历史范围：2005年至今；更新时间：盘后17:00更新

get_extras(info, security_list, start_date='2005-01-01', end_date =None, df=True, count=None)

描述

获取期货结算价

参数

info: [futures_sett_price]期货结算价;

security_list: 期货列表：

start_date/end_date:开始/结束日期, 同 [get_price]

df: 返回[pandas.DataFrame]对象还是一个dict

count: 数量, 与 start_date 二选一, 不可同时使用,
必须大于 0. 表示取 end_date 往前的 count 个交易日的数据

返回结果

df=True:
返回[pandas.DataFrame]对象, 列索引是期货代号, 行索引是[datetime.datetime]

示例：

获取期货结算价：futures_sett_price

get_extras('futures_sett_price', ['A0603.XDCE','A0605.XDCE'],start_date='2005-01-18').head()

A0603.XDCE  A0605.XDCE
2005-01-18      2510.0      2537.0
2005-01-19      2512.0      2537.0
2005-01-20      2512.0      2529.0
2005-01-21      2500.0      2528.0
