stk_get_fundamentals_cashflow - 查询现金流量表数据
查询指定时间段某一股票所属上市公司的现金流量表数据

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_fundamentals_cashflow(symbol, rpt_type=None, data_type=None, start_date=None, end_date=None, fields, df=False)
 
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
fields	list[float]	财务字段数据	指定返回 fields字段的数值. 支持的字段名请参考 现金流量表
示例：

stk_get_fundamentals_cashflow(symbol='SHSE.600000', rpt_type=None, data_type=101, start_date='2022-12-31', end_date='2022-12-31', fields='cash_pay_fee', df=True)
 
        复制成功
    
输出：

        symbol    pub_date    rpt_date  rpt_type  data_type  cash_pay_fee
0  SHSE.600000  2022-10-29  2022-09-30         9        101 7261000000.00
 
        复制成功
    
注意：

1. 当start_date == end_date时，取离 end_date 最近报告日期的一条数据，

当start_dat< end_date时，取指定时间段的数据，

当 start_date > end_date时，返回报错。

2. 若在指定历史时间段内，有多个同一类型报表（如不同年份的一季度报表），将按照报告日期顺序返回。

3. 如果fields参数的财务字段填写不正确，或填写空字段，会报错提示“填写的 fields 不正确”。fields不能超过20个字段

现金流量表

