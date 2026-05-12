stk_quota_shszhk_infos - 查询沪深港通额度数据
查询指定时间段沪深港通额度数据

gm SDK 3.0.163 版本新增

交易所信息披露调整，2024.8.19起，NF-北向资金/SH-沪股通/SZ-深股通只返回结算汇率数据，历史数据不受影响，SHHK-沪港股通/SZHK-深港股通不受影响

函数原型：

stk_quota_shszhk_infos(types=None, start_date=None, end_date=None, count=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
types	str or list	类型	N	None	类型，可输入多个，采用 str 格式时，多个类型必须用英文逗号分割，如：'SZ,SHHK' 采用 list 格式时，多个标的代码示例：['SZ', 'SHHK']，类型包括：SH - 沪股通 ，SHHK - 沪港股通 ，SZ - 深股通 ，SZHK - 深港股通，NF - 北向资金（沪股通+深股通），默认 None 为全部北向资金。
start_date	str or datetime.date	开始日期	N	None	开始日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
end_date	str or datetime.date	结束日期	N	None	结束日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
count	int	交易日数量	N	None	数量(正整数)，不能与start_date同时使用，否则返回报错；与 end_date 同时使用时，表示获取 end_date 前 count 个交易日的数据(包含 end_date 当日)；默认为 None ，不使用该字段。
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict],列表每项的dict的key值为参数指定的 fields 。
返回值：

字段名	类型	中文名称	说明
type	str	类型	SH - 沪股通 ，SHHK - 沪港股通 ，SZ - 深股通 ，SZHK - 深港股通，NF - 北向资金（沪股通+深股通）
trade_date	str	最新交易日期	
daily_quota	float	每日额度上限(亿元)	
day_balance	float	当日余额(亿元)	
day_used	float	当日使用额度(亿元)	
day_used_rate	float	当日额度使用率(%)	
day_buy_amount	float	当日买入成交金额(亿元)	
day_buy_volume	float	当日买入成交笔数(笔)	
day_sell_amount	float	当日卖出成交金额(亿元)	
day_sell_volume	float	当日卖出成交笔数(笔)	
day_net_amount	float	当日买卖成交净额(亿元)	
settle_exrate_buy	float	沪深港通结算汇率(买入)	
settle_exrate_sell	float	沪深港通结算汇率(卖出)	
示例：

stk_quota_shszhk_infos(types='SHHK', start_date=None, end_date='2024-01-23', count=1, df=False)
 
        复制成功
    
输出：

[{'type': 'SHHK', 'trade_date': '2024-01-23', 'daily_quota': 420.0, 'day_balance': 433.3, 'day_used': -13.3, 'day_used_rate': -3.167, 'day_buy_amount': 63.4454, 'day_buy_volume': 179494.0, 'day_sell_amount': 86.4077, 'day_sell_volume': 216835.0, 'settle_exrate_buy': 0.9209, 'settle_exrate_sell': 0.9225, 'day_net_amount': -22.9623}]
 
        复制成功
    
注意：

1. 当start_date == end_date时，取离end_date最近公告日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

2. count不能与start_date同时使用，否则返回报错；与 end_date 同时使用时，表示获取 end_date 前 count 个交易日的数据(包含 end_date 当日)；默认为 None ，不使用该字段。

