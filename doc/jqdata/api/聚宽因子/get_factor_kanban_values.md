获取因子看板列表数据

get_factor_kanban_values(universe='hs300',bt_cycle='month_3',model='long_only',category=['quality','basics','emotion','growth','risk','pershare'],skip_paused=False,commision_slippage=0)

参数

universe：股票池

        'hs300': 沪深300

        'zz500': 中证500

        'zz800': 中证800

        'zz1000': 中证1000

        'zzqz': 中证全指

bt_cycle：测试周期

        'month_3'：近三个月

        'year_1'：近一年

        'year_3'：近三年

        year_10'：近十年

model：组合构建模型

        'long_only'：纯多头组合

        'long_short'：多空组合

category：分类，具体见聚宽因子库

        'quality': 质量类

        'basics': 基础类

        'emotion': 情绪类

         'growth': 成长类

        'risk': 风险类

        'pershare': 每股类

         'style': 风险因子 - 风格因子

        'technical': 技术类

        'momentum': 动量类

kip_paused: 过滤涨停及停牌股

        False: 否

        True: 是

commision_slippage: 手续费及滑点

        0: 无

        1: 3‱佣金+1‰印花税+无滑点

        2: 3‱佣金+1‰印花税+1‰滑点

返回

pandas.DataFrame 针对 [model - 组合构建模型]选择的不同，返回的结构有所差异

一、long_only - 纯多头组合

index：自然增长的数字，从0开始，无意义

column：

        date： 因子的收益需要下一交易日才可得到 ,因此实际数据可获取的时间比date晚一个交易日(T日收盘后的因子收益需要T+1的收盘价才可得出，数据需要在T+2日凌晨3点计算之后才可得到)

        universe: 股票池

        bt_cycle: 测试周期

        skip_paused: 过滤涨停及停牌股

        commision_slippage: 手续费及滑点

        category: 因子分类

        code：因子代码

        compound_return_1q：一分位数累积收益

        compound_return_5q：五分位数累积收益

         annualized_return_1q：一分位数年化收益率

        annualized_return_5q：五分位数年化收益率

        max_drawdown_1q：一分位数最大回撤

        max_drawdown_5q：五分位数最大回撤

        sharpe_1q：一分位数夏普比率

        sharpe_5q：五分位数夏普比率

        turnover_ratio_1q：一分位数换手率

        turnover_ratio_5q：五分位数换手率

        annual_return_bm：基准指数年化收益率

        ic_mean：IC均值

        ir：IR值

        good_ic：IC绝对值大于0.02的比率

二、long_short - 多空组合

index：自然增长的数字，从0开始，无意义

column：

         date：数据的更新日期，因子的收益需要下一交易日才可得到 ,因此实际数据的时间比date晚一天(T日收盘后的因子收益需要T+1的收盘价才可得出，数据需要在T+2日凌晨3点计算之后才可得到)

         universe: 股票池

         bt_cycle: 测试周期

         skip_paused: 过滤涨停及停牌股

         commision_slippage: 手续费及滑点

         category: 因子分类

         code：因子代码

         compound_return_ls：累积收益

         annualized_return_ls：年化收益率

         max_drawdown_ls：最大回撤

         sharpe_ls：夏普比率

         turnover_ratio_ls：换手率

         annual_return_bm：基准指数年化收益率

         ic_mean：IC均值

         ir：IR值

         good_ic：IC绝对值大于0.02的比率

示例

from jqdatasdk import *
df = get_factor_kanban_values(universe='hs300',bt_cycle='month_3',model='long_only',category=['quality','basics','emotion','growth','risk','pershare'],skip_paused=False,commision_slippage=0)
print(df.head(5))

>>>         date category    ...           ir   good_ic
0 2022-09-22   basics    ...    -0.174076  0.952381
1 2022-09-22   basics    ...    -0.114981  0.873016
2 2022-09-22   basics    ...    -0.052311  0.841270
3 2022-09-22   basics    ...    -0.041307  0.841270
4 2022-09-22   basics    ...    -0.015338  0.841270

[5 rows x 21 columns]
