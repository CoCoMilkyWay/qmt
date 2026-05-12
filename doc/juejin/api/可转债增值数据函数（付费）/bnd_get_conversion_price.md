bnd_get_conversion_price - 查询可转债转股价变动信息
查询可转债一段时间的转股价变动和转股结果

函数原型：

bnd_get_conversion_price(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	可转债代码	Y	无	必填，只能输入一个可转债的symbol
start_date	str	开始时间	N	""	开始时间日期（转股价格生效日），%Y-%m-%d 格式， 默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（转股价格生效日），%Y-%m-%d 格式， 默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
pub_date	str	公告日期	%Y-%m-%d 格式
effective_date	str	转股价格生效日期	%Y-%m-%d 格式
execution_date	str	执行日期	%Y-%m-%d 格式
conversion_price	float	转股价格	单位：元
conversion_rate	float	转股比例	单位：%
conversion_volume	float	本期转股数	单位：股
conversion_amount_total	float	累计转股金额	单位：万元，累计转债已经转为股票的金额，累计每次转股金额
bond_float_amount_remain	float	债券流通余额	单位：万元
event_type	str	事件类型	初始转股价，调整转股价，修正转股价
change_reason	str	转股价变动原因	发行，股权激励，股权分置，触发修正条款，其它变动原因，换股吸收合并， 配股，增发，上市，派息，送股，转增股，修正
示例：

bnd_get_conversion_price(symbol='SZSE.123015')
 
        复制成功
    
输出：

pub_date effective_date execution_date  conversion_price  conversion_rate  conversion_volume  conversion_amount_total  bond_float_amount_remain event_type change_reason
0  2022-07-29     2022-08-01     2022-08-01              2.38          42.0168                0.0                      0.0                       0.0      修正转股价     修正,触发修正条款


 
        复制成功
    
注意：

1. 本期转股数、累计转股金额、债券流通余额在执行日期收盘后才有数据。

2. 当start_date == end_date时，取离end_date最近转股价格生效日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

