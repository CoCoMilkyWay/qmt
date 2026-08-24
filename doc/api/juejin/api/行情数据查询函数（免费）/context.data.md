context.data - 查询订阅数据
函数原型：

context.data(symbol, frequency, count, fields)
 
        复制成功
    
参数：

参数名	类型	说明
symbol	str	标的代码(只允许单个标的的代码字符串)，使用时参考symbol
frequency	str	频率, 支持 'tick', '1d', '60s' 等, 默认 '1d', 详情见股票行情数据和期货行情数据, 实时行情支持的频率
count	int	滑窗大小(正整数)，需小于等于 subscribe 函数中 count 值
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有, 具体字段见:tick 对象 和 bar 对象 ，需在 subscribe 函数中指定的fields范围内，指定字段越少，查询速度越快
返回值：

当subscribe的format="df"（默认）时，返回dataframe

类型	说明
dataframe	tick 的 dataframe 或者 bar 的 dataframe
示例：

def init(context):
    subscribe(symbols='SHSE.600519', frequency='60s', count=50, fields='symbol, close, eob', format='df')

def on_bar(context,bars):
    data = context.data(symbol=bars[0]['symbol'], frequency='60s', count=10)
    print(data.tail())
 
        复制成功
    
输出：

                symbol    close                       eob
5  SHSE.600519  1629.96 2024-01-22 14:56:00+08:00
6  SHSE.600519  1627.25 2024-01-22 14:57:00+08:00
7  SHSE.600519  1627.98 2024-01-22 14:58:00+08:00
8  SHSE.600519  1642.00 2024-01-22 15:00:00+08:00
9  SHSE.600519  1632.96 2024-01-23 09:31:00+08:00
 
        复制成功
    
subscribe的format ="row"时，返回list[dict]

类型	说明
list[dict]	当frequency='tick'时，返回tick列表：[{tick_1}, {tick_2}, ..., {tick_n}]，列表长度等于滑窗大小，即n=count， 当frequency='60s', '300s', '900s', '1800s', '3600s'时，返回bar列表：[{bar_1}, {bar_2}, {bar_n}, ..., ] ，列表长度等于滑窗大小，即n=count
示例：

def init(context):
    subscribe(symbols='SHSE.600519', frequency='tick', count=50, fields='symbol, price, quotes,created_at', format='row')


def on_tick(context, tick):
    data = context.data(symbol=tick['symbol'], frequency='tick', count=3)
    print(data)
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600519', 'price': 1642.0, 'quotes': [{'bid_p': 1640.0, 'bid_v': 100, 'ask_p': 1642.0, 'ask_v': 4168}, {'bid_p': 1634.52, 'bid_v': 300, 'ask_p': 1642.01, 'ask_v': 100}, {'bid_p': 1633.0, 'bid_v': 100, 'ask_p': 1642.06, 'ask_v': 100}, {'bid_p': 1632.96, 'bid_v': 100, 'ask_p': 1642.08, 'ask_v': 200}, {'bid_p': 1632.89, 'bid_v': 100, 'ask_p': 1642.2, 'ask_v': 200}], 'created_at': datetime.datetime(2024, 1, 22, 15, 1, 51, 286000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai'))}, {'symbol': 'SHSE.600519', 'price': 1642.0, 'quotes': [{'bid_p': 1640.0, 'bid_v': 100, 'ask_p': 1642.0, 'ask_v': 4168}, {'bid_p': 1634.52, 'bid_v': 300, 'ask_p': 1642.01, 'ask_v': 100}, {'bid_p': 1633.0, 'bid_v': 100, 'ask_p': 1642.06, 'ask_v': 100}, {'bid_p': 1632.96, 'bid_v': 100, 'ask_p': 1642.08, 'ask_v': 200}, {'bid_p': 1632.89, 'bid_v': 100, 'ask_p': 1642.2, 'ask_v': 200}], 'created_at': datetime.datetime(2024, 1, 22, 15, 1, 54, 280000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai'))}, {'symbol': 'SHSE.600519', 'price': 0.0, 'quotes': [{'bid_p': 0.0, 'bid_v': 0, 'ask_p': 0.0, 'ask_v': 0}, {'bid_p': 0.0, 'bid_v': 0, 'ask_p': 0.0, 'ask_v': 0}, {'bid_p': 0.0, 'bid_v': 0, 'ask_p': 0.0, 'ask_v': 0}, {'bid_p': 0.0, 'bid_v': 0, 'ask_p': 0.0, 'ask_v': 0}, {'bid_p': 0.0, 'bid_v': 0, 'ask_p': 0.0, 'ask_v': 0}], 'created_at': datetime.datetime(2024, 1, 23, 9, 14, 1, 91000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai'))}]
 
        复制成功
    
subscribe的format ="col"时，返回dict

类型	说明
dict	当frequency='tick'时，返回tick数据（symbol为str格式，其余字段为列表，列表长度等于滑窗大小count），当frequency='60s', '300s', '900s', '1800s', '3600s'时，返回bar数据（symbol和frequency为str格式，其余字段为列表，列表长度等于滑窗大小count）
示例：

def init(context):
    subscribe(symbols='SHSE.600519', frequency='tick', count=10, fields='symbol, price, bid_p, created_at', format='col')


def on_tick(context, tick):
    data = context.data(symbol=tick['symbol'], frequency='tick', count=10)
    print(data)
 
        复制成功
    
输出：

{'symbol': 'SHSE.600519', 'price': [1642.0, 1642.0, 1642.0, 1642.0, 1642.0, 1642.0, 1642.0, 1642.0, 1642.0, 0.0], 'bid_p': [1640.0, 1640.0, 1640.0, 1640.0, 1640.0, 1640.0, 1640.0, 1640.0, 1640.0, 0.0], 'created_at': [datetime.datetime(2024, 1, 22, 15, 1, 12, 280000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 21, 277000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 24, 278000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 33, 280000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 36, 282000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 39, 279000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 48, 283000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 51, 286000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 22, 15, 1, 54, 280000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai')), datetime.datetime(2024, 1, 23, 9, 14, 1, 91000, tzinfo=datetime.timezone(datetime.timedelta(seconds=28800), 'Asia/Shanghai'))]}
 
        复制成功
    
注意：

1. 只有在订阅后，此接口才能取到数据，如未订阅数据，则返回报错。

2. symbol 参数只支持输入一个标的。

3. count 参数必须小于或等于订阅函数里面的 count 值。

4. fields 参数必须在订阅函数subscribe里面指定的 fields 范围内。指定字段越少，查询速度越快，目前效率是row > col > df。

5. 当subscribe的format指定col时，tick的quotes字段会被拆分，只返回买卖一档的量和价，即只有bid_p，bid_v, ask_p和ask_v。

