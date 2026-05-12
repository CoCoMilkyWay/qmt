stk_abnor_change_detail - 查询龙虎榜营业部数据
查询指定时间段龙虎榜营业数据

gm SDK 3.0.163 版本新增

函数原型：

stk_abnor_change_detail(symbols=None, change_types=None, trade_date=None, fields=None, df=False)
 
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
side	int	交易方向	0-买入 1-卖出
sales_dept	str	营业部名称	
buy_amount	float	买入金额	
sell_amount	float	卖出金额	
rank	int	排名	
stat_days	str	统计天数	
示例：

stk_abnor_change_detail(symbols=['SZSE.300799'], change_types=None, trade_date='2024-01-23', fields=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨东环路第一证券营业部', 'buy_amount': 14564894.0, 'sell_amount': 7558746.0, 'rank': 1, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨团结路第二证券营业部', 'buy_amount': 9851257.0, 'sell_amount': 7961995.0, 'rank': 2, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨东环路第二证券营业部', 'buy_amount': 9168211.0, 'sell_amount': 10625788.4, 'rank': 3, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨金融城南环路证券营业部', 'buy_amount': 8130605.0, 'sell_amount': 4825320.0, 'rank': 4, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨团结路第一证券营业部', 'buy_amount': 7874940.0, 'sell_amount': 7103053.0, 'rank': 5, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨东环路第二证券营业部', 'buy_amount': 9168211.0, 'sell_amount': 10625788.4, 'rank': 1, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '华泰证券股份有限公司杭州求是路证券营业部', 'buy_amount': 86904.0, 'sell_amount': 8832233.0, 'rank': 2, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨团结路第二证券营业部', 'buy_amount': 9851257.0, 'sell_amount': 7961995.0, 'rank': 3, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨东环路第一证券营业部', 'buy_amount': 14564894.0, 'sell_amount': 7558746.0, 'rank': 4, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨团结路第一证券营业部', 'buy_amount': 7874940.0, 'sell_amount': 7103053.0, 'rank': 5, 'change_type': '149'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨东环路第一证券营业部', 'buy_amount': 20244889.0, 'sell_amount': 13720238.0, 'rank': 1, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨东环路第二证券营业部', 'buy_amount': 16872156.0, 'sell_amount': 16700434.4, 'rank': 2, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨团结路第二证券营业部', 'buy_amount': 16274111.0, 'sell_amount': 13804247.0, 'rank': 3, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨团结路第一证券营业部', 'buy_amount': 13191129.0, 'sell_amount': 13434925.0, 'rank': 4, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'sales_dept': '东方财富证券股份有限公司拉萨金融城南环路证券营业部', 'buy_amount': 12009881.0, 'sell_amount': 7226749.0, 'rank': 5, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨东环路第二证券营业部', 'buy_amount': 16872156.0, 'sell_amount': 16700434.4, 'rank': 1, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨团结路第二证券营业部', 'buy_amount': 16274111.0, 'sell_amount': 13804247.0, 'rank': 2, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨东环路第一证券营业部', 'buy_amount': 20244889.0, 'sell_amount': 13720238.0, 'rank': 3, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨团结路第一证券营业部', 'buy_amount': 13191129.0, 'sell_amount': 13434925.0, 'rank': 4, 'stat_days': '2         ', 'change_type': '153'},
 {'symbol': 'SZSE.300799', 'trade_date': '2024-01-23', 'side': 1, 'sales_dept': '东方财富证券股份有限公司拉萨东城区江苏大道证券营业部', 'buy_amount': 8374957.0, 'sell_amount': 11359868.5, 'rank': 5, 'stat_days': '2         ', 'change_type': '153'}]
 
        复制成功
    
注意：

1. 数据日频更新，在交易日约20点更新当日数据。如果当前交易日数据尚未更新，调用时不指定trade_date会返回前一交易日的数据，调用时指定trade_date为当前交易日会返回空。

2. trade_date输入非交易日，会返回空。

