fnd_get_portfolio - 查询基金资产组合
查询某只基金在指定日期的基金资产组合（股票持仓、债券持仓等）

函数原型：

fnd_get_portfolio(fund, report_type, portfolio_type, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
fund	str	基金代码	Y	无	必填，只能输入一个基金的symbol，如：'SZSE.161133'
report_type	int	报表类别	Y	无	公布持仓所在的报表类别，必填，可选： 1:第一季度 2:第二季度 3:第三季报 4:第四季度 6:中报 12:年报
portfolio_type	str	投资组合类型	Y	无	必填，可选以下其中一种组合： 'stk' - 股票投资组合 'bnd' - 债券投资组合 'fnd' - 基金投资组合
start_date	str	开始时间	N	""	开始时间日期（公告日），%Y-%m-%d 格式，默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（公告日），%Y-%m-%d 格式，默认""表示最新时间
返回值：dataframe

portfolio_type='stk'时，返回基金持有的股票投资组合信息 portfolio_stock

字段名	类型	中文名称	说明
fund	str	基金代码	查询股票资产组合的基金代码
fund_name	str	基金名称	
pub_date	str	公告日期	在指定时间段内的公告日期，%Y-%m-%d 格式
period_end	str	报告期	持仓截止日期，%Y-%m-%d 格式
symbol	str	股票代码	exchange.sec_id
sec_name	str	股票名称	
hold_share	float	持仓股数	
hold_value	float	持仓市值	
nv_rate	float	占净值比例	单位：%
ttl_share_rate	float	占总股本比例	单位：%
clrc_share_rate	float	占流通股比例	单位：%
portfolio_type='bnd'时，返回基金持有的债券投资组合信息 portfolio_bond

字段名	类型	中文名称	说明
fund	str	基金代码	查询债券资产组合的基金代码
fund_name	str	基金名称	
pub_date	str	公告日期	在指定时间段内的公告日期，%Y-%m-%d 格式
period_end	str	报告期	持仓截止日期，%Y-%m-%d 格式
symbol	str	债券代码	exchange.sec_id
sec_name	str	债券名称	
hold_share	float	持仓数量	
hold_value	float	持仓市值	
nv_rate	float	占净值比例	单位：%
portfolio_type='fnd'时，返回基金持有的基金投资组合信息 portfolio_fund

字段名	类型	中文名称	说明
fund	str	基金代码	查询基金资产组合的基金代码
fund_name	str	基金名称	
pub_date	str	公告日期	在指定时间段内的公告日期，%Y-%m-%d 格式
period_end	str	报告期	持仓截止日期，%Y-%m-%d 格式
symbol	str	持仓基金代码	exchange.sec_id
sec_name	str	持仓基金名称	
hold_share	float	持有份额	
hold_value	float	期末市值	
nv_rate	float	占净值比例	单位：%
示例：

fnd_get_portfolio(fund='SHSE.510300', start_date='2022-01-01', end_date='2022-10-01', report_type=1, portfolio_type='stk')
 
        复制成功
    
输出：

           fund     fund_name    pub_date  period_end       symbol sec_name  hold_share  hold_value  nv_rate  ttl_share_rate  circ_share_rate
0   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.600519     贵州茅台  1.4424e+06  2.4794e+09     5.54          5.6773           0.1148
1   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.600900     长江电力  2.6245e+07  5.7738e+08     1.29          1.3221           0.1154
2   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SZSE.000333     美的集团  1.1271e+07  6.4247e+08     1.44          1.4711           0.1648
3   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SZSE.301102     兆讯传媒  6.4140e+03  1.9947e+05     0.00          0.0005           0.0149
4   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SZSE.301088     戎美股份  7.0360e+03  1.3434e+05     0.00          0.0003           0.0134
5   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.600036     招商银行  2.8572e+07  1.3372e+09     2.99          3.0618           0.1385
6   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SZSE.300750     宁德时代  3.2106e+06  1.6448e+09     3.68          3.7661           0.1575
7   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.688223     晶科能源  2.1300e+05  2.1620e+06     0.00          0.0050           0.0161
8   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.601166     兴业银行  3.3444e+07  6.9129e+08     1.55          1.5829           0.1705
9   SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.601318     中国平安  2.4996e+07  1.2110e+09     2.71          2.7730           0.2307
10  SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.688282     理工导航  3.1850e+03  1.5119e+05     0.00          0.0003           0.0162
11  SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.603259     药明康德  4.7151e+06  5.2989e+08     1.18          1.2133           0.1851
12  SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.688234     天岳先进  6.7590e+03  3.4451e+05     0.00          0.0008           0.0200
13  SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SHSE.601012     隆基股份  9.9703e+06  7.1976e+08     1.61          1.6481           0.1842
14  SHSE.510300  华泰柏瑞沪深300ETF  2022-04-22  2022-03-31  SZSE.000858      五粮液  4.4720e+06  6.9342e+08     1.55          1.5878           0.1152

 
        复制成功
    
注意：

1. 仅提供场内基金（ETF、LOF、FOF-LOF）的资产组合数据。

2. 当start_date == end_date时，取离end_date最近公告日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

