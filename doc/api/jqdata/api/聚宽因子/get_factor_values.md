质量类因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

描述

获取质量类因子值

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

因子 code	因子名称	计算方法

net_profit_to_total_operate_revenue_ttm	净利润与营业总收入之比	净利润与营业总收入之比=净利润（TTM）/营业总收入（TTM）

cfo_to_ev	经营活动产生的现金流量净额与企业价值之比TTM	经营活动产生的现金流量净额TTM / 企业价值。其中，企业价值=司市值+负债合计-货币资金

accounts_payable_turnover_days	应付账款周转天数	应付账款周转天数 = 360 / 应付账款周转率

net_profit_ratio	销售净利率	售净利率=净利润（TTM）/营业收入（TTM）

net_non_operating_income_to_total_profit	营业外收支利润净额/利润总额	营业外收支利润净额/利润总额

fixed_asset_ratio	固定资产比率	固定资产比率=(固定资产+工程物资+在建工程)/总资产

account_receivable_turnover_days	应收账款周转天数	应收账款周转天数=360/应收账款周转率

DEGM	毛利率增长	毛利率增长=(今年毛利率（TTM）/去年毛利率（TTM）)-1

sale_expense_to_operating_revenue	营业费用与营业总收入之比	营业费用与营业总收入之比=销售费用（TTM）/营业总收入（TTM）

operating_tax_to_operating_revenue_ratio_ttm	销售税金率	销售税金率=营业税金及附加（TTM）/营业收入（TTM）

inventory_turnover_days	存货周转天数	存货周转天数=360/存货周转率

OperatingCycle	营业周期	应收账款周转天数+存货周转天数

