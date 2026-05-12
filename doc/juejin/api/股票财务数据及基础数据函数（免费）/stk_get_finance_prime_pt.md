stk_get_finance_prime_pt - 查询财务主要指标截面数据（多标的）
查询指定日期截面上，股票所属上市公司的财务主要指标数据（point-in-time）

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_finance_prime_pt(symbols, fields, rpt_type=None, data_type=None, date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
fields	str	返回字段	Y	无	指定需要返回的财务主要指标， 如有多个字段，中间用英文逗号分隔
rpt_type	int	报表类型	N	None	按报告期查询可指定以下报表类型：
1-一季度报
6-中报
9-前三季报
12-年报
默认None为不限
data_type	int	数据类型	N	None	在发布原始财务报告以后，上市公司可能会对数据进行修正。
100-合并最初（未修正的合并原始）
101-合并原始
102-合并调整
200-母公司最初（未修正的母公司原始）
201-母公司原始
202-母公司调整 默认None返回当期合并调整，如果没有调整返回合并原始，如果合并原始未修正返回合并最初
date	str	查询日期	N	None	查询时间，时间类型为发布日期，%Y-%m-%d 格式， 默认None表示最新时间
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict]
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	
pub_date	str	发布日期	距查询日期最近的发布日期
若数据类型选择合并原始(data_type=101)，则返回原始发布的发布日期
若数据类型选择合并调整(data_type=102)，则返回调整后最新发布日期
若数据类型选择母公司原始(data_type=201)，则返回母公司原始发布的发布日期
若数据类型选择母公司调整(data_type=202)，则返回母公司调整后最新发布日期
rpt_date	str	报告日期	报告截止日期，财报统计的最后一天，在指定时间段[开始时间,结束时间]内的报告截止日期
rpt_type	int	报表类型	返回数据的报表类型：1-一季度报, 6-中报, 9-前三季报, 12-年报
data_type	int	数据类型	返回数据的数据类型：100-合并最初（未修正的合并原始） 101-合并原始 102-合并调整 201-母公司原始 202-母公司调整 200-母公司最初（未修正的母公司原始）
fields	list[float]	财务字段数据	指定查询 fields字段的数值. 支持的字段名请参考 财务主要指标
示例：

stk_get_finance_prime_pt(symbols=['SZSE.000001', 'SZSE.300002'], fields='eps_basic,eps_dil', rpt_type=None, data_type=None, date='2023-06-19', df=True)
 
        复制成功
    
输出：

        symbol    pub_date    rpt_date  rpt_type  data_type  eps_dil  eps_basic
0  SZSE.000001  2023-04-25  2023-03-31         1        101   0.6500     0.6500
1  SZSE.300002  2023-04-27  2023-03-31         1        101   0.0914     0.0914
 
        复制成功
    
注意：

1. 为避免未来数据问题，指定查询日期date后，返回发布日期小于查询日期下的最新报告日期数据。

2. 如果fields参数的财务字段填写不正确，或填写空字段""，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

3. 财务主要指标由上市公司选择性公布，未公布则对应指标返回NaN. 如需获取标准公开的财务指标，请优先使用三大财务报表数据函数（stk_get_fundamentals_balance_pt - 查询资产负债表截面数据（多标的）、stk_get_fundamentals_cashflow_pt - 查询现金流量表截面数据（多标的）、stk_get_fundamentals_income_pt - 查询利润表截面数据（多标的））查询。

财务主要指标

字段名	类型	中文名称	量纲	说明
eps_basic	float	基本每股收益	元	
eps_dil	float	稀释每股收益	元	
eps_basic_cut	float	扣除非经常性损益后的基本每股收益	元	
eps_dil_cut	float	扣除非经常性损益后的稀释每股收益	元	
net_cf_oper_ps	float	每股经营活动产生的现金流量净额	元	
bps_pcom_ps	float	归属于母公司股东的每股净资产	元	
ttl_ast	float	总资产	元	
ttl_liab	float	总负债	元	
share_cptl	float	股本	股	
ttl_inc_oper	float	营业总收入	元	
inc_oper	float	营业收入	元	
oper_prof	float	营业利润	元	
ttl_prof	float	利润总额	元	
ttl_eqy_pcom	float	归属于母公司股东的所有者权益	元	
net_prof_pcom	float	归属于母公司股东的净利润	元	
net_prof_pcom_cut	float	扣除非经常性损益后归属于母公司股东的净利润	元	
roe	float	全面摊薄净资产收益率	%	
roe_weight_avg	float	加权平均净资产收益率	%	
roe_cut	float	扣除非经常性损益后的全面摊薄净资产收益率	%	
roe_weight_avg_cut	float	扣除非经常性损益后的加权平均净资产收益率	%	
net_cf_oper	float	经营活动产生的现金流量净额	元	
eps_yoy	float	每股收益同比比例	%	
inc_oper_yoy	float	营业收入同比比例	%	
ttl_inc_oper_yoy	float	营业总收入同比比例	%	
net_prof_pcom_yoy	float	归母净利润同比比例	%	
bps_sh	float	归属于普通股东的每股净资产	元	
net_asset	float	归属于普通股东的净资产	元	
net_prof	float	归属于普通股东的净利润	元	
net_prof_cut	float	扣除非经常性损益后归属于普通股股东的净利润	元	
