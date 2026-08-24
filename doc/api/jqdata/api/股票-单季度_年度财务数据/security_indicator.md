security_indicator券商

历史范围：2005年至今；更新时间：交易日24:00更新

get_fundamentals(query_object, date=None, statDate=None)

描述

查询security_indicator券商

按年度更新，统计周期是一年度。 通过get_fundamentals(query_object,statDate=None) statDate传入年查询。当传入 date 参数 或 statDate 传入季度时返回空。

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

security_indicator券商

列名	列的含义	解释

code	股票代码	带后缀.XSHE/.XSHG

pubDate	日期	公司发布财报日期

statDate	日期	财报统计的季度的最后一天, 比如2015-03-31, 2015-06-30

net_capital	净资本	净资本是衡量证券公司资本充足和资产流动性状况的一个综合性监管指标，是证券公司净资产中流动性较高、可快速变现的部分，它表明证券公司可随时用于变现以满足支付需要的资金数额。为公布的母公司净资本及相关风险控制指标之一。

net_assets	净资产	为公布的母公司净资本及相关风险控制指标之一。

net_capital_to_reserve	净资本/各项风险准备之和	为公布的母公司净资本及相关风险控制指标之一。

net_capital_to_net_asset	净资本/净资产	净资本/净资产

net_capital_to_debt	净资本/负债	净资本/负债

net_asset_to_debt	净资产/负债	净资产/负债

net_capital_to_sales_department_number	净资本/营业部家数	净资本/营业部家数

own_stock_to_net_capital	自营股票规模/净资本	自营股票规模/净资本

own_security_to_net_capital	证券自营业务规模/净资本	证券自营业务规模/净资本

operational_risk_reserve	营运风险堆备	营运风险堆备

broker_risk_reserve	经纪业务风险堆备	经纪业务风险堆备

own_security_risk_reserve	证券自营业务风险准备	证券自营业务风险准备

security_underwriting_reserve	证券承消业务风险准备	证券承消业务风险准备

asset_management_reserve	证券资产菅理业务风险准备	证券资产菅理业务风险准备

own_equity_derivatives_to_net_capital	自营权益类证券及证券衍生品/净资本	自营权益类证券及证券衍生品/净资本

own_fixed_income_to_net_capital	自营固定收益类证券/净资本	自营固定收益类证券/净资本

margin_trading_reserve	融资融券业务风险资本准备	融资融券业务风险资本准备

branch_risk_reserve	分支机构风险资本堆备	分支机构风险资本堆备

示例

# 获取券商业专项指标
df=get_fundamentals(query(security_indicator),statDate=2019)[:5]
# pubDate公司发布财报的日期,statDate财报统计的季度的最后一天,net_capital净资本
print(df[['code','pubDate','statDate','net_capital']])

code     pubDate    statDate   net_capital
0  000987.XSHE  2020-02-29  2019-12-31  8.279037e+09
1  601878.XSHG  2020-03-19  2019-12-31  1.251134e+10
2  600030.XSHG  2020-03-20  2019-12-31  9.490422e+10
3  002736.XSHE  2020-03-20  2019-12-31  4.005491e+10
4  601211.XSHG  2020-03-25  2019-12-31  8.597149e+10
