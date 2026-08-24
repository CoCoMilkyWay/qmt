stk_get_ration - 查询股票配股信息
查询指定股票在一段时间内的配股信息

函数原型：

stk_get_ration(symbol, start_date, end_date)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	标的代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
start_date	str	开始时间	Y	无	必填, 开始时间日期（除权除息日），%Y-%m-%d 格式
end_date	str	结束时间	Y	无	必填, 结束时间日期（除权除息日），%Y-%m-%d 格式
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
pub_date	str	公告日	%Y-%m-%d 格式
equity_reg_date	str	股权登记日	%Y-%m-%d 格式
ex_date	str	除权除息日	%Y-%m-%d 格式
ration_ratio	float	配股比例	10:X
ration_price	float	配股价格	单位：元
示例：

stk_get_ration(symbol='SZSE.000728', start_date="2005-01-01", end_date="2022-09-30")
 
        复制成功
    
输出：

        symbol    pub_date equity_reg_date     ex_date  ration_ratio  ration_price
0  SZSE.000728  2020-10-09      2020-10-13  2020-10-22           3.0          5.44
 
        复制成功
    
注意：

1. 当start_date 小于或等于 end_date 时取指定时间段的数据,当start_date > end_date时返回报错.

