get_open_call_auction - 查询集合竞价开盘成交
查询基金开盘成交数据

gm SDK 3.0.176 版本新增

函数原型：

get_open_call_auction (symbols, trade_date=None)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	基金代码	Y	无	必填，可输入多个，使用时参考symbol. 采用 str 格式时，多个标的代码必须用英文逗号分割（逗号中间不能有空格），如：'SHSE.510300,SZSE.159922'; 采用 list 格式时，多个标的代码示例：['SHSE.510300', 'SZSE.159922']
trade_date	str	交易日期	N	None	交易日期，YYYY-MM-DD 格式，默认None返回最新交易日
返回值：返回dataframe

字段名	类型	中文名称	说明
symbol	str	标的代码	格式exchange.sec_id（SHSE.600000, SZSE.000001）
time	str	开盘集合竞价撮合时间	交易日09:25，%Y-%m-%d %H:%M:%S 格式
current_price	float	当前最新价（不复权）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的最新成交价Tick.price，即开盘价
open_volume	int	开盘成交量（份）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的累计成交量Tick.cum_volume
open_amount	float	开盘成交额（元）	09:15~09:25开盘前竞价，在09:25一次性集中撮合产生的累计成交额Tick.cum_amount
ask_v1~ask_v5	float	五档卖量	09:25五档卖量Tick.quotes[0].ask_v~Tick.quotes[4].ask_v。
ask_p1~ask_p5	float	五档卖价	09:25五档卖价Tick.quotes[0].ask_p~Tick.quotes[4].ask_p。
bid_v1~bid_v5	float	五档买量	09:25五档买量Tick.quotes[0].bid_v~Tick.quotes[4].bid_v。
bid_p1~bid_p5	float	五档买价	09:25五档买价Tick.quotes[0].bid_p~Tick.quotes[4].bid_p。
示例：

get_open_call_auction(symbols='SHSE.510300, SZSE.159922', trade_date='2025-03-27')
 
        复制成功
    
输出：

    symbol                 time  current_price  open_volume  open_amount  \
0  SHSE.510300  2025-03-27 09:25:00     4.01100016      1693800    6793832.0   
1  SZSE.159922  2025-03-27 09:25:00     2.36899996         3700       8765.3   
   ask_v1  ask_v2   ask_v3  ask_v4  ask_v5      ask_p1      ask_p2  \
0  665982  485400  1037400   11800  350600  4.01100016  4.01200008   
1  105802  835700      400   43000  210300  2.36899996  2.36999989   
       ask_p3      ask_p4      ask_p5  bid_v1  bid_v2  bid_v3  bid_v4  bid_v5  \
0  4.01300001  4.01399994  4.01499987  372200  130800  521400  210400  205100   
1  2.37100005  2.37199998  2.37400007    3000    1700   23900     200   22200   
       bid_p1      bid_p2      bid_p3      bid_p4      bid_p5  
0  4.01000023  4.00899982  4.00799990  4.00699997  4.00600004  
1  2.36800003  2.36599994  2.36500001  2.36400008  2.36299992  
 
        复制成功
    
注意：

1. 开盘集合竞价的成交数据于每个交易日09:26更新，09:26后可查询当天开盘集合竞价，在09:26前查询当天开盘集合竞价返回空。

2. 如果输入symbols包含不存在的标的代码，会报错。

3. 如果开盘集合竞价没有发生成交，curret_price, open_volume, open_amount返回0.

4. 数据最早为2025-02-21。