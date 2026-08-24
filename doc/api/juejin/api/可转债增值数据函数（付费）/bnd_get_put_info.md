bnd_get_put_info - 查询可转债回售信息
查询可转债一段时间内的回售情况

函数原型：

bnd_get_put_info(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	可转债代码	Y	无	必填，只能输入一个可转债的symbol
start_date	str	开始时间	N	""	开始时间日期（公告日），%Y-%m-%d 格式， 默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（公告日），%Y-%m-%d 格式， 默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
pub_date	str	公告日	回售公告日，%Y-%m-%d 格式
put_start_date	str	赎回日	投资者行权起始日，%Y-%m-%d 格式
put_end_date	str	赎回登记日	投资者行权截止日，%Y-%m-%d 格式
cash_date	str	赎回资金到账日	投资者回售款到账日
put_reason	str	回售原因	满足回售条款，满足附加回售条款
put_price	float	回售价格	单位：元/张，每百元面值回售价格（元），即债券面值加当期应计利息（含税）
interest_included	bool	是否包含利息	False-不包含，True-包含
示例：

bnd_get_put_info(symbol='SZSE.128015')
 
        复制成功
    
输出：

     pub_date put_start_date put_end_date   cash_date put_reason  put_price  interest_included
0  2022-06-09     2022-06-16   2022-06-22  2022-06-29     满足回售条款    100.039               True

 
        复制成功
    
注意：

当start_date == end_date时，取离end_date最近公告日的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

