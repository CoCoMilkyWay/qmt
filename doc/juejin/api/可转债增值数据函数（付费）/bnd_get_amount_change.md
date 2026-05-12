bnd_get_amount_change - 查询可转债剩余规模变动
查询可转债转股、回售、赎回等事件导致的剩余规模变动的情况

函数原型：

bnd_get_amount_change(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	可转债代码	Y	无	必填，只能输入一个可转债的symbol
start_date	str	开始时间	N	""	开始时间日期（变动日期），%Y-%m-%d 格式， 默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（变动日期），%Y-%m-%d 格式， 默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
pub_date	str	公告日	%Y-%m-%d 格式
change_date	str	变动日期	%Y-%m-%d 格式
change_type	str	变动类型	首发，增发，转股，赎回，回售(注销)，到期
change_amount	float	本次变动金额	单位：万元
remain_amount	float	剩余金额	变动后金额，单位：万元
示例：

bnd_get_amount_change(symbol='SZSE.123015')
 
        复制成功
    
输出：

     pub_date change_type change_date  change_amount  remain_amount
0  2022-10-10          转股  2022-09-30           8.91       10004.18

 
        复制成功
    
注意：

1. 变动类型指定为首发时，返回的剩余金额为发行金额。

2. 当start_date == end_date时，取离end_date最近变动日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

