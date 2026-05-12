get_trading_session - 查询交易时段
查询一个标的所属品种交易时间段.

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

get_trading_session(symbols, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	标的代码	Y	无	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002'].
df	bool	返回格式	N	False	是否返回 dataframe 格式， 默认False返回字典格式，返回list[dict]，列表每项的 dict 的 key 值见返回字段名
返回值：

字段名	类型	中文名称	说明
symbol	str	标的代码	exchange.sec_id
exchange	str	交易所代码	SHSE:上海证券交易所，SZSE:深圳证券交易所，CFFEX:中金所， SHFE:上期所，DCE:大商所，CZCE:郑商所，INE:上海国际能源交易中心，GFEX:广期所
time_trading	list[dict]	连续竞价时段	HH:MM 格式，按时间顺序排列，如品种存在夜盘，夜盘时段排最前。 如[{'start': '09:30'，'end': '11:30'}， {'start': '13:00'， 'end': '14:57'}]，
time_auction	list[dict]	集合竞价时段	HH:MM 格式，按时间顺序排列，如品种存在夜盘，夜盘时段排最前。 如[{’start': '09:15'， 'end': '09:25'}，{'start': '14:57'， 'end': '15:00'}]，
示例：

get_trading_session(symbols='SHFE.au2306', df=False)
 
        复制成功
    
输出：

[{'symbol': 'SHFE.AU2306', 'exchange': 'SHFE', 'time_trading': [{'start': '21:00', 'end': '2:30'}, {'start': '9:00', 'end': '10:15'}, {'start': '10:30', 'end': '11:30'}, {'start': '13:30', 'end': '15:00'}], 'time_auction': [{'start': '20:55', 'end': '20:59'}]}]
 
        复制成功
    
注意：

1. 如果输入不存在的合约代码 symbol，会报错提示"该合约[symbol]不存在"。

