get_history_l2orders_queue - 查询历史 L2 委托队列
仅特定券商付费提供

函数原型：

 get_history_l2orders_queue(symbols, start_time, end_time, fields=None, df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str	标的代码，使用时参考symbol
start_time	str	开始时间 (%Y-%m-%d %H:%M:%S 格式)
end_time	str	结束时间 (%Y-%m-%d %H:%M:%S 格式)
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有
df	bool	是否返回 dataframe 格式, 默认 False
返回值:参考 level2 委托队列据

当 df = True 时， 返回dataframe

当 df = Falst， 返回list

示例：

history_order_queue=get_history_l2orders_queue('SHSE.600000', '2020-11-23 14:00:00', '2020-11-23 15:00:00', fields=None, df=False)
print(history_order_queue[0])
 
        复制成功
    
输出：

{'symbol': 'SHSE.600000', 'price': 9.90999984741211, 'total_orders': 155, 'queue_orders': 50, 'queue_volumes': [52452, 600, 1200, 3200, 10000, 1800, 1000, 300, 10000, 2000, 500, 500, 2000, 1000, 2000, 300, 1200, 1400, 1000, 200, 1000, 100, 500, 1000, 500, 2380
0, 25400, 1000, 2000, 200, 500, 1200, 5000, 2000, 17600, 5000, 1000, 1300, 1000, 1200, 1000, 3000, 1000, 1000, 15000, 400, 15000, 5000, 2000, 10000], 'created_at': datetime.datetime(2020, 11, 23, 14, 0, 1, tzinfo=tzfile('PRC')), 'side': '', 'volume': 0}
 
        复制成功
    
注意：

1. get_history_l2orders_queue接口每次只能提取一天的数据, 如果取数时间超过一天，则返回按照开始时间的最近有一个交易日数据