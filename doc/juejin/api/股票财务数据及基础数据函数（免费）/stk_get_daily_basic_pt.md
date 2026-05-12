stk_get_daily_basic_pt - 查询股本等基础指标单日截面数据（多标的）
查询指定日期截面上，股票的单日基础指标截面数据（point-in-time）

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_daily_basic_pt(symbols, fields, trade_date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
fields	str	返回字段	Y	无	指定需要返回的交易衍生指标， 如有多个字段，中间用英文逗号分隔
trade_date	str	查询日期	N	None	查询时间，时间类型为交易日期，%Y-%m-%d 格式， 默认None表示最新时间
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict]
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	
trade_date	str	交易日期	
fields	list[float]	指标字段数据	指定查询 fields字段的数值. 支持的字段名请参考 基础指标
示例：

stk_get_daily_basic_pt(symbols=['SZSE.000001', 'SZSE.300002'], fields='tclose,turnrate,ttl_shr',
                                  trade_date=None, df=True)
 
        复制成功
    
输出：

        symbol  trade_date  turnrate  tclose     ttl_shr
0  SZSE.000001  2023-06-27    0.2379   11.28  1.9406e+10
1  SZSE.300002  2023-06-27    7.3596   13.44  1.9611e+09
 
        复制成功
    
注意：

1. 如果fields参数的财务字段填写不正确，或填写空字段""，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

基础指标

字段名	类型	中文名称	量纲	说明
tclose	float	收盘价	元	
turnrate	float	当日换手率	%	
ttl_shr	float	总股本	股	
circ_shr	float	流通股本（流通股本=无限售条件流通股本+有限售条件流通股本）	股	
ttl_shr_unl	float	无限售条件流通股本(行情软件定义的流通股)	股	
ttl_shr_ltd	float	有限售条件股本	股	
a_shr_unl	float	无限售条件流通股本(A股)	股	
h_shr_unl	float	无限售条件流通股本(H股)	股	
