insurance_indicator保险

历史范围：2005年至今；更新时间：交易日24:00更新

get_fundamentals(query_object, date=None, statDate=None)

描述

查询insurance_indicator保险

按年度更新，统计周期是一年度。 通过get_fundamentals(query_object,statDate=None) statDate传入年查询。当传入 date 参数 或 statDate 传入季度时返回空。

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

insurance_indicator保险

列名	列的含义	解释

code	股票代码	带后缀.XSHE/.XSHG

pubDate	日期	公司发布财报日期

statDate	日期	财报统计的季度的最后一天, 比如2016-12-31

investment_assets	投资资产	投资资产

total_investment_rate_of_return	总投资收益率	总投资收益率

net_investment_rate_of_return	净投资收益率	净投资收益率

earned_premium	己赚保费	己赚保费

earned_premium_growth_rate	己赚保费增长率	己赚保费增长率

payoff_cost	赔付支出	赔付支出

compensation_rate	退保率(寿险业务)	寿险业务退保率

not_expired_duty_reserve	未到期责任准备金（产险业务）	产险业务未到期责任准备金

outstanding_claims_reserve	未决赔款准备金（产险业务）	产险业务未决赔款准备金

comprehensive_cost_ratio	综台成本率（产险业务）	产险业务综台成本率

comprehensive_compensation_rate	综台赔付率（产险业务）	产险业务综台赔付率

solvency_adequacy_ratio	偿付能力充足率	偿付能力充足率

actual_capital	实际资本	实际资本

minimum_capital	最低资本	最低资本

示例

# 获取保险业专项指标
df=get_fundamentals(query(insurance_indicator),statDate=2019).tail(5)
# pubDate公司发布财报的日期,statDate财报统计的季度的最后一天,net_capital净资本
df[['code','pubDate','statDate','total_investment_rate_of_return']]

code     pubDate    statDate  total_investment_rate_of_return
1  601601.XSHG  2020-03-23  2019-12-31                             5.40
2  601628.XSHG  2020-03-26  2019-12-31                             5.23
3  601336.XSHG  2020-03-26  2019-12-31                             4.90
4  000627.XSHE  2020-04-15  2019-12-31                              NaN
5  600291.XSHG  2020-04-30  2019-12-31                            -2.18
