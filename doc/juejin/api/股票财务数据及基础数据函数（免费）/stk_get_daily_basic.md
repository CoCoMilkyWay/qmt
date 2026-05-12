stk_get_daily_basic - 查询股本等基础指标每日数据
查询指定时间段股票的每日基础指标

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_daily_basic(symbol, fields, start_date=None, end_date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	股票代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
fields	str	返回字段	Y	无	指定需要返回的财务字段， 如有多个字段，中间用英文逗号分隔
start_date	str	开始时间	N	None	开始时间，时间类型为交易日期，%Y-%m-%d 格式， 默认None表示最新时间
end_date	str	结束时间	N	None	结束时间，时间类型为交易日期，%Y-%m-%d 格式， 默认None表示最新时间
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict]
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	
trade_date	str	交易日期	
fields	list[float]	指标字段数据	指定返回 fields字段的数值. 支持的字段名请参考 基础指标
示例：

stk_get_daily_basic(symbol='SHSE.600000', fields='tclose,turnrate,ttl_shr,circ_shr',
                                  start_date=None, end_date=None, df=True)
 
        复制成功
    
输出：

        symbol  trade_date  turnrate    circ_shr     ttl_shr  tclose
0  SHSE.600000  2023-06-26    0.1159  2.9352e+10  2.9352e+10    7.16
 
        复制成功
    
注意：

1. 当start_date == end_date时，取离 end_date 最近交易日期的一条数据，

当start_dat< end_date时，取指定时间段的数据，

当 start_date > end_date时，返回报错。

2. 如果fields参数的财务字段填写不正确，或填写空字段，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

基础指标

字段名	类型	中文名称	量纲	说明
tclose	float	收盘价	元	
turnrate	float	当日换手率	%	
ttl_shr	float	总股本	股	
circ_shr	float	流通股本（流通股本=无限售条件流通股本+有限售条件流通股本）	股	
ttl_shr_unl	float	无限售条件流通股本(A股+H股)	股	
ttl_shr_ltd	float	有限售条件股本	股	
a_shr_unl	float	无限售条件流通A股股本(行情软件定义的流通股)	股	
h_shr_unl	float	无限售条件流通H股股本	股	