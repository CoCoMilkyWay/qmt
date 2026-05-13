基础类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取基础类因子值

注意

为保证数据的连续性，所有数据基于后复权计算

为了防止单次返回数据时间过长，每次调用 api 请求的因子值不能超过 200000 个

参数

securities:股票池，单只股票（字符串）或一个股票列表

factors: 因子名称，单个因子（字符串）或一个因子列表

start_date:开始日期，字符串或 datetime 对象，与 coun t参数二选一

end_date: 结束日期， 字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日），与 start_date 参数二选一

返回

一个 dict： key 是因子名称， value 是 pandas.dataframe。

dataframe 的 index 是日期， column 是股票代码， value 是因子值

基础类因子

因子 code	因子名称	计算方法

net_working_capital	净运营资本	流动资产 － 流动负债

total_operating_revenue_ttm	营业总收入TTM	计算过去12个月的 营业总收入 之和

operating_profit_ttm	营业利润TTM	计算过去12个月 营业利润 之和

net_operate_cash_flow_ttm	经营活动现金流量净额TTM	计算过去12个月 经营活动产生的现金流量净值 之和

operating_revenue_ttm	营业收入TTM	计算过去12个月的 营业收入 之和

interest_carry_current_liability	带息流动负债	流动负债合计 - 无息流动负债

sale_expense_ttm	销售费用TTM	计算过去12个月 销售费用 之和

gross_profit_ttm	毛利TTM	过去12个月的 毛利润 之和

retained_earnings	留存收益	盈余公积金+未分配利润

total_operating_cost_ttm	营业总成本TTM	计算过去12个月的 营业总成本 之和

non_operating_net_profit_ttm	营业外收支净额TTM	营业外收入（TTM） - 营业外支出（TTM）

net_invest_cash_flow_ttm	投资活动现金流量净额TTM	计算过去12个月 投资活动现金流量净额 之和

financial_expense_ttm	财务费用TTM	计算过去12个月 财务费用 之和

administration_expense_ttm	管理费用TTM	计算过去12个月 管理费用 之和

net_interest_expense	净利息费用	利息支出-利息收入

value_change_profit_ttm	价值变动净收益TTM	计算过去12个月 价值变动净收益 之和

total_profit_ttm	利润总额TTM	计算过去12个月 利润总额 之和

net_finance_cash_flow_ttm	筹资活动现金流量净额TTM	计算过去12个月 筹资活动现金流量净额 之和

interest_free_current_liability	无息流动负债	应付票据+应付账款+预收账款(用 预售款项 代替)+应交税费+应付利息+其他应付款+其他流动负债

EBIT	息税前利润	净利润+所得税+财务费用

net_profit_ttm	净利润TTM	计算过去12个月 净利润 之和

OperateNetIncome	经营活动净收益	经营活动净收益/利润总额(%) * 利润总额

EBITDA	息税折旧摊销前利润（报告期）	一般企业：（营业总收入-营业税金及附加）-（营业成本+利息支出+手续费及佣金支出+销售费用+管理费用+研发费用+资产减值损失）+（固定资产折旧、油气资产折耗、生产性生物资产折旧）+无形资产摊销+长期待摊费用摊销;
银行业：（营业总收入-营业税金及附加）-（营业成本+管理费用+资产减值损失）+（固定资产折旧、油气资产折耗、生产性生物资产折旧+无形资产摊销+长期待摊费用摊销）

asset_impairment_loss_ttm	资产减值损失TTM	计算过去12个月 资产减值损失 之和

np_parent_company_owners_ttm	归属于母公司股东的净利润TTM	计算过去12个月 归属于母公司股东的净利润 之和

operating_cost_ttm	营业成本TTM	计算过去12个月的 营业成本 之和

net_debt	净债务	总债务-期末现金及现金等价物余额

non_recurring_gain_loss	非经常性损益	归属于母公司股东的净利润-扣除非经常损益后的净利润(元)

goods_sale_and_service_render_cash_ttm	销售商品提供劳务收到的现金	计算过去12个月 销售商品提供劳务收到的现金 之和

market_cap	市值	市值

cash_flow_to_price_ratio	现金流市值比	1 / pcf_ratio (ttm)

sales_to_price_ratio	营收市值比	1 / ps_ratio (ttm)

circulating_market_cap	流通市值	流通市值

operating_assets	经营性资产	总资产 - 金融资产

financial_assets	金融资产	货币资金 + 交易性金融资产 + 应收票据 + 应收利息 + 应收股利 + 可供出售金融资产 + 持有至到期投资

operating_liability	经营性负债	总负债 - 金融负债

financial_liability	金融负债	(流动负债合计-无息流动负债)+(有息非流动负债)=(流动负债合计-应付账款-预收款项-应付职工薪酬-应交税费-其他应付款-一年内的递延收益-其它流动负债)+(长期借款+应付债券)

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['net_working_capital','total_operating_revenue_ttm','operating_profit_ttm'],
start_date='2022-01-01', end_date='2022-01-10')
# 查看因子值
print(factor_data['operating_profit_ttm'])
