last_tick - 查询已订阅的最新 Tick （多标的）
查询已订阅的最新 Tick

函数原型：

last_tick(symbols, fields="", include_call_auction = False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str or list	查询代码，如有多个代码, 中间用 , (英文逗号) 隔开, 也支持 ['symbol1', 'symbol2'] 这种列表格式 ，使用参考symbol
fields	str	查询字段, 默认所有字段 具体字段见:tick 对象
include_call_auction	bool	是否支持集合竞价(9:15-9:25)取数，True为支持，False为不支持，默认为False
返回值：

list[dict]

示例：

def init(context):
    context.symbol_list = ['SZSE.000002', 'SHSE.600000']
    context.index_symbol = 'SHSE.000001'
    subscribe(symbols=context.symbol_list + [context.index_symbol], frequency='tick')
    schedule(schedule_func=algo, date_rule='1d', time_rule='14:50:00')


def on_tick(context, tick):
    symbol = tick['symbol']
    if symbol == context.index_symbol:
        # 每次收到指数行情，获取订阅股票的最新tick，避免每个股票的tick都触发，多次查询占用资源
        data = last_tick(symbols=context.symbol_list, fields='symbol,price,open,created_at')
        print(data)


# 或者定时任务里调用
def algo(context):
    data = last_tick(symbols=context.symbol_list, fields='symbol,price,open,created_at')
    print(data)


 
        复制成功
    
输出：

[{'symbol': 'SZSE.000001', 'price': 16.559999465942383, 'created_at': datetime.datetime(2020, 10, 15, 15, 0, 3, tzinfo=tzfile('PRC'))}]
 
        复制成功
    
注意：

1. 输入的 symbols 必须先订阅 tick，如果 last_tick 查询的标的代码不在 tick 行情订阅范围内，则返该代码的 tick 字典，除 symbol 外其他字段均为 空字符串/0

2. 若输入代码有效，在 tick 行情订阅范围内，但查询字段 fields 中包括错误字段，返回的列表仍包含对应数量的dict，但每个dict中除有效字段外，其他字段的值均为空字符串/0

3. 实时模式获取集合竞价的tick数据，需要指定include_call_auction=True，注意集合竞价阶段没有成交，有效字段只有报价quotes。

4. 回测模式，先订阅标的行情 frequency='tick' 再调用 last_tick ，会返回回测当前时刻最新的 tick.price，如果超出历史行情权限会报错中止回测。

