fut_get_transaction_rankings - 查询期货每日成交持仓排名
查询期货合约每日成交量/持买单量/持卖单量排名

函数原型：

fut_get_transaction_rankings(symbols, trade_date="", indicators="volume")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	期货合约代码	Y	无	必填，期货真实合约代码，使用时参考symbol, 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'CFFEX.IF2409, CFFEX.IC2409', 采用 list 格式时，多个标的代码示例：['CFFEX.IF2409', 'CFFEX.IC2409']
trade_date	str	交易日期	N	""	交易日期，%Y-%m-%d 格式，默认""表示最新交易日
indicators	str	排名指标	N	""	排名指标，即用于排名的依据，可选：'volume'-成交量排名（默认）, 'long'-持买单量排名, 'short'-持卖单量排名, 支持一次查询多个排名指标，如有多个指标，中间用英文逗号分隔, 默认None表示成交量排名
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	期货合约代码	查询排名的期货合约代码
trade_date	str	交易日期	查询的交易日期
member_name	str	期货公司会员简称	
indicator	str	排名指标	'volume'-成交量排名, 'long'-持买单量排名, 'short'-持卖单量排名
indicator_number	int	排名指标数值	单位：手。数值视乎指定的排名指标 indicator，分别为： 成交量（indicator='volume'时） 持买单量（indicator='long'时） 持卖单量（indicator='short'时）
indicator_change	int	排名指标比上交易日增减	单位：手
ranking	int	排名名次	指标具体排名
ranking_change	float	排名名次比上交易日增减	单位：名次
示例：

fut_get_transaction_rankings(symbols='SHFE.ag2212', trade_date="2022-10-10", indicators='volume')
 
        复制成功
    
输出：

symbol  trade_date member_name  indicator_number  indicator_change  \
0   SHFE.ag2212  2022-10-10        海通期货             90561            -19632   
1   SHFE.ag2212  2022-10-10        东证期货             89284            -74685   
2   SHFE.ag2212  2022-10-10        中信期货             64196            -77571   
3   SHFE.ag2212  2022-10-10        国泰君安             36535            -40570   
4   SHFE.ag2212  2022-10-10        新湖期货             22090            -16824   
5   SHFE.ag2212  2022-10-10        华闻期货             16531               826   
6   SHFE.ag2212  2022-10-10        方正中期             14787            -17407   
7   SHFE.ag2212  2022-10-10        华泰期货             14315            -71181   
8   SHFE.ag2212  2022-10-10        银河期货             13333             -9714   
9   SHFE.ag2212  2022-10-10        中泰期货             11832             -6380   
10  SHFE.ag2212  2022-10-10        国投安信             11041            -10375   
11  SHFE.ag2212  2022-10-10        光大期货              9917            -14549   
12  SHFE.ag2212  2022-10-10        中信建投              9653            -12342   
13  SHFE.ag2212  2022-10-10        广发期货              8440             -9462   
14  SHFE.ag2212  2022-10-10        东方财富              8166            -21117   
15  SHFE.ag2212  2022-10-10        南华期货              7096             -3422   
16  SHFE.ag2212  2022-10-10        平安期货              6835             -8312   
17  SHFE.ag2212  2022-10-10        浙商期货              6610             -2008   
18  SHFE.ag2212  2022-10-10        中辉期货              6569             -3830   
19  SHFE.ag2212  2022-10-10        永安期货              6351              -741   
    ranking  ranking_change indicator  
0         1             2.0    volume  
1         2            -1.0    volume  
2         3            -1.0    volume  
3         4             1.0    volume  
4         5             1.0    volume  
5         6            10.0    volume  
6         7             0.0    volume  
7         8            -4.0    volume  
8         9             1.0    volume  
9        10             4.0    volume  
10       11             2.0    volume  
11       12            -3.0    volume  
12       13            -2.0    volume  
13       14             1.0    volume  
14       15            -7.0    volume  
15       16             3.0    volume  
16       17             0.0    volume  
17       18             NaN    volume  
18       19             1.0    volume  
19       20             NaN    volume  

 
        复制成功
    
注意：

1. 当上一交易日没有进入前 20 名，ranking_change返回 NaN.

2. 数据日频更新，当日更新前返回前一交易日的排名数据，约在交易日 20 点左右更新当日数据。

