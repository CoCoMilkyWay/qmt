融资融券信息

历史范围：2010年至今；更新时间：下一个交易日9点之前更新

get_mtss(security_list, start_date=None, end_date=None, fields=None, count=None)

描述

获取一只或者多只基金在一个时间段内的融资融券信息

返回

返回一个 [pandas.DataFrame] 对象，默认的列索引为取得的全部字段. 如果给定了 fields 参数, 则列索引与给定的 fields 对应

参数	名称	注释

security_list	基金代码	一只基金代码或者一个基金代码的 list

start_date	开始日期	与 count 二选一, 不可同时使用； 一个字符串或者 [datetime.datetime]/[datetime.date] 对象, 默认为平台提供的数据的最早日期

end_date	结束日期	一个字符串或者 [datetime.date]/[datetime.datetime] 对象, 默认为 datetime.date.today()

count	数量	与 start_date 二选一，不可同时使用**, 必须大于 0. 表示返回 end_date 之前 count 个交易日的数据, 包含 end_date

fields	内涵不同字段，具体含义如下	字段名或者 list, 可选. 默认为 None, 表示取全部字段,

字段名	含义

date	日期

sec_code	基金代码

fin_value	融资余额(元）

fin_buy_value	融资买入额（元）

fin_refund_value	融资偿还额（元）

sec_value	融券余量（股）

sec_sell_value	融券卖出量（股）

sec_refund_value	融券偿还量（股）

fin_sec_value	融资融券余额（元）

示例

- 获取一支基金多个时间的融资融券信息
df=get_mtss('560800.XSHG','2023-02-07','2023-02-09',fields=['date','sec_code','fin_value','fin_buy_value','fin_refund_value','sec_value','sec_sell_value','sec_refund_value','fin_sec_value'])

print(df[:5])
date     sec_code   fin_value  fin_buy_value  fin_refund_value  \
0 2023-02-07  560800.XSHG  24326759.0      1029193.0          445910.0
1 2023-02-08  560800.XSHG  23113187.0      1512546.0         2726118.0
2 2023-02-09  560800.XSHG  19737582.0      4100908.0         7476513.0

sec_value  sec_sell_value  sec_refund_value  fin_sec_value
0        0.0             0.0               0.0     24326759.0
1        0.0             0.0               0.0     23113187.0
2        0.0             0.0               0.0     19737582.0

获取多支基金多个时间的融资融券信息

fund=get_all_securities("fund",date='2023-02-01').index.tolist()
df=get_mtss(fund,'2023-02-07','2023-02-09',fields=['date','sec_code','fin_value','fin_buy_value','fin_refund_value','sec_value','sec_sell_value','sec_refund_value','fin_sec_value'])
print(df[:5])
​
date     sec_code    fin_value  fin_buy_value  fin_refund_value  \
0 2023-02-07  159605.XSHE  736483739.0     40691302.0        55474067.0
1 2023-02-08  159605.XSHE  739835418.0     53047593.0        49695914.0
2 2023-02-09  159605.XSHE  772567675.0    107469134.0        74736877.0
3 2023-02-07  159825.XSHE   35609132.0      2937920.0         3376536.0
4 2023-02-08  159825.XSHE   35021904.0      1907911.0         2495139.0

sec_value  sec_sell_value  sec_refund_value  fin_sec_value
0        0.0             0.0               0.0    736483739.0
1        0.0             0.0               0.0    739835418.0
2        0.0             0.0               0.0    772567675.0
3  4230000.0        750000.0          750000.0     39399212.0
4  4230000.0        750000.0          750000.0     38782374.0