net_operate_cash_flow_to_operate_income	经营活动产生的现金流量净额与经营活动净收益之比	经营活动产生的现金流量净额（TTM）/(营业总收入（TTM）-营业总成本（TTM））

net_operating_cash_flow_coverage	净利润现金含量	经营活动产生的现金流量净额/归属于母公司所有者的净利润

quick_ratio	速动比率	速动比率=(流动资产合计-存货)/ 流动负债合计

intangible_asset_ratio	无形资产比率	无形资产比率=(无形资产+研发支出+商誉)/总资产

MLEV	市场杠杆	市场杠杆=非流动负债合计/(非流动负债合计+总市值)

debt_to_equity_ratio	产权比率	产权比率=负债合计/归属母公司所有者权益合计

super_quick_ratio	超速动比率	（货币资金+交易性金融资产+应收票据+应收帐款+其他应收款）／流动负债合计

inventory_turnover_rate	存货周转率	存货周转率=营业成本（TTM）/存货

operating_profit_growth_rate	营业利润增长率	营业利润增长率=(今年营业利润（TTM）/去年营业利润（TTM）)-1

long_debt_to_working_capital_ratio	长期负债与营运资金比率	长期负债与营运资金比率=非流动负债合计/(流动资产合计-流动负债合计)

current_ratio	流动比率(单季度)	流动比率=流动资产合计/流动负债合计

net_operate_cash_flow_to_net_debt	经营活动产生现金流量净额/净债务	经营活动产生现金流量净额/净债务

net_operate_cash_flow_to_asset	总资产现金回收率	经营活动产生的现金流量净额(ttm) / 总资产

non_current_asset_ratio	非流动资产比率	非流动资产比率=非流动资产合计/总资产

total_asset_turnover_rate	总资产周转率	总资产周转率=营业收入(ttm)/总资产

long_debt_to_asset_ratio	长期借款与资产总计之比	长期借款与资产总计之比=长期借款/总资产

debt_to_tangible_equity_ratio	有形净值债务率	负债合计/有形净值  其中有形净值=股东权益-无形资产净值，无形资产净值= 商誉+无形资产

ROAEBITTTM	总资产报酬率	（利润总额（TTM）+利息支出（TTM）） / 总资产在过去12个月的平均

operating_profit_ratio	营业利润率	营业利润率=营业利润（TTM）/营业收入（TTM）

long_term_debt_to_asset_ratio	长期负债与资产总计之比	长期负债与资产总计之比=非流动负债合计/总资产

current_asset_turnover_rate	流动资产周转率TTM	过去12个月的营业收入/过去12个月的平均流动资产合计

financial_expense_rate	财务费用与营业总收入之比	财务费用（TTM） / 营业总收入（TTM）

operating_profit_to_total_profit	经营活动净收益/利润总额	经营活动净收益/利润总额

debt_to_asset_ratio	债务总资产比	债务总资产比=负债合计/总资产

equity_to_fixed_asset_ratio	股东权益与固定资产比率	股东权益与固定资产比率=股东权益/(固定资产+工程物资+在建工程)

net_operate_cash_flow_to_total_liability	经营活动产生的现金流量净额/负债合计	经营活动产生的现金流量净额/负债合计

cash_rate_of_sales	经营活动产生的现金流量净额与营业收入之比	经营活动产生的现金流量净额（TTM） / 营业收入（TTM）

operating_profit_to_operating_revenue	营业利润与营业总收入之比	营业利润与营业总收入之比=营业利润（TTM）/营业总收入（TTM）

roa_ttm	资产回报率TTM	资产回报率=净利润（TTM）/期末总资产

admin_expense_rate	管理费用与营业总收入之比	管理费用与营业总收入之比=管理费用（TTM）/营业总收入（TTM）

fixed_assets_turnover_rate	固定资产周转率	等于过去12个月的营业收入/过去12个月的平均（固定资产+工程物资+在建工程）

invest_income_associates_to_total_profit	对联营和合营公司投资收益/利润总额	对联营和营公司投资收益/利润总额

equity_to_asset_ratio	股东权益比率	股东权益比率=股东权益/总资产

goods_service_cash_to_operating_revenue_ttm	销售商品提供劳务收到的现金与营业收入之比	销售商品提供劳务收到的现金与营业收入之比=销售商品和提供劳务收到的现金（TTM）/营业收入（TTM）

cash_to_current_liability	现金比率	期末现金及现金等价物余额/流动负债合计的12个月均值

net_operate_cash_flow_to_total_current_liability	现金流动负债比	现金流动负债比=经营活动产生的现金流量净额（TTM）/流动负债合计

ACCA	现金流资产比和资产回报率之差	现金流资产比-资产回报率,其中现金流资产比=经营活动产生的现金流量净额/总资产

roe_ttm	权益回报率TTM	权益回报率=净利润（TTM）/期末股东权益

accounts_payable_turnover_rate	应付账款周转率	TTM(营业成本,0)/（AvgQ(应付账款,4,0) + AvgQ(应付票据,4,0) + AvgQ(预付款项,4,0) ）

gross_income_ratio	销售毛利率	销售毛利率=(营业收入（TTM）-营业成本（TTM）)/营业收入（TTM）

adjusted_profit_to_total_profit	扣除非经常损益后的净利润/利润总额	扣除非经常损益后的净利润/利润总额

account_receivable_turnover_rate	应收账款周转率	即，TTM(营业收入,0)/（AvgQ(应收账款,4,0) + AvgQ(应收票据,4,0) + AvgQ(预收账款,4,0) ）

equity_turnover_rate	股东权益周转率	股东权益周转率=营业收入(ttm)/股东权益

total_profit_to_cost_ratio	成本费用利润率	成本费用利润率=利润总额/(营业成本+财务费用+销售费用+管理费用)，以上科目使用的都是TTM的数值

operating_cost_to_operating_revenue_ratio	销售成本率	销售成本率=营业成本（TTM）/营业收入（TTM）

LVGI	财务杠杆指数	本期(年报)资产负债率/上期(年报)资产负债率

SGI	营业收入指数	本期(年报)营业收入/上期(年报)营业收入

GMI	毛利率指数	上期(年报)毛利率/本期(年报)毛利率

DSRI	应收账款指数	本期(年报)应收账款占营业收入比例/上期(年报)应收账款占营业收入比例

rnoa_ttm	经营资产回报率TTM	销售利润率*经营资产周转率

profit_margin_ttm	销售利润率TTM	营业利润/营业收入

roe_ttm_8y	长期权益回报率TTM	8年(1+roe_ttm)的累乘 ^ (1/8) - 1  # 至少要有近4年的数据，否则为 nan

asset_turnover_ttm	经营资产周转率TTM	营业收入TTM/近4个季度期末净经营性资产均值; 净经营性资产=经营资产-经营负债

roic_ttm	投资资本回报率TTM	权益回报率=归属于母公司股东的净利润（TTM）/ 前四个季度投资资本均值; 投资资本=股东权益+负债合计-无息流动负债-无息非流动负债;             无息流动负债=应付账款+预收款项+应付职工薪酬+应交税费+其他应付款+一年内的递延收益+其它流动负债;             无息非流动负债=非流动负债合计-长期借款-应付债券；

roa_ttm_8y	长期资产回报率TTM	8年(1+roa_ttm)的乘积 ^ (1/8) - 1  # 至少要有近4年的数据，否则为 nan

SGAI	销售管理费用指数	本期(年报)销售管理费用占营业收入的比例/上期(年报)销售管理费用占营业收入的比例

DEGM_8y	长期毛利率增长	过去8年(1+DEGM)的累成 ^ (1/8) - 1

maximum_margin	最大盈利水平	max(margin_stability, DEGM_8y)

margin_stability	盈利能力稳定性	mean(GM)/std(GM); GM 为过去8年毛利率ttm

示例

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['net_profit_to_total_operate_revenue_ttm','cfo_to_ev','net_profit_ratio'],
start_date='2022-01-01', end_date='2022-01-10')
# 查看因子值
print(factor_data['net_profit_ratio'])
