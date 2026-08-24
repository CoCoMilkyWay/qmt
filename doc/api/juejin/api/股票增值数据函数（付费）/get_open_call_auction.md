get_open_call_auction - 查询集合竞价开盘成交
查询股票开盘成交数据

gm SDK 3.0.176 版本新增

函数原型：

get_open_call_auction (symbols, trade_date=None)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol. 采用 str 格式时，多个标的代码必须用英文逗号分割（逗号中间不能有空格），如：'SHSE.600008,SZSE.000002'; 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
trade_date	str	交易日期	N	None	交易日期，YYYY-MM-DD 格式，默认None返回最新交易日
返回值：返回dataframe

字段名	类型	中文名称	说明
symbol	str	标的代码	格式exchange.sec_id（SHSE.600000, SZSE.000001）
time	str	开盘集合竞价撮合时间	交易日09:25，%Y-%m-%d %H:%M:%S 格式
current_price	float	当前最新价（不复权）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的最新成交价Tick.price，即开盘价
open_volume	int	开盘成交量（股）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的累计成交量Tick.cum_volume
open_amount	float	开盘成交额（元）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的累计成交额Tick.cum_amount
ask_v1~ask_v5	float	五档卖量	09:25五档卖量Tick.quotes[0].ask_v~Tick.quotes[4].ask_v。
ask_p1~ask_p5	float	五档卖价	09:25五档卖价Tick.quotes[0].ask_p~Tick.quotes[4].ask_p。
bid_v1~bid_v5	float	五档买量	09:25五档买量Tick.quotes[0].bid_v~Tick.quotes[4].bid_v。
bid_p1~bid_p5	float	五档买价	09:25五档买价Tick.quotes[0].bid_p~Tick.quotes[4].bid_p。
示例：

get_open_call_auction(symbols='SHSE.600000, SZSE.000001', trade_date='2025-03-27')
 
        复制成功
    
输出：

        symbol                 time  current_price  open_volume  open_amount  \
0  SHSE.600000  2025-03-27 09:25:00    10.50000000       122600    1287300.0   
1  SZSE.000001  2025-03-27 09:25:00    11.36999989       296000    3365520.0   
   ask_v1  ask_v2  ask_v3  ask_v4  ask_v5       ask_p1       ask_p2  \
0     980   15800   21500   16000    6000  10.50000000  10.51000023   
1   91100  580821  951900  462900  764400  11.38000011  11.39000034   
        ask_p3       ask_p4       ask_p5  bid_v1  bid_v2  bid_v3  bid_v4  \
0  10.52000046  10.52999973  10.53999996    1400     900   15400     700   
1  11.39999962  11.40999985  11.42000008  198300  320700  645500  451000   
   bid_v5       bid_p1       bid_p2       bid_p3       bid_p4       bid_p5  
0   27700  10.48999977  10.47999954  10.47000027  10.46000004  10.44999981  
1  391400  11.36999989  11.35999966  11.35000038  11.34000015  11.32999992 
 
        复制成功
    
注意：

1. 开盘集合竞价的成交数据于每个交易日09:26更新，09:26后可查询当天开盘集合竞价，在09:26前查询当天开盘集合竞价返回空。

2. 如果输入symbols包含不存在的标的代码，会报错。

3. 如果开盘集合竞价没有发生成交，curret_price, open_volume, open_amount返回0.

4. 数据最早为2025-02-21。