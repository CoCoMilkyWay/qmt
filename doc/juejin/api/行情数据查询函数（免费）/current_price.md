current_price - 查询当前最新价
查询指定标的当前时点最新价

函数原型：

current_price(symbols)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str or list	查询代码，如有多个代码, 中间用 , (英文逗号) 隔开, 也支持 ['symbol1', 'symbol2'] 这种列表格式 ，使用参考symbol
返回值：

list[dict]

字段名	类型	中文名称	说明
symbol	str	标的代码	格式exchange.sec_id（SHSE.600000, SZSE.000001）
price	float	最新价	实时模式：当前时点最新tick.price。回测模式：若在subscribe的订阅频度frequency='tick', 返回回测当前时点最新tick.price;若在subscribe的订阅频度frequency='60s', 返回回测当前时点最近1分钟bar.close;若在subscribe的订阅频度frequency='1d', 返回回测当前时点最近日线bar.close.
created_at	datetime.datetime	创建时间	实时模式：当前时点最新tick.created_at。回测模式：若在subscribe的订阅频度frequency='tick', 返回回测当前时点最新tick.created_at;若在subscribe的订阅频度frequency='60s', 返回回测当前时点最近1分钟bar.eob; 若在subscribe的订阅频度frequency='1d', 返回回测当前时点最近日线bar.eob.
示例：

current_data = current_price(symbols='SZSE.000001')
 
        复制成功
    
输出：

[{'symbol': 'SZSE.000001', 'price': 16.559999465942383, 'created_at': datetime.datetime(2020, 10, 15, 15, 0, 3, tzinfo=tzfile('PRC'))}]
 
        复制成功
    
注意：

1. 若输入包含无效标的代码，则返回的列表只包含有效标的代码对应的dict

2. 回测模式，如果订阅标的行情 frequency='tick' 或 frequency='60s' 再调用 current_price ，会返回回测当前时刻最新的 tick.price（订阅 tick）或 bar.close（订阅分钟bar），超出历史行情权限会报错中止回测。

3. 回测模式，如果订阅标的行情日线 frequency='1d' 或 不订阅行情，直接调用 current_price ，会根据 run() 函数指定的 backtest_intraday 参数返回：（python 版本 >= 3.0.178起）

backtest_intraday=0，返回回测当前时刻的的历史最新日线收盘价（T日盘中为T-1日收盘价，T日盘后为T日收盘价）；
backtest_intraday=1，返回回测当前交易日的日线收盘价（T日盘中和盘后均为T日收盘价）
