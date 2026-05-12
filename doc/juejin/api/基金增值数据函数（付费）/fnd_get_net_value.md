fnd_get_net_value - 查询基金净值数据
查询某只基金在指定时间段的基金净值数据

函数原型：

fnd_get_net_value(fund, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
fund	str	基金代码	Y	无	必填，只能输入一个基金的symbol，如：'SZSE.159919'
start_date	str	开始时间	N	""	开始时间日期，%Y-%m-%d 格式，默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期，%Y-%m-%d 格式，默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
fund	str	基金代码	查询净值的基金代码
trade_date	str	交易日期	指定时间段内的交易日期，%Y-%m-%d 格式
unit_nv	float	单位净值	T 日单位净值是每个基金份额截至 T 日的净值（也是申赎的价格）
unit_nv_accu	float	累计单位净值	T 日累计净值是指，在基金成立之初投资该基金 1 元钱，在现金分红方式下，截至 T 日账户的净值
unit_nv_adj	float	复权单位净值	T 日复权净值是指，在基金成立之初投资该基金 1 元钱，在分红再投资方式下，截至 T 日账户的净值
示例：

fnd_get_net_value(fund='SHSE.510300')
 
        复制成功
    
输出：

          fund  trade_date  unit_nv  unit_nv_accu  unit_nv_adj
0  SHSE.510300  2022-10-19     3.84        1.6233       1.6579

 
        复制成功
    
注意：

1. 仅提供场内基金（ETF、LOF、FOF-LOF）的净值数据。

2. 当start_date == end_date时，取离end_date最近日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

