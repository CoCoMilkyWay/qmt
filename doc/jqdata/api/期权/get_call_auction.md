获取上证etf集合竞价

历史范围：上市至今

get_call_auction(security, start_date, end_date, fields=None)

描述

支持上证etf期权（2017年至今）的集合竞价，当日的集合竞价数据于盘后15点返回。

为了防止返回数据量过大, 我们每次最多返回10000行。

上证ETF金融期权集合竞价字段

**参数**：

security: 上证etf期权（2017年至今）；深交所ETF期权2022-11-28至今

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

#获取50ETF期权2019-01-02至2019-01-20期间的集合竞价数据
df=get_call_auction('10004404.XSHG','2022-07-18','2022-07-19')
print(df)
code                time  current  volume  money    a1_p  a1_v  \
0  10004404.XSHG 2022-07-18 09:25:00   0.0124     5.0  620.0  0.0124   6.0
1  10004404.XSHG 2022-07-19 09:25:00      NaN     0.0    NaN     NaN   NaN

a2_p  a2_v    a3_p  ...    b1_p  b1_v    b2_p  b2_v    b3_p  b3_v  \
0  0.0126   5.0  0.0127  ...  0.0121   6.0  0.0115   3.0  0.0114   1.0
1     NaN   NaN     NaN  ...     NaN   NaN     NaN   NaN     NaN   NaN

b4_p  b4_v    b5_p  b5_v
0  0.0107   2.0  0.0091   1.0
1     NaN   NaN     NaN   NaN

[2 rows x 25 columns]
