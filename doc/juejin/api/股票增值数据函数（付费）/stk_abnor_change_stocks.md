stk_abnor_change_stocks - 查询龙虎榜股票数据
查询指定时间段龙虎榜股票数据

gm SDK 3.0.163 版本新增

函数原型：

stk_abnor_change_stocks(symbols=None, change_types=None, trade_date=None, fields=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	N	None	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002'; 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002'], 默认None表示所有标的。
change_types	str or list	异动类型	N	None	输入异动类型，可输入多个. 采用 str 格式时，多个异动类型必须用英文逗号分割，如：'106,107'; 采用 list 格式时，多个异动类型示例：['106','107']； 默认None表示所有异动类型。龙虎榜异动类型列表
trade_date	str or datetime.date	交易日期	N	None	交易日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
fields	str	返回字段	N	None	指定需要返回的字段，如有多个字段，中间用英文逗号分隔，默认 None 返回所有字段。
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict],列表每项的dict的key值为参数指定的 fields 。
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
trade_date	str	交易日期	
change_type	str	异动类型	交易所披露的公开信息及异常波动信息的原因类型,龙虎榜异动类型列表
change_type_name	str	异动类型说明	交易所披露的公开信息及异常波动信息的原因的中文说明
abnor_start_date	str	异动开始日期	股票异动开始的日期，仅部分异动类型才有
abnor_end_date	str	异动结束日期	股票异动结束的日期，仅部分异动类型才有
close	float	收盘价	股票的日频收盘价
cum_volume	int	累计成交量	股票的累计成交量，当存在具体异动开始/结束日期时，累计成交量为区间成交量；当不存在具体异动开始/结束日期时，累计成交量为当日成交量
cum_amount	float	累计成交额	股票的累计成交额，当存在具体异动开始/结束日期时，累计成交额为区间成交额；当不存在具体异动开始/结束日期时，累计成交额为当日成交额
prc_change_rate	float	涨跌幅%	当日涨跌幅
avg_turn_rate	float	日均换手率比值	异动类型中，触发相应异动事件的日均换手率比值
stat_value	float	统计值	异动类型中，触发相应异动事件的统计值
示例：

stk_abnor_change_stocks(symbols=None, change_types='106', trade_date=None, fields=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SZSE.000017', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '日涨幅偏离值达到7%的前5只证券', 'close': 12.77, 'cum_volume': 110996688, 'cum_amount': 1375429201.0, 'prc_change_rate': 9.9914, 'stat_value': 9.05},
 {'symbol': 'SZSE.001217', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '日涨幅偏离值达到7%的前5只证券', 'close': 16.68, 'cum_volume': 54707801, 'cum_amount': 798100880.0, 'prc_change_rate': 10.0264, 'stat_value': 9.05},
 {'symbol': 'SZSE.002230', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '日涨幅偏离值达到7%的前5只证券', 'close': 42.11, 'cum_volume': 105142255, 'cum_amount': 4299831646.0, 'prc_change_rate': 10.0052, 'stat_value': 9.05},
 {'symbol': 'SZSE.002517', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '日涨幅偏离值达到7%的前5只证券', 'close': 11.2, 'cum_volume': 134710030, 'cum_amount': 1488908194.0, 'prc_change_rate': 10.0196, 'stat_value': 9.05},
 {'symbol': 'SZSE.003027', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '日涨幅偏离值达到7%的前5只证券', 'close': 28.84, 'cum_volume': 26272266, 'cum_amount': 727342648.0, 'prc_change_rate': 9.9924, 'stat_value': 9.05},
 {'symbol': 'SHSE.600200', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '有价格涨跌幅限制的日收盘价格涨幅偏离值达到7%的前五只证券', 'close': 8.97, 'cum_volume': 5877996, 'cum_amount': 52725624.0, 'prc_change_rate': 10.0613, 'stat_value': 9.53},
 {'symbol': 'SHSE.600629', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '有价格涨跌幅限制的日收盘价格涨幅偏离值达到7%的前五只证券', 'close': 4.93, 'cum_volume': 25619361, 'cum_amount': 121095785.0, 'prc_change_rate': 10.0446, 'stat_value': 9.52},
 {'symbol': 'SHSE.600675', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '有价格涨跌幅限制的日收盘价格涨幅偏离值达到7%的前五只证券', 'close': 3.16, 'cum_volume': 37060390, 'cum_amount': 111530727.0, 'prc_change_rate': 10.1045, 'stat_value': 9.58},
 {'symbol': 'SHSE.600816', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '有价格涨跌幅限制的日收盘价格涨幅偏离值达到7%的前五只证券', 'close': 2.83, 'cum_volume': 21759618, 'cum_amount': 59379245.0, 'prc_change_rate': 10.1167, 'stat_value': 9.59},
 {'symbol': 'SHSE.600836', 'trade_date': '2024-01-23', 'change_type': '106', 'change_type_name': '有价格涨跌幅限制的日收盘价格涨幅偏离值达到7%的前五只证券', 'close': 3.81, 'cum_volume': 21817595, 'cum_amount': 78789358.0, 'prc_change_rate': 10.1156, 'stat_value': 9.59}]
 
        复制成功
    
注意：

1. 数据日频更新，在交易日约20点更新当日数据。如果当前交易日数据尚未更新，调用时不指定trade_date会返回前一交易日的数据，调用时指定trade_date为当前交易日会返回空。

2. trade_date输入非交易日，会返回空。

