bnd_get_call_info - 查询可转债赎回信息
查询可转债一段时间内的赎回情况

函数原型：

bnd_get_call_info(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	可转债代码	Y	无	必填，只能输入一个可转债的symbol
start_date	str	开始时间	N	""	开始时间日期（公告日），%Y-%m-%d 格式， 默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（公告日），%Y-%m-%d 格式， 默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
pub_date	str	公告日	赎回公告日，%Y-%m-%d 格式
call_date	str	赎回日	发行人行权日（实际），%Y-%m-%d 格式
record_date	str	赎回登记日	理论登记日，%Y-%m-%d 格式
cash_date	str	赎回资金到账日	投资者赎回款到账日
call_type	str	赎回类型	部分赎回，全部赎回
call_reason	str	赎回原因	满足赎回条件，强制赎回，到期赎回
call_price	float	赎回价格	单位：元/张，每百元面值赎回价格，即债券面值加当期应计利息（含税）
interest_included	bool	是否包含利息	False-不包含，True-包含
示例：

bnd_get_call_info(symbol='SHSE.110041')
 
        复制成功
    
输出：

     pub_date   call_date record_date cash_date call_type call_reason  call_price  interest_included
0  2021-10-18  2021-11-05  2021-11-04      None      全部赎回        强制赎回     101.307               True

 
        复制成功
    
注意：

当start_date == end_date时，取离end_date最近公告日的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

