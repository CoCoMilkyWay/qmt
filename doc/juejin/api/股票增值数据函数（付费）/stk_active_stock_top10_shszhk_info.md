stk_active_stock_top10_shszhk_info - 查询沪深港通十大活跃成交股数据
查询沪深港通十大活跃成交股数据

gm SDK 3.0.163 版本新增

函数原型：

stk_active_stock_top10_shszhk_info(types=None, trade_date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
types	str or list	类型	N	None	类型，可输入多个，采用 str 格式时，多个类型必须用英文逗号分割，如：'SZ,SHHK' 采用 list 格式时，多个标的代码示例：['SZ', 'SHHK']，类型包括：SH - 沪股通 ，SHHK - 沪港股通 ，SZ - 深股通 ，SZHK - 深港股通，NF - 北向资金（沪股通+深股通），默认 None 为全部北向资金。
trade_date	str or datetime.date	交易日期	N	None	交易日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict],列表每项的dict的key值为参数指定的 fields 。
返回值：

字段名	类型	中文名称	说明
trade_type	str	类型	SH - 沪股通 ，SHHK - 沪港股通 ，SZ - 深股通 ，SZHK - 深港股通，NF - 北向资金（沪股通+深股通）
trade_date	str	最新交易日期	
rank	int	排名	
symbol	str	代码	
sec_name	str	名称	
close	float	收盘价格(元)	
price_range	float	涨跌幅(%)	
buy_amount	float	买入金额(万元)	（沪深港通）买入金额(万元)
buy_volume	float	卖出金额(万元)	（沪深港通）卖出金额(万元)
total_amount	float	成交金额(万元)	（沪深港通）成交金额(万元)
stock_total_amount	float	股票成交金额(万元)	股票成交金额(万元)
transaction_rate	float	成交占比(%)	（沪深港通）成交金额(万元)占股票成交金额(万元)的比例(%)
market_value_total	float	总市值(亿元)	
cum_number_of_times	int	累计上榜次数	股票进入每日十大活跃成交股的次数
currency	str	币种	CNY(人民币) , HKD(港元)
示例：

stk_active_stock_top10_shszhk_info(types='SZHK', trade_date=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'HK.03690', 'trade_date': '2024-01-25', 'rank': 4, 'type': 'SZHK', 'sec_name': '美团-W', 'close': 69.4, 'price_range': -1.2802, 'buy_amount': 30656.8865, 'sell_amount': 28844.0799, 'total_amount': 59500.9664, 'stock_total_amount': 310032.2982, 'transaction_rate': 19.1919, 'market_value_total': 4333.9628, 'cum_number_of_times': 1996, 'currency': 'HKD'},
 {'symbol': 'HK.02318', 'trade_date': '2024-01-25', 'rank': 8, 'type': 'SZHK', 'sec_name': '中国平安', 'close': 34.6, 'price_range': 4.8485, 'buy_amount': 11403.6125, 'sell_amount': 10698.3075, 'total_amount': 22101.92, 'stock_total_amount': 296208.1587, 'transaction_rate': 7.4616, 'market_value_total': 6300.7412, 'cum_number_of_times': 790, 'currency': 'HKD'},
 {'symbol': 'HK.01810', 'trade_date': '2024-01-25', 'rank': 10, 'type': 'SZHK', 'sec_name': '小米集团-W', 'close': 13.74, 'price_range': 0.292, 'buy_amount': 6782.6328, 'sell_amount': 13975.542, 'total_amount': 20758.1748, 'stock_total_amount': 115367.6607, 'transaction_rate': 17.9931, 'market_value_total': 3445.2863, 'cum_number_of_times': 1246, 'currency': 'HKD'},
 {'symbol': 'HK.01797', 'trade_date': '2024-01-25', 'rank': 6, 'type': 'SZHK', 'sec_name': '东方甄选', 'close': 24.2, 'price_range': -8.3333, 'buy_amount': 21350.9475, 'sell_amount': 12630.7475, 'total_amount': 33981.695, 'stock_total_amount': 67946.4211, 'transaction_rate': 50.0125, 'market_value_total': 246.0016, 'cum_number_of_times': 348, 'currency': 'HKD'},
 {'symbol': 'HK.01024', 'trade_date': '2024-01-25', 'rank': 9, 'type': 'SZHK', 'sec_name': '快手-W', 'close': 44.6, 'price_range': -0.112, 'buy_amount': 14876.216, 'sell_amount': 6185.893, 'total_amount': 21062.109, 'stock_total_amount': 97141.6742, 'transaction_rate': 21.6818, 'market_value_total': 1948.6613, 'cum_number_of_times': 637, 'currency': 'HKD'},
 {'symbol': 'HK.00981', 'trade_date': '2024-01-25', 'rank': 7, 'type': 'SZHK', 'sec_name': '中芯国际', 'close': 16.04, 'price_range': 3.4839, 'buy_amount': 15796.551, 'sell_amount': 7910.155, 'total_amount': 23706.706, 'stock_total_amount': 74554.1682, 'transaction_rate': 31.798, 'market_value_total': 1274.6363, 'cum_number_of_times': 1533, 'currency': 'HKD'},
 {'symbol': 'HK.00941', 'trade_date': '2024-01-25', 'rank': 2, 'type': 'SZHK', 'sec_name': '中国移动', 'close': 67.65, 'price_range': 2.3449, 'buy_amount': 61646.64, 'sell_amount': 26017.6775, 'total_amount': 87664.3175, 'stock_total_amount': 248358.7732, 'transaction_rate': 35.2975, 'market_value_total': 14472.2173, 'cum_number_of_times': 1118, 'currency': 'HKD'},
 {'symbol': 'HK.00883', 'trade_date': '2024-01-25', 'rank': 3, 'type': 'SZHK', 'sec_name': '中国海洋石油', 'close': 14.44, 'price_range': 4.7896, 'buy_amount': 57442.304, 'sell_amount': 23673.3544, 'total_amount': 81115.6584, 'stock_total_amount': 269234.0355, 'transaction_rate': 30.1283, 'market_value_total': 6868.6407, 'cum_number_of_times': 1044, 'currency': 'HKD'},
 {'symbol': 'HK.00762', 'trade_date': '2024-01-25', 'rank': 5, 'type': 'SZHK', 'sec_name': '中国联通', 'close': 5.45, 'price_range': 4.6065, 'buy_amount': 21708.258, 'sell_amount': 12586.296, 'total_amount': 34294.554, 'stock_total_amount': 59194.3214, 'transaction_rate': 57.9355, 'market_value_total': 1667.5978, 'cum_number_of_times': 112, 'currency': 'HKD'},
 {'symbol': 'HK.00700', 'trade_date': '2024-01-25', 'rank': 1, 'type': 'SZHK', 'sec_name': '腾讯控股', 'close': 290.8, 'price_range': 3.1938, 'buy_amount': 31622.082, 'sell_amount': 76991.966, 'total_amount': 108614.048, 'stock_total_amount': 984146.2621, 'transaction_rate': 11.0364, 'market_value_total': 27487.5599, 'cum_number_of_times': 3613, 'currency': 'HKD'}]

 
        复制成功
    
注意：

1. 数据日频更新，在交易日约20点更新当日数据。如果当前交易日数据尚未更新，调用时不指定trade_date会返回前一交易日的数据，调用时指定trade_date为当前交易日会返回空。

2. trade_date输入非交易日，会返回空。

