stk_get_finance_deriv_pt - 查询财务衍生指标截面数据（多标的）
查询指定日期截面上，股票所属上市公司的财务衍生指标数据（point-in-time）

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_finance_deriv_pt(symbols, fields, rpt_type=None, data_type=None, date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
fields	str	返回字段	Y	无	指定需要返回的财务衍生指标， 如有多个字段，中间用英文逗号分隔
rpt_type	int	报表类型	N	None	按报告期查询可指定以下报表类型：
1-一季度报
6-中报
9-前三季报
12-年报
默认None为不限
data_type	int	数据类型	N	None	在发布原始财务报告以后，上市公司可能会对数据进行修正。 101-合并原始
102-合并调整 201-母公司原始
202-母公司调整 默认None返回当期合并调整，如果没有调整返回合并原始
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
data_type	int	数据类型	返回数据的数据类型：101-合并原始, 102-合并调整, 201-母公司原始, 202-母公司调整
fields	list[float]	财务字段数据	指定查询 fields字段的数值. 支持的字段名请参考 财务衍生指标指标
示例：

stk_get_finance_deriv_pt(symbols=['SZSE.000001', 'SZSE.300002'], fields='eps_basic,eps_dil2',
                                   rpt_type=None, data_type=None, date='2023-06-19', df=True)
 
        复制成功
    
输出：

        symbol    pub_date    rpt_date  ...  data_type  eps_basic  eps_dil2
0  SZSE.000001  2023-04-25  2023-03-31  ...        102     0.6500    0.6500
1  SZSE.300002  2023-04-27  2023-03-31  ...        102     0.0914    0.0914
 
        复制成功
    
注意：

1. 为避免未来数据问题，指定查询日期date后，返回发布日期小于查询日期下的最新报告日期数据。

2. 如果fields参数的财务字段填写不正确，或填写空字段""，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

财务衍生指标指标

字段名	类型	中文名称	量纲	说明
eps_basic	float	每股收益EPS(基本)	元	
eps_dil2	float	每股收益EPS(稀释)	元	
eps_dil	float	每股收益EPS(期末股本摊薄)	元	
eps_basic_cut	float	每股收益EPS(扣除/基本)	元	
eps_dil2_cut	float	每股收益EPS(扣除/稀释)	元	
eps_dil_cut	float	每股收益EPS(扣除/期末股本摊薄)	元	
bps	float	每股净资产BPS	元	
net_cf_oper_ps	float	每股经营活动产生的现金流量净额	元	
ttl_inc_oper_ps	float	每股营业总收入	元	
inc_oper_ps	float	每股营业收入	元	
ebit_ps	float	每股息税前利润	元	
cptl_rsv_ps	float	每股资本公积	元	
sur_rsv_ps	float	每股盈余公积	元	
retain_prof_ps	float	每股未分配利润	元	
retain_inc_ps	float	每股留存收益	元	
net_cf_ps	float	每股现金流量净额	元	
fcff_ps	float	每股企业自由现金流量	元	
fcfe_ps	float	每股股东自由现金流量	元	
ebitda_ps	float	每股EBITDA	元	
roe	float	净资产收益率ROE(摊薄)	%	
roe_weight	float	净资产收益率ROE(加权)	%	
roe_avg	float	净资产收益率ROE(平均)	%	
roe_cut	float	净资产收益率ROE(扣除/摊薄)	%	
roe_weight_cut	float	净资产收益率ROE(扣除/加权)	%	
ocf_toi	float	经营性现金净流量/营业总收入		
eps_dil_yoy	float	稀释每股收益同比增长率	%	
net_cf_oper_ps_yoy	float	每股经营活动中产生的现金流量净额同比增长率	%	
ttl_inc_oper_yoy	float	营业总收入同比增长率	%	
inc_oper_yoy	float	营业收入同比增长率	%	
oper_prof_yoy	float	营业利润同比增长率	%	
ttl_prof_yoy	float	利润总额同比增长率	%	
net_prof_pcom_yoy	float	归属母公司股东的净利润同比增长率	%	
net_prof_pcom_cut_yoy	float	归属母公司股东的净利润同比增长率(扣除非经常性损益)	%	
net_cf_oper_yoy	float	经营活动产生的现金流量净额同比增长率	%	
roe_yoy	float	净资产收益率同比增长率(摊薄)	%	
net_asset_yoy	float	净资产同比增长率	%	
ttl_liab_yoy	float	总负债同比增长率	%	
ttl_asset_yoy	float	总资产同比增长率	%	
net_cash_flow_yoy	float	现金净流量同比增长率	%	
bps_gr_begin_year	float	每股净资产相对年初增长率	%	
ttl_asset_gr_begin_year	float	资产总计相对年初增长率	%	
ttl_eqy_pcom_gr_begin_year	float	归属母公司的股东权益相对年初增长率	%	
net_debt_eqy_ev	float	净债务/股权价值	%	
int_debt_eqy_ev	float	带息债务/股权价值		
eps_bas_yoy	float	基本每股收益同比增长率	%	
ebit	float	EBIT(正推法)	元	
ebitda	float	EBITDA(正推法)	元	
ebit_inverse	float	EBIT(反推法)	元	
ebitda_inverse	float	EBITDA(反推法)	元	
nr_prof_loss	float	非经常性损益	元	
net_prof_cut	float	扣除非经常性损益后的净利润	元	
gross_prof	float	毛利润	元	
oper_net_inc	float	经营活动净收益	元	
val_chg_net_inc	float	价值变动净收益	元	
exp_rd	float	研发费用	元	
ttl_inv_cptl	float	全部投入资本	元	
work_cptl	float	营运资本	元	
net_work_cptl	float	净营运资本	元	
tg_asset	float	有形资产	元	
retain_inc	float	留存收益	元	
int_debt	float	带息债务	元	
net_debt	float	净债务	元	
curr_liab_non_int	float	无息流动负债	元	
ncur_liab_non_int	float	无息非流动负债	元	
fcff	float	企业自由现金流量FCFF	元	
fcfe	float	股权自由现金流量FCFE	元	
cur_depr_amort	float	当期计提折旧与摊销	元	
eqy_mult_dupont	float	权益乘数(杜邦分析)		
net_prof_pcom_np	float	归属母公司股东的净利润/净利润	%	
net_prof_tp	float	净利润/利润总额	%	
ttl_prof_ebit	float	利润总额/息税前利润	%	
roe_cut_avg	float	净资产收益率ROE(扣除/平均)	%	
roe_add	float	净资产收益率ROE(增发条件)	%	
roe_ann	float	净资产收益率ROE(年化)	%	
roa	float	总资产报酬率ROA	%	
roa_ann	float	总资产报酬率ROA(年化)	%	
jroa	float	总资产净利率	%	
jroa_ann	float	总资产净利率(年化)	%	
roic	float	投入资本回报率ROIC	%	
sale_npm	float	销售净利率	%	
sale_gpm	float	销售毛利率	%	
sale_cost_rate	float	销售成本率	%	
sale_exp_rate	float	销售期间费用率	%	
net_prof_toi	float	净利润/营业总收入	%	
oper_prof_toi	float	营业利润/营业总收入	%	
ebit_toi	float	息税前利润/营业总收入	%	
ttl_cost_oper_toi	float	营业总成本/营业总收入	%	
exp_oper_toi	float	营业费用/营业总收入	%	
exp_admin_toi	float	管理费用/营业总收入	%	
exp_fin_toi	float	财务费用/营业总收入	%	
ast_impr_loss_toi	float	资产减值损失/营业总收入	%	
ebitda_toi	float	EBITDA/营业总收入	%	
oper_net_inc_tp	float	经营活动净收益/利润总额	%	
val_chg_net_inc_tp	float	价值变动净收益/利润总额	%	
net_exp_noper_tp	float	营业外支出净额/利润总额		
inc_tax_tp	float	所得税/利润总额	%	
net_prof_cut_np	float	扣除非经常性损益的净利润/净利润	%	
eqy_mult	float	权益乘数		
curr_ast_ta	float	流动资产/总资产	%	
ncurr_ast_ta	float	非流动资产/总资产	%	
tg_ast_ta	float	有形资产/总资产	%	
ttl_eqy_pcom_tic	float	归属母公司股东的权益/全部投入资本	%	
int_debt_tic	float	带息负债/全部投入资本	%	
curr_liab_tl	float	流动负债/负债合计	%	
ncurr_liab_tl	float	非流动负债/负债合计	%	
ast_liab_rate	float	资产负债率	%	
quick_rate	float	速动比率		
curr_rate	float	流动比率		
cons_quick_rate	float	保守速动比率		
liab_eqy_rate	float	产权比率		
ttl_eqy_pcom_tl	float	归属母公司股东的权益/负债合计		
ttl_eqy_pcom_debt	float	归属母公司股东的权益/带息债务		
tg_ast_tl	float	有形资产/负债合计		
tg_ast_int_debt	float	有形资产/带息债务		
tg_ast_net_debt	float	有形资产/净债务		
ebitda_tl	float	息税折旧摊销前利润/负债合计		
net_cf_oper_tl	float	经营活动产生的现金流量净额/负债合计		
net_cf_oper_int_debt	float	经营活动产生的现金流量净额/带息债务		
net_cf_oper_curr_liab	float	经营活动产生的现金流量净额/流动负债		
net_cf_oper_net_liab	float	经营活动产生的现金流量净额/净债务		
ebit_int_cover	float	已获利息倍数		
long_liab_work_cptl	float	长期债务与营运资金比率		
ebitda_int_debt	float	EBITDA/带息债务	%	
oper_cycle	float	营业周期	天	
inv_turnover_days	float	存货周转天数	天	
acct_rcv_turnover_days	float	应收账款周转天数(含应收票据)	天	
inv_turnover_rate	float	存货周转率	次	
acct_rcv_turnover_rate	float	应收账款周转率(含应收票据)	次	
curr_ast_turnover_rate	float	流动资产周转率	次	
fix_ast_turnover_rate	float	固定资产周转率	次	
ttl_ast_turnover_rate	float	总资产周转率	次	
cash_rcv_sale_oi	float	销售商品提供劳务收到的现金/营业收入	%	
net_cf_oper_oi	float	经营活动产生的现金流量净额/营业收入	%	
net_cf_oper_oni	float	经营活动产生的现金流量净额/经营活动净收益		
cptl_exp_da	float	资本支出/折旧摊销	%	
cash_rate	float	现金比率		
acct_pay_turnover_days	float	应付账款周转天数(含应付票据)	天	
acct_pay_turnover_rate	float	应付账款周转率(含应付票据)	次	
net_oper_cycle	float	净营业周期	天	
ttl_cost_oper_yoy	float	营业总成本同比增长率	%	
net_prof_yoy	float	净利润同比增长率	%	
net_cf_oper_np	float	经营活动产生的现金流量净额/净利润	%	
