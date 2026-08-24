获取聚宽因子名称

# 导入函数库
from jqdatasdk import *
get_all_factors()

描述

获取聚宽因子库中所有的因子code和因子名称

详细介绍:函数计算公式、API 调用方法，输入输出值详情请见:数据字典 - 聚宽因子

【风险因子-新风格因子】目前正在调试中，未正式上线，敬请期待

参数

无

返回

pandas.DataFrame

factor:因子code

factor_intro:因子说明

category:因子分类名称

category_intro:因子分类说明

示例：

from jqdatasdk import *
#获取聚宽因子库所有因子
df = get_all_factors()
print(df)

factor factor_intro   category category_intro
0    administration_expense_ttm      管理费用TTM     basics     基础科目及衍生类因子
1     asset_impairment_loss_ttm    资产减值损失TTM     basics     基础科目及衍生类因子
2      cash_flow_to_price_ratio       现金流市值比     basics     基础科目及衍生类因子
3        circulating_market_cap         流通市值     basics     基础科目及衍生类因子
4                          EBIT        息税前利润     basics     基础科目及衍生类因子
..                          ...          ...        ...            ...
255                        MAC5       5日移动均线  technical         技术指标因子
256                       MAC60      60日移动均线  technical         技术指标因子
257                       MACDC    平滑异同移动平均线  technical         技术指标因子
258                       MFI14       资金流量指标  technical         技术指标因子
259                 price_no_fq      不复权价格因子  technical         技术指标因子

[260 rows x 4 columns]
