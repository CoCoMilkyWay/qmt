bnd_get_analysis - 查询可转债分析指标
查询可转债分析指标

gm SDK 3.0.172 版本新增

函数原型：

bnd_get_analysis(symbol, start_date=None, end_date=None)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	可转债代码	Y	无	必填，只能输入一个可转债的symbol
start_date	str	开始时间	N	None	开始时间日期，%Y-%m-%d 格式，日期类型为交易日期，默认None表示最新时间
end_date	str	结束时间	N	None	结束时间日期，%Y-%m-%d 格式，日期类型为交易日期，默认None表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	可转债代码	查询分析指标的可转债代码
trade_date	str	交易日期	%Y-%m-%d 格式
cnv_value	float	转股价值	平价，是指当前每100元转债转换成股票的价值。转股价值=100×当前正股价格/当前转股价格
cnv_premium	float	转股溢价	转股溢价=转债价格-转股价值
cnv_premium_rate	float	转股溢价率(%)	转股溢价率=转债价格/转股价值-1，当转股溢价率越低时，转债价格和转股价值越接近，转债股性越强，转债的价格对正股的价格波动越敏感。当转股溢价率越高时，转股债性越强。
arbitrage	float	套利空间	转股溢价率为负时的折价套利空间
cur_yield	float	当期收益率(%)	当期收益率=年息票利息/转债价格
pure_value_cb	float	纯债价值(中债基准)	债底，是指在不考虑转债提前赎回、回售或转股的情况下，将转债各期的现金流，根据同期限、同评级的企业债券到期收益率进行贴现，所得贴现值之和即为当前时点转债的纯债价值。 （中债基准）
pure_value_csi	float	纯债价值(中证基准)	债底，是指在不考虑转债提前赎回、回售或转股的情况下，将转债各期的现金流，根据同期限、同评级的企业债券到期收益率进行贴现，所得贴现值之和即为当前时点转债的纯债价值。 （中证基准）
pure_premium	float	纯债溢价	纯债溢价=转债价格-纯债价值
pure_premium_rate	float	纯债溢价率(%)	纯债溢价率=转债价格/纯债价值-1
floor_premium_rate	float	平价底价溢价率(%)	平价底价溢价率=转股价值/转债价值，根据平价底价溢价率，可将转债划分成三类风格：平价底价溢价率大于1.2为偏股型转债，介于0.8和1.2之间为混合型转债，小于0.8为偏债型转债。
cnv_dil_rate	float	转股稀释率(%)	(正股总股本+转股数量)/正股总股本
circ_dil_rate	float	对流通股稀释率(%)	(正股流通股本+转股数量)/正股流通股本
示例：

bnd_get_analysis(symbol='SHSE.118022', start_date=None, end_date=None)
 
        复制成功
    
输出：

        symbol                 trade_date   cnv_value  cnv_premium  \
0  SHSE.118022  2024-11-25T00:00:00+08:00  40.1802962   60.9697038   
   cnv_premium_rate   arbitrage   cur_yield  pure_value_cb  pure_value_csi  \
0      151.74030449 -60.9697038  0.98863075   103.81204257    103.58579086   
   pure_premium  pure_premium_rate  floor_premium_rate  cnv_dil_rate  \
0   -2.43579086        -2.35147199        -61.21061019    9.78546274   
   circ_dil_rate  
0     9.78546274  
 
        复制成功
    
注意：

1. 变动类型指定为首发时，返回的剩余金额为发行金额。

2. 当start_date == end_date时，取离end_date最近变动日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

