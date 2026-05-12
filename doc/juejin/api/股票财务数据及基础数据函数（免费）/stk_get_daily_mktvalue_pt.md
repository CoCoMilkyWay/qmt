stk_get_daily_mktvalue_pt - 查询市值指标单日截面数据（多标的）
查询指定日期截面上，股票的单日市值截面数据（point-in-time）

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_daily_mktvalue_pt(symbols, fields, trade_date=None, df=False)
 
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
fields	list[float]	指标字段数据	指定查询 fields字段的数值. 支持的字段名请参考 市值指标
示例：

stk_get_daily_mktvalue_pt(symbols=['SZSE.000001', 'SZSE.300002'], fields='tot_mv,tot_mv_csrc,a_mv',
                               trade_date=None, df=True)
 
        复制成功
    
输出：

        symbol  trade_date        a_mv      tot_mv  tot_mv_csrc
0  SZSE.000001  2023-06-26  2.1696e+11  2.1696e+11   2.1696e+11
1  SZSE.300002  2023-06-26  2.5828e+10  2.5828e+10   2.5828e+10
 
        复制成功
    
注意：

1. 如果fields参数的财务字段填写不正确，或填写空字段""，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

市值指标

字段名	类型	中文名称	量纲	说明
tot_mv	float	总市值	元	
tot_mv_csrc	float	总市值(证监会算法)	元	
a_mv	float	A股流通市值(含限售股)	元	
a_mv_ex_ltd	float	A股流通市值(不含限售股)	元	
b_mv	float	B股流通市值(含限售股，折人民币)	元	
b_mv_ex_ltd	float	B股流通市值(不含限售股，折人民币)	元	
ev	float	企业价值(含货币资金)(EV1)	元	
ev_ex_curr	float	企业价值(剔除货币资金)(EV2)	元	
ev_ebitda	float	企业倍数	倍	
equity_value	float	股权价值	元	
