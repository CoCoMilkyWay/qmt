get_history_l2orders - 查询历史 L2 逐笔委托
仅特定券商付费提供 注意： 仅深市标的可用

函数原型：

 get_history_l2orders(symbols, start_time, end_time, fields=None, df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str	标的代码，使用时参考symbol
start_time	str	开始时间 (%Y-%m-%d %H:%M:%S 格式)
end_time	str	结束时间 (%Y-%m-%d %H:%M:%S 格式)
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有
df	bool	是否返回 dataframe 格式, 默认 False
返回值:参考level2 逐笔委托数据

当 df = True 时， 返回dataframe

当 df = Falst， 返回list

示例：

history_order=get_history_l2orders('SZSE.000001', '2020-11-23 14:00:00', '2020-11-23 15:00:00', fields=None, df=False)
print(history_order[0])
 
        复制成功
    
输出：

{'symbol': 'SZSE.000001', 'side': '1', 'price': 19.520000457763672, 'volume': 200, 'created_at': datetime.datetime(2020, 11, 23, 14, 0, 0, 110000, tzinfo=tzfile('PRC')), 'order_type': '2'}
 
        复制成功
    
注意：

1. get_history_l2orders接口每次只能提取一天的数据, 如果取数时间超过一天，则返回按照开始时间的最近有一个交易日数据

