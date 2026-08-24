stk_get_fundamentals_income - 查询利润表数据
查询指定时间段某一股票所属上市公司的利润表数据

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_fundamentals_income(symbol, rpt_type=None, data_type=None, start_date=None, end_date=None, fields, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	股票代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
fields	str	返回字段	Y	无	指定需要返回的财务字段， 如有多个字段，中间用英文逗号分隔
rpt_type	int	报表类型	N	None	按报告期查询可指定以下报表类型：
1-一季度报
6-中报
9-前三季报
12-年报
默认None为不限
data_type	int	数据类型	N	None	在发布原始财务报告以后，上市公司可能会对数据进行修正。
101-合并原始
102-合并调整
201-母公司原始
202-母公司调整 默认None返回当期合并调整，如果没有调整返回合并原始
start_date	str	开始时间	N	None	开始时间，时间类型为报告日期，%Y-%m-%d 格式， 默认None表示最新时间
end_date	str	结束时间	N	None	结束时间，时间类型为报告日期，%Y-%m-%d 格式， 默认None表示最新时间
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict]
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	
pub_date	str	发布日期	若数据类型选择合并原始(data_type=101)，则返回原始发布的发布日期 若数据类型选择合并调整(data_type=102)，则返回调整后最新发布日期 若数据类型选择母公司原始(data_type=201)，则返回母公司原始发布的发布日期
若数据类型选择母公司调整(data_type=202)，则返回母公司调整后最新发布日期
rpt_date	str	报告日期	报告截止日期，财报统计的最后一天，在指定时间段[开始时间,结束时间]内的报告截止日期
rpt_type	int	报表类型	返回数据的报表类型：1-一季度报, 6-中报, 9-前三季报, 12-年报
data_type	int	数据类型	返回数据的数据类型：101-合并原始, 102-合并调整, 201-母公司原始, 202-母公司调整
fields	list[float]	财务字段数据	指定返回 fields字段的数值. 支持的字段名请参考 利润表
示例：

stk_get_fundamentals_income(symbol='SHSE.600000', rpt_type=6, data_type=None, start_date='2022-12-31', end_date='2022-12-31', fields='inc_oper', df=True)
 
        复制成功
    
输出：

        symbol    pub_date    rpt_date  rpt_type  data_type       inc_oper
0  SHSE.600000  2022-08-27  2022-06-30         6        102 98644000000.00
 
        复制成功
    
注意：

1. 当start_date == end_date时，取离 end_date 最近报告日期的一条数据，

当start_dat< end_date时，取指定时间段的数据，

当 start_date > end_date时，返回报错。

2. 若在指定历史时间段内，有多个同一类型报表（如不同年份的一季度报表），将按照报告日期顺序返回。

3. 如果fields参数的财务字段填写不正确，或填写空字段，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

利润表