字段名	类型	中文名称	量纲	说明
一、经营活动产生的现金流量				
cash_rcv_sale	float	销售商品、提供劳务收到的现金	元	
net_incr_cust_dpst_ob	float	客户存款和同业存放款项净增加额	元	
net_incr_cust_dpst	float	客户存款净增加额	元	银行
net_incr_dpst_ob	float	同业及其他金融机构存放款项净增加额	元	银行
net_incr_brw_cb	float	向中央银行借款净增加额	元	
net_incr_ln_fm_oth	float	向其他金融机构拆入资金净增加额	元	
cash_rcv_orig_in	float	收到原保险合同保费取得的现金	元	
net_cash_rcv_rin_biz	float	收到再保险业务现金净额	元	
net_incr_ph_dpst_inv	float	保户储金及投资款净增加额	元	
net_decrdpst_cb_ob	float	存放中央银行和同业款项及其他金融机构净减少额	元	银行、保险
net_decr_cb	float	存放中央银行款项净减少额	元	银行
net_decr_ob_fin_inst	float	存放同业及其他金融机构款项净减少额	元	银行
net_cert_dpst	float	存款证净额	元	银行
net_decr_trd_fin	float	交易性金融资产净减少额	元	银行
net_incr_trd_liab	float	交易性金融负债净增加额	元	银行
cash_rcv_int_fee	float	收取利息、手续费及佣金的现金	元	
cash_rcv_int	float	其中：收取利息的现金	元	银行
cash_rcv_fee	float	收取手续费及佣金的现金	元	银行
net_incr_lnfm_sell_repo	float	拆入资金及卖出回购金融资产款净增加额	元	银行
net_incr_ln_fm	float	拆入资金净增加额	元	
net_incr_sell_repo	float	卖出回购金融资产款净增加额	元	银行 保险
net_decr_lnto_pur_resell	float	拆出资金及买入返售金融资产净减少额	元	银行
net_decr_ln_cptl	float	拆出资金净减少额	元	银行、保险
net_dect_pur_resell	float	买入返售金融资产净减少额	元	银行、保险
net_incr_repo	float	回购业务资金净增加额	元	
net_decr_repo	float	回购业务资金净减少额	元	证券
tax_rbt_rcv	float	收到的税费返还	元	
net_cash_rcv_trd	float	收到交易性金融资产现金净额	元	保险
cash_rcv_oth_oper	float	收到其他与经营活动有关的现金	元	
net_cash_agy_secu_trd	float	代理买卖证券收到的现金净额	元	证券
cash_rcv_pur_resell	float	买入返售金融资产收到的现金	元	证券、保险
net_cash_agy_secu_uw	float	代理承销证券收到的现金净额	元	证券
cash_rcv_dspl_debt	float	处置抵债资产收到的现金	元	银行
canc_loan_rcv	float	收回的已于以前年度核销的贷款	元	银行
cf_in_oper	float	经营活动现金流入小计	元	
cash_pur_gds_svc	float	购买商品、接受劳务支付的现金	元	
net_incr_ln_adv_cust	float	客户贷款及垫款净增加额	元	
net_decr_brw_cb	float	向中央银行借款净减少额	元	银行
net_incr_dpst_cb_ob	float	存放中央银行和同业款项净增加额	元	
net_incr_cb	float	存放中央银行款项净增加额	元	银行
net_incr_ob_fin_inst	float	存放同业及其他金融机构款项净增加额	元	银行
net_decr_dpst_ob	float	同业及其他机构存放款减少净额	元	银行
net_decr_issu_cert_dpst	float	已发行存款证净减少额	元	银行
net_incr_lnto_pur_resell	float	拆出资金及买入返售金融资产净增加额	元	银行
net_incr_ln_to	float	拆出资金净增加额	元	银行、保险
net_incr_pur_resell	float	买入返售金融资产净增加额	元	银行、保险
net_decr_lnfm_sell_repo	float	拆入资金及卖出回购金融资产款净减少额	元	银行
net_decr_ln_fm	float	拆入资金净减少额	元	银行、证券
net_decr_sell_repo	float	卖出回购金融资产净减少额	元	银行、保险
net_incr_trd_fin	float	交易性金融资产净增加额	元	银行
net_decr_trd_liab	float	交易性金融负债净减少额	元	银行
cash_pay_indem_orig	float	支付原保险合同赔付款项的现金	元	
net_cash_pay_rin_biz	float	支付再保险业务现金净额	元	保险
cash_pay_int_fee	float	支付利息、手续费及佣金的现金	元	
cash_pay_int	float	其中：支付利息的现金	元	银行
cash_pay_fee	float	支付手续费及佣金的现金	元	银行
ph_dvd_pay	float	支付保单红利的现金	元	
net_decr_ph_dpst_inv	float	保户储金及投资款净减少额	元	保险
cash_pay_emp	float	支付给职工以及为职工支付的现金		
cash_pay_tax	float	支付的各项税费	元	
net_cash_pay_trd	float	支付交易性金融资产现金净额	元	保险
cash_pay_oth_oper	float	支付其他与经营活动有关的现金	元	
net_incr_dspl_trd_fin	float	处置交易性金融资产净增加额	元	
cash_pay_fin_leas	float	购买融资租赁资产支付的现金	元	银行
net_decr_agy_secu_pay	float	代理买卖证券支付的现金净额（净减少额）	元	证券
net_decr_dspl_trd_fin	float	处置交易性金融资产的净减少额	元	证券
cf_out_oper	float	经营活动现金流出小计	元	
net_cf_oper	float	经营活动产生的现金流量净额	元	
二、投资活动产生的现金流量：				
cash_rcv_sale_inv	float	收回投资收到的现金	元	
inv_inc_rcv	float	取得投资收益收到的现金	元	
cash_rcv_dvd_prof	float	分得股利或利润所收到的现金	元	银行
cash_rcv_dspl_ast	float	处置固定资产、无形资产和其他长期资产收回的现金净额	元	
cash_rcv_dspl_sub_oth	float	处置子公司及其他营业单位收到的现金净额	元	
cash_rcv_oth_inv	float	收到其他与投资活动有关的现金	元	
cf_in_inv	float	投资活动现金流入小计	元	
pur_fix_intg_ast	float	购建固定资产、无形资产和其他长期资产支付的现金	元	
cash_out_dspl_sub_oth	float	处置子公司及其他营业单位流出的现金净额	元	保险
cash_pay_inv	float	投资支付的现金	元	
net_incr_ph_plge_ln	float	保户质押贷款净增加额	元	保险
add_cash_pled_dpst	float	增加质押和定期存款所支付的现金	元	
net_incr_plge_ln	float	质押贷款净增加额	元	
net_cash_get_sub	float	取得子公司及其他营业单位支付的现金净额	元	
net_pay_pur_resell	float	支付买入返售金融资产现金净额	元	证券、保险
cash_pay_oth_inv	float	支付其他与投资活动有关的现金	元	
cf_out_inv	float	投资活动现金流出小计		
net_cf_inv	float	投资活动产生的现金流量净额	元	
三、筹资活动产生的现金流量：				
cash_rcv_cptl	float	吸收投资收到的现金	元	
sub_rcv_ms_inv	float	其中：子公司吸收少数股东投资收到的现金	元	
brw_rcv	float	取得借款收到的现金	元	
cash_rcv_bnd_iss	float	发行债券收到的现金	元	
net_cash_rcv_sell_repo	float	收到卖出回购金融资产款现金净额	元	保险
cash_rcv_oth_fin	float	收到其他与筹资活动有关的现金	元	
issu_cert_dpst	float	发行存款证	元	银行
cf_in_fin_oth	float	筹资活动现金流入其他项目	元	
cf_in_fin	float	筹资活动现金流入小计	元	
cash_rpay_brw	float	偿还债务支付的现金	元	
cash_pay_bnd_int	float	偿付债券利息支付的现金	元	银行
cash_pay_dvd_int	float	分配股利、利润或偿付利息支付的现金	元	
sub_pay_dvd_prof	float	其中：子公司支付给少数股东的股利、利润	元	
cash_pay_oth_fin	float	支付其他与筹资活动有关的现金	元	
net_cash_pay_sell_repo	float	支付卖出回购金融资产款现金净额	元	保险
cf_out_fin	float	筹资活动现金流出小计	元	
net_cf_fin	float	筹资活动产生的现金流量净额	元	
efct_er_chg_cash	float	四、汇率变动对现金及现金等价物的影响	元	
net_incr_cash_eq	float	五、现金及现金等价物净增加额	元	
cash_cash_eq_bgn	float	加：期初现金及现金等价物余额	元	
cash_cash_eq_end	float	六、期末现金及现金等价物余额	元	
补充资料 1．将净利润调节为经营活动现金流量：				
net_prof	float	净利润	元	
ast_impr	float	资产减值准备	元	
accr_prvs_ln_impa	float	计提贷款减值准备	元	银行
accr_prvs_oth_impa	float	计提其他资产减值准备	元	银行
accr_prem_rsv	float	提取的保险责任准备金	元	保险
accr_unearn_prem_rsv	float	提取的未到期的责任准备金	元	保险
defr_fix_prop	float	固定资产和投资性房地产折旧	元	
depr_oga_cba	float	其中:固定资产折旧、油气资产折耗、生产性生物资产折旧	元	
amor_intg_ast_lt_exp	float	无形资产及长期待摊费用等摊销	元	银行、证券、保险
amort_intg_ast	float	无形资产摊销	元	
amort_lt_exp_ppay	float	长期待摊费用摊销	元	
dspl_ast_loss	float	处置固定资产、无形资产和其他长期资产的损失	元	
fair_val_chg_loss	float	固定资产报废损失	元	
fv_chg_loss	float	公允价值变动损失	元	
dfa	float	固定资产折旧	元	银行
fin_exp	float	财务费用	元	
inv_loss	float	投资损失	元	
exchg_loss	float	汇兑损失	元	银行、证券、保险
dest_incr	float	存款的增加	元	银行
loan_decr	float	贷款的减少	元	银行
cash_pay_bnd_int_iss	float	发行债券利息支出	元	银行
dfr_tax	float	递延所得税	元	
dfr_tax_ast_decr	float	其中:递延所得税资产减少	元	
dfr_tax_liab_incr	float	递延所得税负债增加	元	
invt_decr	float	存货的减少	元	
decr_rcv_oper	float	经营性应收项目的减少	元	
incr_pay_oper	float	经营性应付项目的增加	元	
oth	float	其他	元	
cash_end	float	现金的期末余额	元	
cash_bgn	float	减：现金的期初余额	元	
cash_eq_end	float	加:现金等价物的期末余额	元	
cash_eq_bgn	float	减:现金等价物的期初余额	元	
cred_impr_loss	float	信用减值损失	元	
est_liab_add	float	预计负债的增加	元	
dr_cnv_cptl	float	债务转为资本	元	
cptl_bnd_expr_one_y	float	一年内到期的可转换公司债券	元	
fin_ls_fix_ast	float	融资租入固定资产	元	
amort_dfr_inc	float	递延收益摊销	元	
depr_inv_prop	float	投资性房地产折旧	元	
trd_fin_decr	float	交易性金融资产的减少	元	证券、保险
im_net_cf_oper	float	间接法-经营活动产生的现金流量净额	元	
im_net_incr_cash_eq	float	间接法-现金及现金等价物净增加额	元	
