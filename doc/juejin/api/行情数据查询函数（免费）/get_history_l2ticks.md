get_history_l2ticks - 查询历史 L2 Tick 行情
仅特定券商付费提供

函数原型：

 get_history_l2ticks(symbols, start_time, end_time, fields=None,skip_suspended=True, fill_missing=None,adjust=ADJUST_NONE, adjust_end_time='', df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str	标的代码，使用时参考symbol
start_time	str	开始时间 (%Y-%m-%d %H:%M:%S 格式)
end_time	str	结束时间 (%Y-%m-%d %H:%M:%S 格式)
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有
skip_suspended	bool	是否跳过停牌, 默认跳过
fill_missing	str or None	填充方式, None - 不填充, 'NaN' - 用空值填充, 'Last' - 用上一个值填充, 默认 None
adjust	int	ADJUST_NONE or 0: 不复权, ADJUST_PREV or 1: 前复权, ADJUST_POST or 2: 后复权 默认不复权
adjust_end_time	str	复权基点时间, 默认当前时间
df	bool	是否返回 dataframe 格式, 默认 False
返回值:参考tick 对象

当 df = True 时， 返回dataframe

当 df = Falst， 返回list 示例：

history_l2tick=get_history_l2ticks('SHSE.600519', '2020-11-23 14:00:00', '2020-11-23 15:00:00', fields=None,
                        skip_suspended=True, fill_missing=None,
                        adjust=ADJUST_NONE, adjust_end_time='', df=False)
print(history_l2tick[0])
 
        复制成功
    
输出：

{'symbol': 'SHSE.600519', 'open': 1771.010009765625, 'high': 1809.9000244140625, 'low': 1771.010009765625, 'price': 1791.0999755859375, 'quotes': [{'bid_p': 1790.8800048828125, 'bid_v': 100, 'ask_p': 1794.760009765625, 'ask_v': 200}, {'bid_p': 1790.80004882812
5, 'bid_v': 123, 'ask_p': 1794.800048828125, 'ask_v': 100}, {'bid_p': 1790.699951171875, 'bid_v': 100, 'ask_p': 1794.8800048828125, 'ask_v': 416}, {'bid_p': 1790.68994140625, 'bid_v': 200, 'ask_p': 1794.8900146484375, 'ask_v': 300}, {'bid_p': 1790.630004882812
5, 'bid_v': 300, 'ask_p': 1794.9000244140625, 'ask_v': 1000}, {'bid_p': 1790.6199951171875, 'bid_v': 500, 'ask_p': 1794.949951171875, 'ask_v': 300}, {'bid_p': 1790.6099853515625, 'bid_v': 300, 'ask_p': 1794.9599609375, 'ask_v': 300}, {'bid_p': 1790.59997558593
75, 'bid_v': 200, 'ask_p': 1794.97998046875, 'ask_v': 100}, {'bid_p': 1790.510009765625, 'bid_v': 314, 'ask_p': 1794.989990234375, 'ask_v': 200}, {'bid_p': 1790.5, 'bid_v': 4200, 'ask_p': 1795.0, 'ask_v': 9700}], 'cum_volume': 5866796, 'cum_amount': 1049949547
1.0, 'last_amount': 1973854.0, 'last_volume': 1100, 'created_at': datetime.datetime(2020, 11, 23, 14, 0, 2, tzinfo=tzfile('PRC')), 'cum_position': 0, 'trade_type': 0}
 
        复制成功
    
注意：

1. get_history_l2ticks接口每次只能提取一天的数据, 如果取数时间超过一天，则返回按照结束时间的最近有一个交易日数据， 如果取数时间段超过 1 个自然月（31）天，则获取不到数据

