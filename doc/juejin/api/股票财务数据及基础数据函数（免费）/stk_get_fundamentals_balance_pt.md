stk_get_fundamentals_balance_pt - 查询资产负债表截面数据（多标的）
查询指定日期截面的股票所属上市公司的资产负债表数据（point-in-time）

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_fundamentals_balance_pt(symbols, rpt_type=None, data_type=None, date=None, fields, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
fields	str	返回字段	Y	无	指定需要返回的财务字段， 如有多个字段，中间用英文逗号分隔
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
fields	list[float]	财务字段数据	指定查询 fields字段的数值. 支持的字段名请参考 资产负债表
示例：

stk_get_fundamentals_balance_pt(symbols='SHSE.600000, SZSE.000001', rpt_type=None, data_type=None, date='2022-10-01', fields='fix_ast', df=True)
 
        复制成功
    
输出：

        symbol    pub_date    rpt_date        fix_ast  data_type  rpt_type
0  SZSE.000001  2022-10-25  2022-09-30 10975000000.00        102         9
1  SHSE.600000  2022-10-29  2022-09-30 42563000000.00        102         9
 
        复制成功
    
注意：

1. 为避免未来数据问题，指定查询日期date后，返回发布日期小于查询日期下的最新报告日期数据。

2. 如果fields参数的财务字段填写不正确，或填写空字段""，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

资产负债表

字段名	类型	中文名称	量纲	说明
流动资产(资产)				
cash_bal_cb	float	现金及存放中央银行款项	元	银行
dpst_ob	float	存放同业款项	元	银行
mny_cptl	float	货币资金	元	
cust_cred_dpst	float	客户信用资金存款	元	证券
cust_dpst	float	客户资金存款	元	证券
pm	float	贵金属	元	银行
bal_clr	float	结算备付金	元	
cust_rsv	float	客户备付金	元	证券
ln_to_ob	float	拆出资金	元	
fair_val_fin_ast	float	以公允价值计量且其变动计入当期损益的金融资产	元	
ppay	float	预付款项	元	
fin_out	float	融出资金	元	
trd_fin_ast	float	交易性金融资产	元	
deriv_fin_ast	float	衍生金融资产	元	
note_acct_rcv	float	应收票据及应收账款	元	
note_rcv	float	应收票据	元	
acct_rcv	float	应收账款	元	
acct_rcv_fin	float	应收款项融资	元	
int_rcv	float	应收利息	元	
dvd_rcv	float	应收股利	元	
oth_rcv	float	其他应收款	元	
in_prem_rcv	float	应收保费	元	
rin_acct_rcv	float	应收分保账款	元	
rin_rsv_rcv	float	应收分保合同准备金	元	保险
rcv_un_prem_rin_rsv	float	应收分保未到期责任准备金	元	
rcv_clm_rin_rsv	float	应收分保未决赔偿准备金	元	保险
rcv_li_rin_rsv	float	应收分保寿险责任准备金	元	保险
rcv_lt_hi_rin_rsv	float	应收分保长期健康险责任准备金	元	保险
ph_plge_ln	float	保户质押贷款	元	保险
ttl_oth_rcv	float	其他应收款合计	元	
rfd_dpst	float	存出保证金	元	证券、保险
term_dpst	float	定期存款	元	保险
pur_resell_fin	float	买入返售金融资产	元	
aval_sale_fin	float	可供出售金融资产	元	
htm_inv	float	持有至到期投资	元	
hold_for_sale	float	持有待售资产	元	
acct_rcv_inv	float	应收款项类投资	元	保险
invt	float	存货	元	
contr_ast	float	合同资产	元	
ncur_ast_one_y	float	一年内到期的非流动资产	元	
oth_cur_ast	float	其他流动资产	元	
cur_ast_oth_item	float	流动资产其他项目	元	
ttl_cur_ast	float	流动资产合计	元	
非流动资产(资产)				
loan_adv	float	发放委托贷款及垫款	元	
cred_inv	float	债权投资	元	
oth_cred_inv	float	其他债权投资	元	
lt_rcv	float	长期应收款	元	
lt_eqy_inv	float	长期股权投资	元	
oth_eqy_inv	float	其他权益工具投资	元	
rfd_cap_guar_dpst	float	存出资本保证金	元	保险
oth_ncur_fin_ast	float	其他非流动金融资产	元	
amor_cos_fin_ast_ncur	float	以摊余成本计量的金融资产（非流动）	元	
fair_val_oth_inc_ncur	float	以公允价值计量且其变动计入其他综合收益的金融资产（非流动）	元	
inv_prop	float	投资性房地产	元	
fix_ast	float	固定资产	元	
const_prog	float	在建工程	元	
const_matl	float	工程物资	元	
fix_ast_dlpl	float	固定资产清理	元	
cptl_bio_ast	float	生产性生物资产	元	
oil_gas_ast	float	油气资产	元	
rig_ast	float	使用权资产	元	
intg_ast	float	无形资产	元	
trd_seat_fee	float	交易席位费	元	证券
dev_exp	float	开发支出	元	
gw	float	商誉	元	
lt_ppay_exp	float	长期待摊费用	元	
dfr_tax_ast	float	递延所得税资产	元	
oth_ncur_ast	float	其他非流动资产	元	
ncur_ast_oth_item	float	非流动资产其他项目	元	
ttl_ncur_ast	float	非流动资产合计	元	
oth_ast	float	其他资产	元	银行、证券、保险
ast_oth_item	float	资产其他项目	元	
ind_acct_ast	float	独立账户资产	元	保险
ttl_ast	float	资产总计	元	
流动负债(负债)				
brw_cb	float	向中央银行借款	元	
dpst_ob_fin_inst	float	同业和其他金融机构存放款项	元	银行、保险
ln_fm_ob	float	拆入资金	元	
fair_val_fin_liab	float	以公允价值计量且其变动计入当期损益的金融负债	元	
sht_ln	float	短期借款	元	
adv_acct	float	预收款项	元	
contr_liab	float	合同负债	元	
trd_fin_liab	float	交易性金融负债	元	
deriv_fin_liab	float	衍生金融负债	元	
sell_repo_ast	float	卖出回购金融资产款	元	
cust_bnk_dpst	float	吸收存款	元	银行、保险
dpst_cb_note_pay	float	存款证及应付票据	元	银行
dpst_cb	float	存款证	元	银行
acct_rcv_adv	float	预收账款	元	保险
in_prem_rcv_adv	float	预收保费	元	保险
fee_pay	float	应付手续费及佣金	元	
note_acct_pay	float	应付票据及应付账款	元	
stlf_pay	float	应付短期融资款	元	
note_pay	float	应付票据	元	
acct_pay	float	应付账款	元	
rin_acct_pay	float	应付分保账款	元	
emp_comp_pay	float	应付职工薪酬	元	
tax_pay	float	应交税费	元	
int_pay	float	应付利息	元	
dvd_pay	float	应付股利	元	
ph_dvd_pay	float	应付保单红利	元	保险
indem_pay	float	应付赔付款	元	保险
oth_pay	float	其他应付款	元	
ttl_oth_pay	float	其他应付款合计	元	
ph_dpst_inv	float	保户储金及投资款	元	保险
in_contr_rsv	float	保险合同准备金	元	保险
un_prem_rsv	float	未到期责任准备金	元	保险
clm_rin_rsv	float	未决赔款准备金	元	保险
li_liab_rsv	float	寿险责任准备金	元	保险
lt_hi_liab_rsv	float	长期健康险责任准备金	元	保险
cust_bnk_dpst_fin	float	吸收存款及同业存放	元	
inter_pay	float	内部应付款	元	
agy_secu_trd	float	代理买卖证券款	元	
agy_secu_uw	float	代理承销证券款	元	
sht_bnd_pay	float	应付短期债券	元	
est_cur_liab	float	预计流动负债	元	
liab_hold_for_sale	float	持有待售负债	元	
ncur_liab_one_y	float	一年内到期的非流动负债	元	
oth_cur_liab	float	其他流动负债	元	
cur_liab_oth_item	float	流动负债其他项目	元	
ttl_cur_liab	float	流动负债合计	元	
非流动负债（负债）				
lt_ln	float	长期借款	元	
lt_pay	float	长期应付款	元	
leas_liab	float	租赁负债		
dfr_inc	float	递延收益	元	
dfr_tax_liab	float	递延所得税负债	元	
bnd_pay	float	应付债券	元	
bnd_pay_pbd	float	其中:永续债	元	
bnd_pay_pfd	float	其中:优先股	元	
oth_ncur_liab	float	其他非流动负债	元	
spcl_pay	float	专项应付款	元	
ncur_liab_oth_item	float	非流动负债其他项目	元	
lt_emp_comp_pay	float	长期应付职工薪酬	元	
est_liab	float	预计负债	元	
oth_liab	float	其他负债	元	银行、证券、保险
liab_oth_item	float	负债其他项目	元	银行、证券、保险
ttl_ncur_liab	float	非流动负债合计	元	
ind_acct_liab	float	独立账户负债	元	保险
ttl_liab	float	负债合计	元	
所有者权益(或股东权益)				
paid_in_cptl	float	实收资本（或股本）	元	
oth_eqy	float	其他权益工具	元	
oth_eqy_pfd	float	其中:优先股	元	
oth_eqy_pbd	float	其中:永续债	元	
oth_eqy_oth	float	其中:其他权益工具	元	
cptl_rsv	float	资本公积	元	
treas_shr	float	库存股	元	
oth_comp_inc	float	其他综合收益	元	
spcl_rsv	float	专项储备	元	
sur_rsv	float	盈余公积	元	
rsv_ord_rsk	float	一般风险准备	元	
trd_risk_rsv	float	交易风险准备	元	证券
ret_prof	float	未分配利润	元	
sugg_dvd	float	建议分派股利	元	银行
eqy_pcom_oth_item	float	归属于母公司股东权益其他项目	元	
ttl_eqy_pcom	float	归属于母公司股东权益合计	元	
min_sheqy	float	少数股东权益	元	
sheqy_oth_item	float	股东权益其他项目	元	
ttl_eqy	float	股东权益合计	元	
ttl_liab_eqy	float	负债和股东权益合计	元	
