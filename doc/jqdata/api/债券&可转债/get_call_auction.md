获取可转债集合竞价

历史范围：2019年至今；更新时间：盘后15点

get_call_auction(security, start_date, end_date, fields=None)

描述

支持可转债的集合竞价，当日的集合竞价数据于盘后15点返回。

为了防止返回数据量过大, 我们每次最多返回5000行。

可转债集合竞价

参数：

security: 可转债

start_date: 开始日期，YYYY-MM-DD格式

end_date: 结束日期，YYYY-MM-DD格式

fields: 选择要获取的行情数据字段，参数为list格式，默认为None，返回全部字段。

返回值：

返回指定时间区间标的集合竞价tick数据，返回字段结果如下：

字段名	说明	字段类型

time	时间	datetime

current	当前价	float

volume	累计成交量（股）	float

money	累计成交额（元）	float

a1_v~a5_v	五档卖量	float

a1_p~a5_p	五档卖价	float

b1_v~b5_v	五档买量	float

b1_p~b5_p	五档买价	float

#获取110043.XSHG可转债2023-08-08至2023-8-09期间的集合竞价数据
df=get_call_auction('110043.XSHG','2023-08-08','2023-08-09')
print(df)

code                time  current  volume    money    a1_p   a1_v  \
0  110043.XSHG 2023-08-08 09:25:02   112.81   120.0  13537.0  112.81  630.0
1  110043.XSHG 2023-08-09 09:25:01   112.20   130.0  14586.0  112.20  270.0

a2_p   a2_v     a3_p  ...       b1_p  b1_v   b2_p  b2_v    b3_p  b3_v  \
0  113.09   10.0  113.188  ...    112.788  20.0  112.6  10.0  112.51  10.0
1  112.27  640.0  112.370  ...    111.988  20.0  111.9  90.0  111.89  10.0

b4_p  b4_v   b5_p   b5_v
0  112.500  10.0  112.4  200.0
1  111.707  80.0  111.6   10.0

[2 rows x 25 columns]
