bank_indicator银行业

历史范围：2005年至今；更新时间：交易日24:00更新

get_fundamentals(query_object, date=None, statDate=None)

描述

查询bank_indicator银行业

按年度更新，统计周期是一年度。 通过get_fundamentals(query_object,statDate=None) statDate传入年查询。当传入 date 参数 或 statDate 传入季度时返回空。

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

bank_indicator银行业

列名	列的含义	解释

code	股票代码	带后缀.XSHE/.XSHG

pubDate	日期	公司发布财报日期

statDate	日期	财报统计的季度的最后一天, 比如2016-12-31

total_loan	贷款总额	银行发放的贷款总额

total_deposit	存款总额	银行的存款总额

interest_earning_assets	生息资产	生息资产是指贷款、投资等业务形式上的资产，能为银行的经营带来收入

non_interest_earning_assets	非生息资产	非生息资产

interest_earning_assets_yield	生息资产收益率	生息资产收益率

interest_bearing_liabilities	计息负债	计息负债指银行负债当中需要支付利息的债务

non_interest_bearing_liabilities	非计息负债	非计息负债

interest_bearing_liabilities_interest_rate	计息负债成本率	计息负债成本率

non_interest_income	非利息收入	非利息收入

non_interest_income_ratio	非利息收入占比	非利息收入占比为非利息收入占全部收入的比例

net_interest_margin	净息差	净息差指的是银行净利息收入和银行全部生息资产的比值

net_profit_margin	净利差	净利差是指平均生息资产收益率与平均计息负债成本率之差

core_level_capital	核心一级资本(2013)	核心一级资本

net_core_level_capital	核心一级资本净额(2013)	核心一级资本净额

core_level_capital_adequacy_ratio	核心一级资本充足率(2013)	核心一级资本充足率

net_level_1_capital	一级资本净额(2013)	一级资本净额

level_1_capital_adequacy_ratio	一级资本充足率(2013)	一级资本充足率

net_capital	资本净额(2013)	资本净额为核心资本加上附属资本减去扣减项

capital_adequacy_ratio	资本充足率（2013）	资本充足率是一个银行的资产对其风险的比率

weighted_risky_asset	风险加权资产合计（2013）	风险加权资产合计

deposit_loan_ratio	存贷款比例	存贷款比例是指将银行的贷款总额与存款总额进行对比

short_term_asset_liquidity_ratio_CNY	短期资产流动性比例（人民币）	人民币的短期资产流动性比例

short_term_asset_liquidity_ratio_FC	短期资产流动性比例（外币）	外币的短期资产流动性比例

Nonperforming_loan_rate	不良贷款率	金融机构不良贷款占总贷款余额的比重

single_largest_customer_loan_ratio	单一最大客户贷款比例	单一最大客户贷款额占全部贷款余额的比例

top_ten_customer_loan_ratio	最大十家客户贷款比例	最大十家客户贷款额占全部贷款余额的比例

bad_debts_reserve	贷款呆账准备金	贷款呆账准备金

non_performing_loan_provision_coverage	不良贷款拨备覆盖率	不良贷款拨备覆盖率是衡量商业银行贷款损失准备金计提是否充足的一个重要指标。该项指标从宏观上反映银行贷款的风险程度及社会经济环境、诚信等方面的情况。不良贷款拨备覆盖率=贷款损失准备/(次级类资产+可疑类资产+损失类资产)*100%

cost_to_income_ratio	成本收入比	成本收入比为业务及管理费占营业收入的比例。成本收入比=业务及管理费/营业收入

former_core_capital	核心资本 (旧)	核心资本净额为核心资本减去核心资本扣减项。

former_net_core_capital	核心资本净额（旧）

former_net_core_capital_adequacy_ratio	核心资本充足率 (旧)	核心资本充足率是指核心资本与加权风险资产总额的比率

former_net_capital	资本净额 (旧)	资本净额为核心资本加上附属资本减去扣减项

former_capital_adequacy_ratio	资本充足率 (旧)	资本充足率是一个银行的资产对其风险的比率

former_weighted_risky_asset	加权风险资产净额（旧）	加权风险资产净额是指对银行的资产加以分类，根据不同类别资产的风险性质确定不同的风险系数，以这种风险系数为权重求得的资产净额。

银行贷款的五级分类指标

normal_amount	正常-金额	正常类贷款余额

normal_amount_ratio	正常金额占比	正常类贷款占贷款总额的比例。正常金额占比=正常类贷款/(正常类贷款+关注类贷款+次级类贷款+可疑类贷款+损失类贷款)*100%

concerned_amount	关注-金额	关注类贷款余额

concerned_amount_ratio	关注金额占比	关注类贷款占贷款总额的比例。关注金额占比=关注类贷款/(正常类贷款+关注类贷款+次级类贷款+可疑类贷款+损失类贷款)*100%

secondary_amount	次级-金额	次级类贷款余额

secondary_amount_ratio	次级金额占比	次级类贷款占贷款总额的比例。次级金额占比=次级类贷款/(正常类贷款+关注类贷款+次级类贷款+可疑类贷款+损失类贷款)*100%

suspicious_amount	可疑-金额	可疑类贷款余额

suspicious_amount_ratio	可疑金额占比	可疑类贷款占贷款总额的比例。可疑金额占比=可疑类贷款/(正常类贷款+关注类贷款+次级类贷款+可疑类贷款+损失类贷款)*100%

loss_amount	损失-金额	损失类贷款余额

loss_amount_ratio	损失金额占比	损失类贷款占贷款总额的比例。损失金额占比=损失类贷款/(正常类贷款+关注类贷款+次级类贷款+可疑类贷款+损失类贷款)*100%

平均贷款利率

short_term_loan_average_balance	短期贷款-平均余额	短期贷款的平均余额

short_term_loan_annualized_average_interest_rate	短期贷款-年平均利率	短期贷款的年平均利率

mid_term_loan_annualized_average_balance	中长期贷款-平均余额	中长期贷款的平均余额

mid_term_loan_annualized_average_interest_rate	中长期贷款-年平均利率	中长期贷款的年平均利率

enterprise_deposits_average_balance	企业存款-平均余额	企业存款的平均余额

enterprise_deposits_average_interest_rate	企业存款-年平均利率	企业存款的年平均利率

savings_deposit_average_balance	储蓄存款-平均余额	储蓄存款的平均余额

savings_deposit_average_interest_rate	储蓄存款-年平均利率	储蓄存款的年平均利率

示例

# 获取2019年银行业专项指标
df=get_fundamentals(query(bank_indicator),statDate=2019)[:5]
# pubDate公司发布财报的日期,statDate财报统计的季度的最后一天,total_loan贷款总额
print(df[['code','pubDate','statDate','total_loan']])

code     pubDate    statDate    total_loan
0  000001.XSHE  2020-02-14  2019-12-31  2.328909e+12
1  600036.XSHG  2020-03-21  2019-12-31  4.500199e+12
2  002948.XSHE  2020-03-21  2019-12-31  1.735679e+11
3  601860.XSHG  2020-03-25  2019-12-31  1.019562e+11
4  601658.XSHG  2020-03-26  2019-12-31  4.974186e+12
