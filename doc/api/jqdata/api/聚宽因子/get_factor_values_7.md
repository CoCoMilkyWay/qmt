动量类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取动量类因子值

注意

为保证数据的连续性，所有数据基于后复权计算

为了防止单次返回数据时间过长，每次调用 api 请求的因子值不能超过 200000 个

参数

securities:股票池，单只股票（字符串）或一个股票列表

factors: 因子名称，单个因子（字符串）或一个因子列表

start_date:开始日期，字符串或 datetime 对象，与 coun t参数二选一

end_date: 结束日期， 字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日），与 start_date 参数二选一

返回

一个 dict： key 是因子名称， value 是 pandas.dataframe。

dataframe 的 index 是日期， column 是股票代码， value 是因子值

因子 code	因子名称	计算方法

arron_up_25	Aroon指标上轨	Aroon(上升)=[(计算期天数-最高价后的天数)/计算期天数]*100

arron_down_25	Aroon指标下轨	Aroon(下降)=[(计算期天数-最低价后的天数)/计算期天数]*100

BBIC	BBI 动量	BBI(3, 6, 12, 24) / 收盘价 （BBI 为常用技术指标类因子“多空均线”）

bear_power	空头力道	(最低价-EMA(close,13)) / close

BIAS5	5日乖离率	（收盘价-收盘价的N日简单平均）/ 收盘价的N日简单平均*100，在此n取5

BIAS10	10日乖离率	（收盘价-收盘价的N日简单平均）/ 收盘价的N日简单平均*100，在此n取10

BIAS20	20日乖离率	（收盘价-收盘价的N日简单平均）/ 收盘价的N日简单平均*100，在此n取20

BIAS60	60日乖离率	（收盘价-收盘价的N日简单平均）/ 收盘价的N日简单平均*100，在此n取60

bull_power	多头力道	(最高价-EMA(close,13)) / close

CCI10	10日顺势指标	CCI:=(TYP-MA(TYP,N))/(0.015*AVEDEV(TYP,N)); TYP:=(HIGH+LOW+CLOSE)/3; N:=10

CCI15	15日顺势指标	CCI:=(TYP-MA(TYP,N))/(0.015*AVEDEV(TYP,N)); TYP:=(HIGH+LOW+CLOSE)/3; N:=15

CCI20	20日顺势指标	CCI:=(TYP-MA(TYP,N))/(0.015*AVEDEV(TYP,N)); TYP:=(HIGH+LOW+CLOSE)/3; N:=20

CCI88	88日顺势指标	CCI:=(TYP-MA(TYP,N))/(0.015*AVEDEV(TYP,N)); TYP:=(HIGH+LOW+CLOSE)/3; N:=88

CR20	CR指标	①中间价=1日前的最高价+最低价/2
②上升值=今天的最高价-前一日的中间价（负值记0）
③下跌值=前一日的中间价-今天的最低价（负值记0）
④多方强度=20天的上升值的和，空方强度=20天的下跌值的和
⑤CR=（多方强度÷空方强度）×100

fifty_two_week_close_rank	当前价格处于过去1年股价的位置	取过去的250个交易日各股的收盘价时间序列，每只股票按照从大到小排列，并找出当日所在的位置

MASS	梅斯线	MASS(N1=9, N2=25, M=6)

PLRC12	12日收盘价格与日期线性回归系数	计算 12 日收盘价格，与日期序号（1-12）的线性回归系数，(close / mean(close)) = beta * t + alpha

PLRC24	24日收盘价格与日期线性回归系数	计算 24 日收盘价格，与日期序号（1-24）的线性回归系数， (close / mean(close)) = beta * t + alpha

PLRC6	6日收盘价格与日期线性回归系数	计算 6 日收盘价格，与日期序号（1-6）的线性回归系数，(close / mean(close)) = beta * t + alpha

Price1M	当前股价除以过去一个月股价均值再减1	当日收盘价 / mean(过去一个月(21天)的收盘价) -1

Price3M	当前股价除以过去三个月股价均值再减1	当日收盘价 / mean(过去三个月(61天)的收盘价) -1

Price1Y	当前股价除以过去一年股价均值再减1	当日收盘价 / mean(过去一年(250天)的收盘价) -1

Rank1M	1减去 过去一个月收益率排名与股票总数的比值	1-(Rank(个股20日收益) / 股票总数)

ROC12	12日变动速率（Price Rate of Change）	①AX=今天的收盘价—12天前的收盘价
②BX=12天前的收盘价
③ROC=AX/BX*100

ROC120	120日变动速率（Price Rate of Change）	①AX=今天的收盘价—120天前的收盘价
②BX=120天前的收盘价
③ROC=AX/BX*100

ROC20	20日变动速率（Price Rate of Change）	①AX=今天的收盘价—20天前的收盘价
②BX=20天前的收盘价
③ROC=AX/BX*100

ROC6	6日变动速率（Price Rate of Change）	①AX=今天的收盘价—6天前的收盘价
②BX=6天前的收盘价
③ROC=AX/BX*100

ROC60	60日变动速率（Price Rate of Change）	①AX=今天的收盘价—60天前的收盘价
②BX=60天前的收盘价
③ROC=AX/BX*100

single_day_VPT	单日价量趋势	（今日收盘价 - 昨日收盘价）/ 昨日收盘价 * 当日成交量  # (复权方法为基于当日前复权)

single_day_VPT_12	单日价量趋势12均值	MA(single_day_VPT, 12)

single_day_VPT_6	单日价量趋势6日均值	MA(single_day_VPT, 6)

TRIX10	10日终极指标TRIX	MTR=收盘价的10日指数移动平均的10日指数移动平均的10日指数移动平均(求三次ema10);
TRIX=(MTR-1日前的MTR)/1日前的MTR*100

TRIX5	5日终极指标TRIX	MTR=收盘价的5日指数移动平均的5日指数移动平均的5日指数移动平均(求三次ema5);
TRIX=(MTR-1日前的MTR)/1日前的MTR*100

Volume1M	当前交易量相比过去1个月日均交易量 与过去过去20日日均收益率乘积	当日交易量 / 过去20日交易量MEAN * 过去20日收益率MEAN

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['arron_up_25','arron_down_25','BBIC'], start_date='2022-01-01', end_date='2022-01-10')

print(factor_data['BBIC'])
