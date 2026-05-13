获取指数集合竞价

历史范围：2017年至今；

get_call_auction(security, start_date, end_date, fields=None)

描述

支持指数（2017年至今）的集合竞价，当日的集合竞价数据于盘后15点返回。

为了防止返回数据量过大, 我们每次最多返回5000行。

指数集合竞价信息

参数：

security: 指数（20117年至今）

start_date: 开始日期，YYYY-MM-DD格式

end_date: 结束日期，YYYY-MM-DD格式

fields: 选择要获取的行情数据字段，参数为list格式，默认为None，返回全部字段。

返回值：

返回指定时间区间标的集合竞价tick数据，返回字段结果如下：

字段名	说明	字段类型

time	时间	datetime

current	当前价（不复权）	float

volume	累计成交量（股）	float

money	累计成交额（元）	float

a1_v~a5_v	五档卖量	float

a1_p~a5_p	五档卖价	float

b1_v~b5_v	五档买量	float

b1_p~b5_p	五档买价	float

#获取沪深300指数2017-09-01至2017-09-05期间的集合竞价数据
df=get_call_auction('000300.XSHG','2017-09-01','2017-09-05')
print(df)

code                time  current       volume         money  a1_p  \
0  000300.XSHG 2017-09-01 09:25:12  3825.34   60406600.0  6.963988e+08  None
1  000300.XSHG 2017-09-04 09:25:10  3828.54  145635600.0  1.222139e+09  None
2  000300.XSHG 2017-09-05 09:25:13  3845.55   59488700.0  6.133801e+08  None

a1_v  a2_p  a2_v  a3_p  ...   b1_p  b1_v  b2_p  b2_v  b3_p  b3_v  b4_p  \
0  None  None  None  None  ...   None  None  None  None  None  None  None
1  None  None  None  None  ...   None  None  None  None  None  None  None
2  None  None  None  None  ...   None  None  None  None  None  None  None

b4_v  b5_p  b5_v
0  None  None  None
1  None  None  None
2  None  None  None

[3 rows x 25 columns]
