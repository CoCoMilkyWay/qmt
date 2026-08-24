stk_get_daily_valuation - 查询估值指标每日数据
查询指定时间段股票的每日估值指标

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_daily_valuation(symbol, fields, start_date=None, end_date=None, df=False)
 
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
fields	list[float]	指标字段数据	指定返回 fields字段的数值. 支持的字段名请参考 估值指标
示例：

stk_get_daily_valuation(symbol='SHSE.600000', fields='pe_ttm,pe_lyr,pe_mrq', start_date=None, end_date=None, df=True)
 
        复制成功
    
输出：

        symbol  trade_date  pe_ttm  pe_lyr  pe_mrq
0  SHSE.600000  2023-06-26  4.4139   4.107  3.3188
 
        复制成功
    
注意：

1. 当start_date == end_date时，取离 end_date 最近交易日期的一条数据，

当start_dat< end_date时，取指定时间段的数据，

当 start_date > end_date时，返回报错。

2. 如果fields参数的指标字段填写不正确，或填写空字段，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

估值指标

字段名	类型	中文名称	量纲	说明
pe_ttm	float	市盈率(TTM)	倍	
pe_lyr	float	市盈率(最新年报LYR)	倍	
pe_mrq	float	市盈率(最新报告期MRQ)	倍	
pe_1q	float	市盈率(当年一季×4)	倍	
pe_2q	float	市盈率(当年中报×2)	倍	
pe_3q	float	市盈率(当年三季×4/3)	倍	
pe_ttm_cut	float	市盈率(TTM) 扣除非经常性损益	倍	
pe_lyr_cut	float	市盈率(最新年报LYR) 扣除非经常性损益	倍	
pe_mrq_cut	float	市盈率(最新报告期MRQ) 扣除非经常性损益	倍	
pe_1q_cut	float	市盈率(当年一季×4) 扣除非经常性损益	倍	
pe_2q_cut	float	市盈率(当年中报×2) 扣除非经常性损益	倍	
pe_3q_cut	float	市盈率(当年三季×4/3) 扣除非经常性损益	倍	
pb_lyr	float	市净率(最新年报LYR)	倍	
pb_mrq	float	市净率(最新报告期MRQ)	倍	
pb_lyr_1	float	市净率(剔除其他权益工具，最新年报LYR)	倍	
pb_mrq_1	float	市净率(剔除其他权益工具，最新报告期MRQ)	倍	
pcf_ttm_oper	float	市现率(经营现金流,TTM)	倍	
pcf_ttm_ncf	float	市现率(现金净流量,TTM)	倍	
pcf_lyr_oper	float	市现率(经营现金流,最新年报LYR)	倍	
pcf_lyr_ncf	float	市现率(现金净流量,最新年报LYR)	倍	
ps_ttm	float	市销率(TTM)	倍	
ps_lyr	float	市销率(最新年报LYR)	倍	
ps_mrq	float	市销率(最新报告期MRQ)	倍	
ps_1q	float	市销率(当年一季×4)	倍	
ps_2q	float	市销率(当年中报×2)	倍	
ps_3q	float	市销率(当年三季×4/3)	倍	
peg_lyr	float	历史PEG值(当年年报增长率)		
peg_mrq	float	历史PEG值(最新报告期增长率)		
peg_1q	float	历史PEG值(当年1季*4较上年年报增长率)		
peg_2q	float	历史PEG值(当年中报*2较上年年报增长率)		
peg_3q	float	历史PEG值(当年3季*4/3较上年年报增长率)		
peg_np_cgr	float	历史PEG值(PE_TTM较净利润3年复合增长率)		
peg_npp_cgr	float	历史PEG值(PE_TTM较净利润3年复合增长率)		
dy_ttm	float	股息率(滚动 12 月TTM)	%	
dy_lfy	float	股息率(上一财年LFY)	%	
