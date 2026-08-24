get_history_l2bars - 查询历史 L2 Bar 行情
仅特定券商付费提供

函数原型：

 get_history_l2bars(symbols, frequency, start_time, end_time, fields=None,skip_suspended=True, fill_missing=None,adjust=ADJUST_NONE, adjust_end_time='', df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbols	str	标的代码，使用时参考symbol
frequency	str	频率, 支持 '1d', '60s'等
start_time	str	开始时间 (%Y-%m-%d %H:%M:%S 格式)
end_time	str	结束时间 (%Y-%m-%d %H:%M:%S 格式)
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有
skip_suspended	bool	是否跳过停牌, 默认跳过
fill_missing	str or None	填充方式, None - 不填充, 'NaN' - 用空值填充, 'Last' - 用上一个值填充, 默认 None
adjust	int	ADJUST_NONE or 0: 不复权, ADJUST_PREV or 1: 前复权, ADJUST_POST or 2: 后复权 默认不复权
adjust_end_time	str	复权基点时间, 默认当前时间
df	bool	是否返回 dataframe 格式, 默认 False
返回值:参考bar 对象。

当 df = True 时， 返回dataframe

当 df = Falst， 返回list

示例：

history_l2bar=get_history_l2bars('SHSE.600000', '60s', '2020-11-23 14:00:00', '2020-11-23 15:00:00', fields=None,
								skip_suspended=True, fill_missing=None,
								adjust=ADJUST_NONE, adjust_end_time='', df=False)
print(history_l2bar[0])

 
        复制成功
    
输出：

{'symbol': 'SHSE.600000', 'frequency': '60s', 'open': 9.90999984741211, 'high': 9.90999984741211, 'low': 9.890000343322754, 'close': 9.899999618530273, 'volume': 1270526, 'amount': 12574276.0, 'bob': datetime.datetime(2020, 11, 23, 14, 0, tzinfo=tzfile('PRC'))
, 'eob': datetime.datetime(2020, 11, 23, 14, 1, tzinfo=tzfile('PRC')), 'position': 0, 'pre_close': 0.0}
 
        复制成功
    
注意：

1. get_history_l2bars接口每次最多可提取 1 个自然月（31）天的数据如：2015.1.1-2015.1.31 错误设置：（2015.1.1-2015.2.1）超出 31 天则获取不到任何数据

