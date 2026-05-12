get_history_l2transactions - 查询历史 L2 逐笔成交
仅特定券商付费提供

函数原型：

 get_history_l2transactions(symbols, start_time, end_time, fields=None, df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str	标的代码，使用时参考symbol
start_time	str	开始时间 (%Y-%m-%d %H:%M:%S 格式)
end_time	str	结束时间 (%Y-%m-%d %H:%M:%S 格式)
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有
df	bool	是否返回 dataframe 格式, 默认 False
返回值:参考level2 逐笔成交数据

当 df = True 时， 返回dataframe

当 df = Falst， 返回list

示例：

history_transactions=get_history_l2transactions('SHSE.600000', '2020-11-23 14:00:00', '2020-11-23 15:00:00', fields=None, df=False)
print(history_transactions[0])
 
        复制成功
    
输出：

{'symbol': 'SHSE.600000', 'side': 'B', 'price': 9.90999984741211, 'volume': 100, 'created_at': datetime.datetime(2020, 11, 23, 14, 0, 0, 820000, tzinfo=tzfile('PRC')), 'exec_type': ''}
 
        复制成功
    
注意：

1. get_history_l2transactions接口每次只能提取一天的数据, 如果取数时间超过一天，则返回按照开始时间的最近有一个交易日数据