字段名	类型	中文名称	量纲	说明
ttl_inc_oper	float	营业总收入	元	
inc_oper	float	营业收入	元	
net_inc_int	float	利息净收入	元	证券、银行、保险
exp_int	float	利息支出	元	
net_inc_fee_comm	float	手续费及佣金净收入	元	证券、银行
inc_rin_prem	float	其中：分保费收入	元	保险
net_inc_secu_agy	float	其中:代理买卖证券业务净收入	元	证券
inc_fee_comm	float	手续费及佣金收入	元	
in_prem_earn	float	已赚保费	元	保险
inc_in_biz	float	其中:保险业务收入	元	保险
rin_prem_cede	float	分出保费	元	保险
unear_prem_rsv	float	提取未到期责任准备金	元	保险
net_inc_uw	float	证券承销业务净收入	元	证券
net_inc_cust_ast_mgmt	float	受托客户资产管理业务净收入	元	证券
inc_fx	float	汇兑收益	元	
inc_other_oper	float	其他业务收入	元	
inc_oper_balance	float	营业收入平衡项目	元	
ttl_inc_oper_other	float	营业总收入其他项目	元	
ttl_cost_oper	float	营业总成本	元	
cost_oper	float	营业成本	元	
exp_oper	float	营业支出	元	证券、银行、保险
biz_tax_sur	float	营业税金及附加	元	
exp_sell	float	销售费用	元	
exp_adm	float	管理费用	元	
exp_rd	float	研发费用	元	
exp_fin	float	财务费用	元	
int_fee	float	其中:利息费用	元	
inc_int	float	利息收入	元	
exp_oper_adm	float	业务及管理费	元	证券、银行、保险
exp_rin	float	减:摊回分保费用	元	保险
rfd_prem	float	退保金	元	保险
comp_pay	float	赔付支出	元	保险
rin_clm_pay	float	减:摊回赔付支出	元	保险
draw_insur_liab	float	提取保险责任准备金	元	保险
amor_insur_liab	float	减:摊回保险责任准备金	元	保险
exp_ph_dvd	float	保单红利支出	元	保险
exp_fee_comm	float	手续费及佣金支出	元	
other_oper_cost	float	其他业务成本	元	
oper_exp_balance	float	营业支出平衡项目	元	证券、银行、保险
exp_oper_other	float	营业支出其他项目	元	证券、银行、保险
ttl_cost_oper_other	float	营业总成本其他项目	元	
其他经营收益			元	
inc_inv	float	投资收益	元	
inv_inv_jv_p	float	对联营企业和合营企业的投资收益	元	
inc_ast_dspl	float	资产处置收益	元	
ast_impr_loss	float	资产减值损失(新)	元	
cred_impr_loss	float	信用减值损失(新)	元	
inc_fv_chg	float	公允价值变动收益	元	
inc_other	float	其他收益	元	
oper_prof_balance	float	营业利润平衡项目	元	
oper_prof	float	营业利润	元	
inc_noper	float	营业外收入	元	
exp_noper	float	营业外支出	元	
ttl_prof_balance	float	利润总额平衡项目	元	
oper_prof_other	float	营业利润其他项目	元	
ttl_prof	float	利润总额	元	
inc_tax	float	所得税费用	元	
net_prof	float	净利润	元	
oper_net_prof	float	持续经营净利润	元	
net_prof_pcom	float	归属于母公司股东的净利润	元	
min_int_inc	float	少数股东损益	元	
end_net_prof	float	终止经营净利润	元	
net_prof_other	float	净利润其他项目	元	
eps_base	float	基本每股收益	元	
eps_dil	float	稀释每股收益	元	
other_comp_inc	float	其他综合收益	元	
other_comp_inc_pcom	float	归属于母公司股东的其他综合收益	元	
other_comp_inc_min	float	归属于少数股东的其他综合收益	元	
ttl_comp_inc	float	综合收益总额	元	
ttl_comp_inc_pcom	float	归属于母公司所有者的综合收益总额	元	
ttl_comp_inc_min	float	归属于少数股东的综合收益总额	元	
prof_pre_merge	float	被合并方在合并前实现利润	元	
net_rsv_in_contr	float	提取保险合同准备金净额	元	
net_pay_comp	float	赔付支出净额	元	
net_loss_ncur_ast	float	非流动资产处置净损失	元	
amod_fin_asst_end	float	以摊余成本计量的金融资产终止确认收益	元	
cash_flow_hedging_pl	float	现金流量套期损益的有效部分	元	
cur_trans_diff	float	外币财务报表折算差额	元	
gain_ncur_ast	float	非流动资产处置利得	元	
afs_fv_chg_pl	float	可供出售金融资产公允价值变动损益	元	
oth_eqy_inv_fv_chg	float	其他权益工具投资公允价值变动	元	
oth_debt_inv_fv_chg	float	其他债权投资公允价值变动	元	
oth_debt_inv_cred_impr	float	其他债权投资信用减值准备	元	
